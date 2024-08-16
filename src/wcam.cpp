#include "wcam/wcam.hpp"
#include "internal/CapturesManager.hpp"
#include "internal/InfosManager.hpp"

namespace wcam {

auto all_webcams_info() -> std::vector<Info>
{
    return internal::infos_manager().infos();
}

auto start_capture(DeviceId const& id, img::Size const& resolution) -> CaptureStrongRef
{
    return internal::captures_manager().start_capture(id, resolution);
}

} // namespace wcam