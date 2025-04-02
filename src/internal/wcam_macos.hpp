#pragma once
#if defined(__APPLE__)
#include <CoreImage/CoreImage.h>
#include "ICaptureImpl.hpp"
#include "wcam/wcam.hpp"

namespace wcam::internal {

class CaptureImpl : public ICaptureImpl {
public:
    CaptureImpl(DeviceId const& id, Resolution const& resolution);
    ~CaptureImpl() override;
    CaptureImpl(CaptureImpl const&)                        = delete;
    auto operator=(CaptureImpl const&) -> CaptureImpl&     = delete;
    CaptureImpl(CaptureImpl&&) noexcept                    = delete;
    auto operator=(CaptureImpl&&) noexcept -> CaptureImpl& = delete;

private:
    void open_webcam();
    void close_webcam();
    void webcam_callback(CGImageRef);
};

} // namespace wcam::internal

#endif