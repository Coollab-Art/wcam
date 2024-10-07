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
#include <exception>
#include <filesystem>
#include <format>
#include <functional>
#include <iostream>
#include <source_location>
#include <thread>
#include <vector>
#include "../Info.hpp"
#include "ICaptureImpl.hpp"
#include "ImageFactory.hpp"
#include "make_device_id.hpp"

namespace wcam::internal {

void mjpeg_to_rgb(unsigned char* mjpeg_data, unsigned long mjpeg_size, unsigned char* rgb_data)
{
    struct jpeg_decompress_struct cinfo; // NOLINT(*member-init)
    struct jpeg_error_mgr         jerr;  // NOLINT(*member-init)

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
std::string getErrorMessage()
{
    return std::string(strerror(errno));
}

static void throw_error(std::string const& err, std::string_view code_that_failed, std::source_location location = std::source_location::current())
{
    if (errno == 16)
        throw CaptureException{Error_WebcamAlreadyUsedInAnotherApplication{}};
    else
        throw CaptureException{Error_Unknown{std::format("{}\n(During `{}`, at {}({}:{}))", err, code_that_failed, location.file_name(), location.line(), location.column())}};
}

#define THROW_IF_ERR(exp) /*NOLINT(*macro*)*/     \
    {                                             \
        int const err_code = exp;                 \
        if (err_code == -1)                       \
            throw_error(getErrorMessage(), #exp); \
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

static auto for_each_webcam_id_path(std::function<void(const char* webcam_id_path)> const& callback)
{
    try
    {
        for (auto const& entry : std::filesystem::directory_iterator("/dev/v4l/by-id"))
        {
            callback(entry.path().string().c_str());
        }
    }
    catch (std::exception const&)
    {
    }
}

static auto get_webcam_name(int video_device) -> std::string
{
    v4l2_capability cap{};
    if (ioctl(video_device, VIDIOC_QUERYCAP, &cap) == -1)
        return "";

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
        return "";

    return reinterpret_cast<char const*>(cap.card); // NOLINT(*-pro-type-reinterpret-cast)
}

static auto get_webcam_id(const char* webcam_id_path) -> DeviceId
{
    return make_device_id(std::filesystem::path{webcam_id_path}.filename());
}

auto grab_all_infos_impl() -> std::vector<Info>
{
    std::vector<Info> list_webcam_info{};

    for_each_webcam_id_path([&](const char* webcam_id_path) {
        int const video_device = open(webcam_id_path, O_RDONLY);
        if (video_device == -1)
            return;
        auto const scope_guard = CloseFileAtExit{video_device};

        std::vector<Resolution> const available_resolutions = find_available_resolutions(video_device);
        if (available_resolutions.empty())
            return;

        list_webcam_info.push_back({get_webcam_name(video_device), get_webcam_id(webcam_id_path), available_resolutions});
    });

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
    throw CaptureException{Error_Unknown{"Unsupported pixel format"}}; // TODO list the supported formats
}

static auto find_webcam_path(DeviceId const& id) -> std::string
{
    return "/dev/v4l/by-id/" + id.as_string();
    // std::string path{};
    // for_each_webcam_id_path([&](const char* webcam_id_path) {
    // int const video_device = open(webcam_id_path, O_RDONLY);
    // if (video_device == -1)
    //     return;
    // auto const scope_guard = CloseFileAtExit{video_device};
    //     if (get_webcam_id(webcam_id_path) == id)
    //         path = webcam_id_path;
    // });
    // if (path.empty())
    //     throw CaptureException{Error_WebcamUnplugged{}};
    // std::cout << path << '\n';
    // return path;
}

CaptureImpl::CaptureImpl(DeviceId const& id, Resolution const& resolution)
    : _webcam_handle{open(find_webcam_path(id).c_str(), O_RDWR)}
    , _resolution{resolution}
{
    if (_webcam_handle == -1)
        throw CaptureException{Error_WebcamUnplugged{}};
    _pixels_format = select_pixel_format(_webcam_handle, resolution.width(), resolution.height());

    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = _resolution.width();
    fmt.fmt.pix.height      = _resolution.height();
    fmt.fmt.pix.pixelformat = _pixels_format;
    fmt.fmt.pix.field       = V4L2_FIELD_NONE;

    THROW_IF_ERR(ioctl(_webcam_handle, VIDIOC_S_FMT, &fmt));

    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count  = 6;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    THROW_IF_ERR(ioctl(_webcam_handle, VIDIOC_REQBUFS, &req));

    buffers.resize(req.count);
    for (size_t i = 0; i < req.count; ++i)
    {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;

        THROW_IF_ERR(ioctl(_webcam_handle, VIDIOC_QUERYBUF, &buf));

        buffers[i].size = buf.length;
        buffers[i].ptr  = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, _webcam_handle, buf.m.offset);
        if (buffers[i].ptr == MAP_FAILED)
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

        THROW_IF_ERR(ioctl(_webcam_handle, VIDIOC_QBUF, &buf));
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    THROW_IF_ERR(ioctl(_webcam_handle, VIDIOC_STREAMON, &type));

    _thread = std::thread{&CaptureImpl::thread_job, std::ref(*this)};
}

CaptureImpl::~CaptureImpl()
{
    _wants_to_stop_thread.store(true);
    _thread.join();

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(_webcam_handle, VIDIOC_STREAMOFF, &type) == -1)
    {
        perror("Failed to stop capture");
    }
    for (size_t i = 0; i < buffers.size(); ++i)
    {
        if (munmap(buffers[i].ptr, buffers[i].size) == -1)
        {
            perror("Failed to unmap buffer");
        }
    }
    if (close(_webcam_handle) == -1)
    {
        perror("Failed to close device");
    }
}

void CaptureImpl::thread_job(CaptureImpl& This)
{
    while (!This._wants_to_stop_thread.load())
        This.process_next_image();
}

void CaptureImpl::process_next_image()
{
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));

    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (ioctl(_webcam_handle, VIDIOC_DQBUF, &buf) == -1) // Blocks until a new frame is available
    {
        perror("Failed to dequeue buffer");
        // return false;
    }
    auto image = image_factory().make_image();

    if (_pixels_format == V4L2_PIX_FMT_YUYV)
        image->set_data(ImageDataView<YUYV>{(unsigned char*)buffers[buf.index].ptr, buffers[buf.index].size, _resolution, wcam::FirstRowIs::Top});
    // yuyv_to_rgb(, rgb_data, _resolution.width(), _resolution.height());
    else if (_pixels_format == V4L2_PIX_FMT_MJPEG)
    {
        uint8_t* rgb_data = new uint8_t[_resolution.pixels_count() * 3];
        mjpeg_to_rgb((unsigned char*)buffers[buf.index].ptr, buffers[buf.index].size, rgb_data);
        image->set_data(ImageDataView<RGB24>{std::shared_ptr<uint8_t>{rgb_data}, _resolution.pixels_count() * 3, _resolution, wcam::FirstRowIs::Top});
    }
    else
    {
        assert(false);
    };
    set_image(std::move(image));
    if (ioctl(_webcam_handle, VIDIOC_QBUF, &buf) == -1)
    {
        perror("Failed to queue buffer");
    }
}

} // namespace wcam::internal
#endif