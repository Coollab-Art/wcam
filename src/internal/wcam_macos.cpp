#if defined(__APPLE__)
#include "wcam_macos.hpp"

namespace wcam::internal {

CaptureImpl::CaptureImpl(DeviceId const& id, Resolution const& resolution)
{
    open_webcam();
}

CaptureImpl::~CaptureImpl()
{
    close_webcam();
}

} // namespace wcam::internal

#endif