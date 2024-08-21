#if defined(__linux__)
#include "wcam_linux.hpp"
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>
#include "../Info.hpp"
#include "ICaptureImpl.hpp"
#include "make_device_id.hpp"

namespace wcam::internal {

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

static auto find_available_resolutions(int const video_device) -> std::vector<img::Size>
{
    std::vector<img::Size> available_resolutions;

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
                        available_resolutions.push_back({static_cast<img::Size::DataType>(frame_interval.width), static_cast<img::Size::DataType>(frame_interval.height)});
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

        std::vector<img::Size> const available_resolutions = find_available_resolutions(video_device);
        if (available_resolutions.empty())
            continue;

        list_webcam_info.push_back({webcam_name, make_device_id(entry.path()), available_resolutions});
    }

    return list_webcam_info;
}

CaptureImpl::CaptureImpl(DeviceId const& id, img::Size const& resolution)
    : _resolution{resolution}
{
    fd = open(id.as_string().c_str(), O_RDWR);
    if (fd == -1)
    {
        perror("Failed to open device");
        exit(EXIT_FAILURE);
    }
    struct v4l2_capability cap;
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == -1)
    {
        perror("Failed to query capabilities");
        exit(EXIT_FAILURE);
    }

    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = _resolution.width();
    fmt.fmt.pix.height      = _resolution.height();
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field       = V4L2_FIELD_NONE;

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) == -1)
    {
        perror("Failed to set format");
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

CaptureImpl::~CaptureImpl()
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

auto CaptureImpl::image() -> MaybeImage
{
    auto* bob = getFrame();
    // assert(bob.size() == _resolution.pixels_count() * 3);
    return std::make_shared<img::Image>(_resolution, img::PixelFormat::RGB, img::FirstRowIs::Bottom, bob);
    // std::lock_guard lock{_mutex};

    // auto res = std::move(_image);
    // if (std::holds_alternative<img::Image>(res))
    //     _image = NoNewImageAvailableYet{}; // Make sure we know that the current image has been consumed

    // return res; // We don't use std::move here because it would prevent copy elision
}

auto CaptureImpl::getFrame() -> uint8_t*
{
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));

    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd, VIDIOC_DQBUF, &buf) == -1)
    {
        perror("Failed to dequeue buffer");
        // return false;
    }
    uint8_t* rgb_data = new uint8_t[_resolution.pixels_count() * 3];
    yuyv_to_rgb((unsigned char*)buffers[0].start, rgb_data, _resolution.width(), _resolution.height());

    if (ioctl(fd, VIDIOC_QBUF, &buf) == -1)
    {
        perror("Failed to queue buffer");
    }
    return rgb_data;
}

} // namespace wcam::internal
#endif