#include "wcam/wcam.hpp"
#include "internal/Manager.hpp"

namespace wcam {

auto all_webcams_info() -> std::vector<Info>
{
    return internal::manager().infos();
}

auto open_webcam(DeviceId const& id) -> SharedWebcam
{
    return internal::manager().open_or_get_webcam(id);
}

} // namespace wcam