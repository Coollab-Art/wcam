#include "ICapture.hpp"
#if defined(__linux__)
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>
#include "../Info.hpp"
#include "make_device_id.hpp"
#include "wcam_linux.hpp"

namespace wcam::internal {

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
    : ICapture{id}
    , _resolution{resolution}
{
    openDevice(id);
    initDevice();
    startCapture();
}

CaptureImpl::~CaptureImpl()
{
    stopCapture();
    uninitDevice();
    closeDevice();
}

auto CaptureImpl::image() -> MaybeImage
{
    static std::vector<uint8_t> bob;
    getFrame(bob);
    assert(bob.size() == _resolution.pixels_count() * 3);
    return img::Image{_resolution, img::PixelFormat::RGB, img::FirstRowIs::Bottom, bob.data()};
    // std::lock_guard lock{_mutex};

    // auto res = std::move(_image);
    // if (std::holds_alternative<img::Image>(res))
    //     _image = NoNewImageAvailableYet{}; // Make sure we know that the current image has been consumed

    // return res; // We don't use std::move here because it would prevent copy elision
}

bool CaptureImpl::getFrame(std::vector<uint8_t>& frameBuffer)
{
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));

    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd, VIDIOC_DQBUF, &buf) == -1)
    {
        perror("Failed to dequeue buffer");
        return false;
    }

    frameBuffer.assign(static_cast<uint8_t*>(buffers[buf.index].start), static_cast<uint8_t*>(buffers[buf.index].start) + buf.bytesused);

    if (ioctl(fd, VIDIOC_QBUF, &buf) == -1)
    {
        perror("Failed to queue buffer");
        return false;
    }

    return true;
}

void CaptureImpl::openDevice(DeviceId const& id)
{
    fd = open(id.as_string().c_str(), O_RDWR);
    if (fd == -1)
    {
        perror("Failed to open device");
        exit(EXIT_FAILURE);
    }
}

void CaptureImpl::initDevice()
{
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
    fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) == -1)
    {
        perror("Failed to set format");
        exit(EXIT_FAILURE);
    }

    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count  = 4;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd, VIDIOC_REQBUFS, &req) == -1)
    {
        perror("Failed to request buffers");
        exit(EXIT_FAILURE);
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
            exit(EXIT_FAILURE);
        }

        buffers[i].length = buf.length;
        buffers[i].start  = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
        if (buffers[i].start == MAP_FAILED)
        {
            perror("Failed to map buffer");
            exit(EXIT_FAILURE);
        }
    }
}

void CaptureImpl::startCapture()
{
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
            exit(EXIT_FAILURE);
        }
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) == -1)
    {
        perror("Failed to start capture");
        exit(EXIT_FAILURE);
    }
}

void CaptureImpl::stopCapture()
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMOFF, &type) == -1)
    {
        perror("Failed to stop capture");
        exit(EXIT_FAILURE);
    }
}

void CaptureImpl::uninitDevice()
{
    for (size_t i = 0; i < buffers.size(); ++i)
    {
        if (munmap(buffers[i].start, buffers[i].length) == -1)
        {
            perror("Failed to unmap buffer");
            exit(EXIT_FAILURE);
        }
    }
}

void CaptureImpl::closeDevice()
{
    if (close(fd) == -1)
    {
        perror("Failed to close device");
        exit(EXIT_FAILURE);
    }
}

} // namespace wcam::internal
#endif