#pragma once
#if defined(__linux__)
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
    CaptureImpl(DeviceId const& id, img::Size const& resolution);
    ~CaptureImpl() override;

    auto image() -> MaybeImage override;

private:
    auto getFrame() -> uint8_t*;
    void openDevice(DeviceId const& id);
    void initDevice();
    void startCapture();

private:
    MaybeImage _image{ImageNotInitYet{}};
    img::Size  _resolution;
    std::mutex _mutex{};

    int                 fd{-1};
    std::vector<Buffer> buffers;
};

} // namespace wcam::internal

#endif