#if defined(__linux__)
#include "wcam_linux.hpp"
#include <fcntl.h>
#include <jpeglib.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <cstring>
#include <filesystem>
#include <format>
#include <functional>
#include <source_location>
#include "../Info.hpp"
#include "ImageFactory.hpp"
#include "fallback_webcam_name.hpp"
#include "make_device_id.hpp"

namespace wcam::internal {

static auto errno_to_string(int errnum) -> std::string
{
    auto error_message = std::string(128, '\0'); // Start with a reasonable buffer size

#ifdef _GNU_SOURCE
    // GNU version of strerror_r returns a char*
    char* const result = strerror_r(errnum, error_message.data(), error_message.size());
    // The result may not use the provided buffer, but an internal buffer instead, so return a copy of that buffer
    if (result != error_message.data())
        return result;
#else
    // POSIX version of strerror_r returns an int and fills the buffer
    int result;
    while (true) // Retry until we succeed
    {
        result = strerror_r(errnum, error_message.data(), error_message.size());
        if (result != ERANGE)
            break;
        error_message.resize(error_message.size() * 2);
    }

    if (result != 0)
        return "Unknown error";
#endif

    // Resize the string to fit the actual message length
    error_message.resize(std::strlen(error_message.c_str()));
    return error_message;
}

static void throw_error(std::string const& err, std::string_view code_that_failed, std::source_location location = std::source_location::current())
{
    if (errno == 16)
        throw CaptureException{Error_WebcamAlreadyUsedInAnotherApplication{}};
    throw CaptureException{Error_Unknown{std::format("{}\n(During `{}`, at {}({}:{}))", err, code_that_failed, location.file_name(), location.line(), location.column())}};
}

#define THROW_IF_ERR(exp) /*NOLINT(*macro*)*/          \
    {                                                  \
        int const err_code = exp;                      \
        if (err_code == -1)                            \
            throw_error(errno_to_string(errno), #exp); \
    }

static auto for_each_webcam_path(std::function<void(std::filesystem::path const& webcam_path)> const& callback)
{
    try
    {
        for (auto const& entry : std::filesystem::directory_iterator("/dev/v4l/by-id"))
            callback(entry.path());
    }
    catch (std::exception const&)
    {
    }
}

static auto webcam_path(DeviceId const& id) -> std::string
{
    return "/dev/v4l/by-id/" + id.as_string();
}

static auto webcam_id(std::filesystem::path const& webcam_path) -> DeviceId
{
    return make_device_id(webcam_path.filename());
}

class FileRAII {
public:
    FileRAII(FileRAII const&)            = delete;
    FileRAII& operator=(FileRAII const&) = delete;
    FileRAII(FileRAII&&)                 = delete;
    FileRAII& operator=(FileRAII&&)      = delete;

    explicit FileRAII(int file_handle)
        : _file_handle{file_handle}
    {}

    ~FileRAII()
    {
        close(_file_handle);
    }

private:
    int _file_handle{};
};

static auto find_webcam_name(int webcam_handle) -> std::string
{
    auto cap = v4l2_capability{};
    if (ioctl(webcam_handle, VIDIOC_QUERYCAP, &cap) == -1)
        return fallback_webcam_name();

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
        return fallback_webcam_name();

    return reinterpret_cast<const char*>(cap.card); // NOLINT(*-pro-type-reinterpret-cast)
}

static auto find_resolutions(int webcam_handle) -> std::vector<Resolution>
{
    auto resolutions = std::vector<Resolution>{};

    auto format_description = v4l2_fmtdesc{};
    format_description.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    for (; ioctl(webcam_handle, VIDIOC_ENUM_FMT, &format_description) == 0; format_description.index++)
    {
        auto frame_size         = v4l2_frmsizeenum{};
        frame_size.pixel_format = format_description.pixelformat;
        for (; ioctl(webcam_handle, VIDIOC_ENUM_FRAMESIZES, &frame_size) == 0; frame_size.index++)
        {
            if (frame_size.type != V4L2_FRMSIZE_TYPE_DISCRETE)
                continue;
            auto frame_interval         = v4l2_frmivalenum{};
            frame_interval.pixel_format = format_description.pixelformat;
            frame_interval.width        = frame_size.discrete.width;
            frame_interval.height       = frame_size.discrete.height;

            for (; ioctl(webcam_handle, VIDIOC_ENUM_FRAMEINTERVALS, &frame_interval) == 0; frame_interval.index++)
            {
                if (frame_interval.type != V4L2_FRMIVAL_TYPE_DISCRETE)
                    continue;
                resolutions.emplace_back(static_cast<Resolution::DataType>(frame_interval.width), static_cast<Resolution::DataType>(frame_interval.height));
            }
        }
    }

    return resolutions;
}

auto grab_all_infos_impl() -> std::vector<Info>
{
    auto infos = std::vector<Info>{};

    for_each_webcam_path([&](std::filesystem::path const& webcam_path) {
        int const webcam_handle = open(webcam_path.string().c_str(), O_RDONLY);
        if (webcam_handle == -1)
            return;
        auto const scope_guard = FileRAII{webcam_handle};

        auto const resolutions = find_resolutions(webcam_handle);
        if (resolutions.empty())
            return;

        infos.push_back({find_webcam_name(webcam_handle), webcam_id(webcam_path), resolutions});
    });

    return infos;
}

/// The list of formats we support for now. We will add more when the need arises.
static auto is_supported_format(uint32_t format) -> bool
{
    return format == V4L2_PIX_FMT_MJPEG
           || format == V4L2_PIX_FMT_YUYV;
}

static auto select_pixel_format(int fd, int width, int height) -> uint32_t
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

CaptureImpl::CaptureImpl(DeviceId const& id, Resolution const& resolution)
    : _webcam_handle{open(webcam_path(id).c_str(), O_RDWR)}
    , _resolution{resolution}
{
    if (_webcam_handle == -1)
        throw CaptureException{Error_WebcamUnplugged{}};
    _pixel_format = select_pixel_format(_webcam_handle, resolution.width(), resolution.height());

    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = _resolution.width();
    fmt.fmt.pix.height      = _resolution.height();
    fmt.fmt.pix.pixelformat = _pixel_format;
    fmt.fmt.pix.field       = V4L2_FIELD_NONE;

    THROW_IF_ERR(ioctl(_webcam_handle, VIDIOC_S_FMT, &fmt));

    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count  = 6;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    THROW_IF_ERR(ioctl(_webcam_handle, VIDIOC_REQBUFS, &req));

    _buffers.resize(req.count);
    for (size_t i = 0; i < req.count; ++i)
    {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;

        THROW_IF_ERR(ioctl(_webcam_handle, VIDIOC_QUERYBUF, &buf));

        _buffers[i].size = buf.length;
        _buffers[i].ptr  = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, _webcam_handle, buf.m.offset);
        if (_buffers[i].ptr == MAP_FAILED)
        {
            perror("Failed to map buffer");
        }
    }
    for (size_t i = 0; i < _buffers.size(); ++i)
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
    for (size_t i = 0; i < _buffers.size(); ++i)
    {
        if (munmap(_buffers[i].ptr, _buffers[i].size) == -1)
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

static void mjpeg_to_rgb(Buffer buffer, unsigned char* rgb_data)
{
    struct jpeg_decompress_struct info; // NOLINT(*member-init)
    struct jpeg_error_mgr         err;  // NOLINT(*member-init)

    info.err = jpeg_std_error(&err);
    jpeg_create_decompress(&info);

    jpeg_mem_src(&info, static_cast<unsigned char*>(buffer.ptr), buffer.size);
    jpeg_read_header(&info, TRUE);
    jpeg_start_decompress(&info);

    while (info.output_scanline < info.output_height)
    {
        unsigned char* buffer_array = rgb_data + static_cast<uint64_t>(info.output_scanline) * static_cast<uint64_t>(info.output_width) * static_cast<uint64_t>(info.output_components); // NOLINT(*pointer-arithmetic)
        jpeg_read_scanlines(&info, &buffer_array, 1);
    }

    jpeg_finish_decompress(&info);
    jpeg_destroy_decompress(&info);
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

    if (_pixel_format == V4L2_PIX_FMT_YUYV)
    {
        image->set_data(ImageDataView<YUYV>{static_cast<unsigned char*>(_buffers[buf.index].ptr), _buffers[buf.index].size, _resolution, wcam::FirstRowIs::Top});
    }
    else if (_pixel_format == V4L2_PIX_FMT_MJPEG)
    {
        auto* const rgb_data = new uint8_t[_resolution.pixels_count() * 3]; // NOLINT(*owning-memory)
        mjpeg_to_rgb(_buffers[buf.index], rgb_data);
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