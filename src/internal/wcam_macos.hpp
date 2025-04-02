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

public:// HACK this is public only because the callback needs it
    void webcam_callback(CGImageRef); 

private:
    void open_webcam();
    void close_webcam();
};

} // namespace wcam::internal

#endif