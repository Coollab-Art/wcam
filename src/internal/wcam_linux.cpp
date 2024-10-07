#if defined(__linux__)
#include "wcam_linux.hpp"
#include <fcntl.h>
#include <jpeglib.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <thread>
#include <vector>
#include "../Info.hpp"
#include "ICaptureImpl.hpp"
#include "ImageFactory.hpp"
#include "make_device_id.hpp"

namespace wcam::internal {

void mjpeg_to_rgb(unsigned char* mjpeg_data, unsigned long mjpeg_size, unsigned char* rgb_data)
{
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr         jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);

    // Set memory buffer as the input source for JPEG decompression
    jpeg_mem_src(&cinfo, mjpeg_data, mjpeg_size);

    // Read JPEG header
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    // Allocate memory for RGB data
    int row_stride = cinfo.output_width * cinfo.output_components;
    // *rgb_data      = (unsigned char*)malloc(cinfo.output_width * cinfo.output_height * cinfo.output_components);

    // Decompress each scanline and store in the RGB buffer
    while (cinfo.output_scanline < cinfo.output_height)
    {
        unsigned char* buffer_array[1];
        buffer_array[0] = rgb_data + (cinfo.output_scanline) * row_stride;
        jpeg_read_scanlines(&cinfo, buffer_array, 1);
    }

    // Finish decompression
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
}

void yuyv_to_rgb(unsigned char* yuyv, unsigned char* rgb, int width, int height)
{
    for (int i = 0; i < width * height * 2; i += 4)
    {
        int y0 = yuyv[i + 0] << 8;
        int u  = yuyv[i + 1] - 128;
        int y1 = yuyv[i + 2] << 8;
        int v  = yuyv[i + 3] - 128;

        int r0 = (y0 + 359 * v) >> 8;
        int g0 = (y0 - 88 * u - 183 * v) >> 8;
        int b0 = (y0 + 454 * u) >> 8;
        int r1 = (y1 + 359 * v) >> 8;
        int g1 = (y1 - 88 * u - 183 * v) >> 8;
        int b1 = (y1 + 454 * u) >> 8;

        rgb[i * 3 / 2 + 0] = std::clamp(r0, 0, 255);
        rgb[i * 3 / 2 + 1] = std::clamp(g0, 0, 255);
        rgb[i * 3 / 2 + 2] = std::clamp(b0, 0, 255);
        rgb[i * 3 / 2 + 3] = std::clamp(r1, 0, 255);
        rgb[i * 3 / 2 + 4] = std::clamp(g1, 0, 255);
        rgb[i * 3 / 2 + 5] = std::clamp(b1, 0, 255);
    }
}

static auto find_available_resolutions(int const video_device) -> std::vector<Resolution>
{
    std::vector<Resolution> available_resolutions;

    v4l2_fmtdesc format_description{};
    format_description.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    while (ioctl(video_device, VIDIOC_ENUM_FMT, &format_description) == 0)
    {
        v4l2_frmsizeenum frame_size{};
        frame_size.pixel_format = format_description.pixelformat;
        while (ioctl(video_device, VIDIOC_ENUM_FRAMESIZES, &frame_size) == 0)
        {
            v4l2_frmivalenum frame_interval{};
            frame_interval.pixel_format = format_description.pixelformat;
            frame_interval.width        = frame_size.discrete.width;
            frame_interval.height       = frame_size.discrete.height;

            while (ioctl(video_device, VIDIOC_ENUM_FRAMEINTERVALS, &frame_interval) == 0)
            {
                if (frame_interval.type == V4L2_FRMIVAL_TYPE_DISCRETE)
                {
                    // float fps = static_cast<float>(frameInterval.discrete.denominator) / static_cast<float>(frameInterval.discrete.numerator);
                    if (/*fps > 29. &&*/ frame_size.type == V4L2_FRMSIZE_TYPE_DISCRETE)
                    {
                        available_resolutions.push_back({static_cast<Resolution::DataType>(frame_interval.width), static_cast<Resolution::DataType>(frame_interval.height)});
                    }
                }
                frame_interval.index++;
            }
            frame_size.index++;
        }

        format_description.index++;
    }

    return available_resolutions;
}

class CloseFileAtExit {
public:
    explicit CloseFileAtExit(int file_descriptor)
        : _file_descriptor{file_descriptor}
    {}

    ~CloseFileAtExit()
    {
        close(_file_descriptor);
    }

private:
    int _file_descriptor{};
};

auto grab_all_infos_impl() -> std::vector<Info>
{
    std::vector<Info> list_webcam_info{};

    for (auto const& entry : std::filesystem::directory_iterator("/dev"))
    {
        if (entry.path().string().find("video") == std::string::npos)
            continue;

        int const video_device = open(entry.path().c_str(), O_RDONLY);
        if (video_device == -1)
            continue;
        auto const scope_guard = CloseFileAtExit{video_device};

        v4l2_capability cap{};
        if (ioctl(video_device, VIDIOC_QUERYCAP, &cap) == -1)
            continue;

        if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
            continue;

        std::string const webcam_name = reinterpret_cast<char const*>(cap.card); // NOLINT(*-pro-type-reinterpret-cast)

        std::vector<Resolution> const available_resolutions = find_available_resolutions(video_device);
        if (available_resolutions.empty())
            continue;

        list_webcam_info.push_back({webcam_name, make_device_id(entry.path()), available_resolutions});
    }

    return list_webcam_info;
}

static auto is_supported_format(uint32_t format) -> bool
{
    return format == V4L2_PIX_FMT_MJPEG
           || format == V4L2_PIX_FMT_YUYV;
}

