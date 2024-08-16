#include "Capture.hpp"
#include "wcam_linux.hpp"
#include "wcam_macos.hpp"
#include "wcam_windows.hpp"

namespace wcam::internal {

Capture::Capture(DeviceId const& id, img::Size const& resolution)
    : _pimpl{std::make_unique<internal::CaptureImpl>(id, resolution)}
{
}

} // namespace wcam::internal