#pragma once
#if defined(__linux__)
#include <array>
#include <atomic>
#include <thread>
#include "../DeviceId.hpp"
#include "ICaptureImpl.hpp"

namespace wcam::internal {

struct Buffer {
    void*  ptr{};
    size_t size{};

    Buffer() = default;
    ~Buffer();
    Buffer(Buffer const&)                = delete;
    Buffer& operator=(Buffer const&)     = delete;
    Buffer(Buffer&&) noexcept            = delete;
    Buffer& operator=(Buffer&&) noexcept = delete;
};

class FileRAII {
public:
    FileRAII(FileRAII const&)                = delete;
    FileRAII& operator=(FileRAII const&)     = delete;
    FileRAII(FileRAII&&) noexcept            = delete;
    FileRAII& operator=(FileRAII&&) noexcept = delete;

    explicit FileRAII(int file_handle)
        : _file_handle{file_handle}
    {}

    operator int() const { return _file_handle; } // NOLINT(*explicit*)

    ~FileRAII()
    {
        close(_file_handle);
    }

private:
    int _file_handle{};
};

class CaptureImpl : public ICaptureImpl {
public:
    CaptureImpl(DeviceId const& id, Resolution const& resolution);
    ~CaptureImpl() override;
    CaptureImpl(CaptureImpl const&)                        = delete;
    auto operator=(CaptureImpl const&) -> CaptureImpl&     = delete;
    CaptureImpl(CaptureImpl&&) noexcept                    = delete;
    auto operator=(CaptureImpl&&) noexcept -> CaptureImpl& = delete;

private:
    static void thread_job(CaptureImpl&);
    void        process_next_image();

private:
    FileRAII              _webcam_handle;
    std::array<Buffer, 6> _buffers; // 6 is nice number that gives us good performance
    uint32_t              _pixel_format;
    Resolution            _resolution;

    std::atomic<bool> _wants_to_stop_thread{false};
    std::thread       _thread{};
};

} // namespace wcam::internal

#endif