auto select_pixel_format(int fd, int width, int height) -> uint32_t
{
    struct v4l2_fmtdesc     fmt;
    struct v4l2_frmsizeenum frmsize;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    // Enumerate pixel formats
    for (fmt.index = 0; ioctl(fd, VIDIOC_ENUM_FMT, &fmt) == 0; fmt.index++)
    {
        memset(&frmsize, 0, sizeof(frmsize));
        frmsize.pixel_format = fmt.pixelformat;

        // Enumerate frame sizes for this pixel format
        for (frmsize.index = 0; ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) == 0; frmsize.index++)
        {
            if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE)
            {
                if (frmsize.discrete.width == width && frmsize.discrete.height == height && is_supported_format(fmt.pixelformat))
                {
                    return fmt.pixelformat;
                }
            }
            // else if (frmsize.type == V4L2_FRMSIZE_TYPE_STEPWISE)
            // {
            //     if (frmsize.stepwise.min_width <= width && frmsize.stepwise.max_width >= width && frmsize.stepwise.min_height <= height && frmsize.stepwise.max_height >= height)
            //     {
            //         printf("  - Pixel format: %s (0x%08x)\n", fmt.description, fmt.pixelformat);
            //     }
            // }
        }
    }
    throw CaptureException{Error_Unknown{"Unsupported pixel format"}};
}

Bob::Bob(DeviceId const& id, Resolution const& resolution)
    : _resolution{resolution}
{
    fd = open(id.as_string().c_str(), O_RDWR);
    if (fd == -1)
    {
        perror("Failed to open device");
        exit(EXIT_FAILURE);
    }
    _pixels_format = select_pixel_format(fd, resolution.width(), resolution.height());

    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = _resolution.width();
    fmt.fmt.pix.height      = _resolution.height();
    fmt.fmt.pix.pixelformat = _pixels_format;
    fmt.fmt.pix.field       = V4L2_FIELD_NONE;

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) == -1)
    {
        // perror("Failed to set format");
        throw CaptureException{Error_WebcamAlreadyUsedInAnotherApplication{}}; // TODO cleaner
    }

    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count  = 1;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd, VIDIOC_REQBUFS, &req) == -1)
    {
        perror("Failed to request buffers");
    }

    buffers.resize(req.count);
    for (size_t i = 0; i < req.count; ++i)
    {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;

        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) == -1)
        {
            perror("Failed to query buffer");
        }

        buffers[i].length = buf.length;
        buffers[i].start  = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
        if (buffers[i].start == MAP_FAILED)
        {
            perror("Failed to map buffer");
        }
    }
    for (size_t i = 0; i < buffers.size(); ++i)
    {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;

        if (ioctl(fd, VIDIOC_QBUF, &buf) == -1)
        {
            perror("Failed to queue buffer");
        }
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) == -1)
    {
        perror("Failed to start capture");
    }
}

Bob::~Bob()
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMOFF, &type) == -1)
    {
        perror("Failed to stop capture");
    }
    for (size_t i = 0; i < buffers.size(); ++i)
    {
        if (munmap(buffers[i].start, buffers[i].length) == -1)
        {
            perror("Failed to unmap buffer");
        }
    }
    if (close(fd) == -1)
    {
        perror("Failed to close device");
    }
}

CaptureImpl::CaptureImpl(DeviceId const& id, Resolution const& resolution)
    : _bob{id, resolution}
    , _thread{&CaptureImpl::thread_job, std::ref(*this)}
{}

CaptureImpl::~CaptureImpl()
{
    _wants_to_stop_thread.store(true);
    _thread.join();
}

void CaptureImpl::thread_job(CaptureImpl& This)
{
    while (!This._wants_to_stop_thread.load())
    {
        std::shared_ptr<uint8_t> data  = This._bob.getFrame(); // Blocks until a new image is received
        auto                     image = image_factory().make_image();
        image->set_data(ImageDataView<RGB24>{std::move(data), static_cast<size_t>(This._bob._resolution.pixels_count() * 3), This._bob._resolution, wcam::FirstRowIs::Top});
        This.set_image(std::move(image));

        // timeval timeout;
        // if (This.videoCapture->isReadable(&timeout))
        // {
        //     auto     image  = image_factory().make_image();
        //     char*    buffer = new char[This._resolution.pixels_count() * 3];
        //     size_t   nb     = This.videoCapture->read(buffer, This._resolution.pixels_count() * 3);
        //     uint8_t* rgb_data; // new uint8_t[This._resolution.pixels_count() * 3];
        //     int      w, h;
        //     mjpeg_to_rgb((unsigned char*)buffer, This._resolution.pixels_count() * 3, &rgb_data, &w, &h);
        //     image->set_data(ImageDataView<RGB24>{rgb_data, static_cast<size_t>(This._resolution.pixels_count() * 3), This._resolution, wcam::FirstRowIs::Top});
        //     This.set_image(std::move(image));
        // }
    }
}

auto Bob::getFrame() -> std::shared_ptr<uint8_t>
{
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));

    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd, VIDIOC_DQBUF, &buf) == -1) // Blocks until a new frame is available
    {
        perror("Failed to dequeue buffer");
        // return false;
    }
    uint8_t* rgb_data = new uint8_t[_resolution.pixels_count() * 3];
    if (_pixels_format == V4L2_PIX_FMT_YUYV)
        yuyv_to_rgb((unsigned char*)buffers[0].start, rgb_data, _resolution.width(), _resolution.height());
    else if (_pixels_format == V4L2_PIX_FMT_MJPEG)
        mjpeg_to_rgb((unsigned char*)buffers[0].start, buffers[0].length, rgb_data);
    else
    {
        assert(false);
    };
    if (ioctl(fd, VIDIOC_QBUF, &buf) == -1)
    {
        perror("Failed to queue buffer");
    }
    return std::shared_ptr<uint8_t>{rgb_data};
}

} // namespace wcam::internal
#endif