#pragma once
#if defined(__linux__)
#include <atomic>
#include <thread>
#include <vector>
#include "../DeviceId.hpp"
#include "ICaptureImpl.hpp"

namespace wcam::internal {

struct Buffer {
    void*  start;
    size_t length;
};

class CaptureImpl : public ICaptureImpl {
public:
    CaptureImpl(DeviceId const& id, Resolution const& resolution);
    ~CaptureImpl() override;

private:
    auto        getFrame() -> uint8_t*;
    void        openDevice(DeviceId const& id);
    void        initDevice();
    void        startCapture();
    static void thread_job(CaptureImpl&);

private:
    Resolution          _resolution;
    int                 fd{-1};
    std::vector<Buffer> buffers;
    std::atomic<bool>   _wants_to_stop_thread{false};
    std::thread         _thread{}; // Must be initialized last, to make sure that everything else is init when the thread starts its job and uses those other things
};

} // namespace wcam::internal

#endif