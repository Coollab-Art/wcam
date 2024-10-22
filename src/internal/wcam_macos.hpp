#pragma once
#if defined(__APPLE__)
#include "ICaptureImpl.hpp"
#include <CoreImage/CoreImage.h>
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

    public:
void open_webcam();
void webcam_callback(CGImageRef);
};

} // namespace wcam::internal

#endif