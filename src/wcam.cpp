#include "wcam/wcam.hpp"
#include "internal/Manager.hpp"
#include "internal/ResolutionsManager.hpp"

namespace wcam {

auto all_webcams_info() -> std::vector<Info>
{
    return internal::manager()->infos();
}

auto open_webcam(DeviceId const& id) -> SharedWebcam
{
    return internal::manager()->open_or_get_webcam(id);
}

auto get_selected_resolution(DeviceId const& id) -> Resolution
{
    return internal::resolutions_manager().selected_resolution(id);
}

void set_selected_resolution(DeviceId const& id, Resolution resolution)
{
    internal::resolutions_manager().set_selected_resolution(id, resolution);
}

auto get_resolutions_map() -> ResolutionsMap&
{
    return internal::resolutions_manager().get_map();
}

} // namespace wcam