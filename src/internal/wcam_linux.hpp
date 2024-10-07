#pragma once
#if defined(__linux__)
#include <atomic>
#include <thread>
#include <vector>
#include "../DeviceId.hpp"
#include "ICaptureImpl.hpp"
#include "V4l2Capture.h"

namespace wcam::internal {

struct Buffer {
    void*  start;
    size_t length;
};

class Bob {
public:
    Bob(DeviceId const& id, Resolution const& resolution);
    ~Bob();
    auto       getFrame() -> uint8_t*;
    Resolution _resolution;

private:
    int                 fd{-1};
    std::vector<Buffer> buffers;
};

class CaptureImpl : public ICaptureImpl {
public:
    CaptureImpl(DeviceId const& id, Resolution const& resolution);
    ~CaptureImpl() override;

private:
    static void thread_job(CaptureImpl&);

private:
    V4l2Capture* videoCapture;
    // Bob               _bob;
    Resolution        _resolution;
    std::atomic<bool> _wants_to_stop_thread{false};
    std::thread       _thread{}; // Must be initialized last, to make sure that everything else is init when the thread starts its job and uses those other things
};

} // namespace wcam::internal

#endif