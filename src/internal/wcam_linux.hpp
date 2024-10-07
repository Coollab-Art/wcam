#pragma once
#if defined(__linux__)
#include <atomic>
#include <thread>
#include <vector>
#include "../DeviceId.hpp"
#include "ICaptureImpl.hpp"

namespace wcam::internal {

struct Buffer {
    void*  ptr;
    size_t size;
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
    int                 fd{-1};
    std::vector<Buffer> buffers;
    uint32_t            _pixels_format;
    Resolution          _resolution;

    std::atomic<bool> _wants_to_stop_thread{false};
    std::thread       _thread{};
};

} // namespace wcam::internal

#endif