#if defined(__APPLE__)
#include "wcam_macos.hpp"

namespace wcam::internal {

void open_webcam();

CaptureImpl::CaptureImpl(DeviceId const& id, Resolution const& resolution)
{
    open_webcam();
}

CaptureImpl::~CaptureImpl()
{
}

} // namespace wcam::internal

#endif