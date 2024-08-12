#include "wcam_macos.hpp"
#if defined(__APPLE__)

namespace wcam::internal {

void open_webcam();

CaptureImpl::CaptureImpl(UniqueId const& unique_id, img::Size const& resolution)
{
    open_webcam();
}

} // namespace wcam::internal

#endif