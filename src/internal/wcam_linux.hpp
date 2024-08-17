#pragma once
#if defined(__linux__)
#include <vector>
#include "../DeviceId.hpp"
#include "ICapture.hpp"

namespace wcam::internal {

struct Buffer {
    void*  start;
    size_t length;
};

class CaptureImpl : public ICapture {
public:
    CaptureImpl(DeviceId const& id, img::Size const& resolution);
    ~CaptureImpl() override;

    auto image() -> MaybeImage override;

private:
    bool getFrame(std::vector<uint8_t>& frameBuffer);
    void openDevice(DeviceId const& id);
    void initDevice();
    void startCapture();
    void stopCapture();
    void uninitDevice();
    void closeDevice();

private:
    MaybeImage _image{MustClearPreviousImage{}};
    img::Size  _resolution;
    std::mutex _mutex{};

    int                 fd{-1};
    std::vector<Buffer> buffers;
};

} // namespace wcam::internal

#endif