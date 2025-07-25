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

auto get_selected_resolution(DeviceId const& id) -> Resolution
{
    return internal::manager().selected_resolution(id);
}

void set_selected_resolution(DeviceId const& id, Resolution resolution)
{
    internal::manager().set_selected_resolution(id, resolution);
}

auto get_name(DeviceId const& id) -> std::string
{
    return internal::manager().get_name(id);
}

auto get_resolutions_map() -> ResolutionsMap&
{
    return internal::manager().get_resolutions_map();
}

void update()
{
    internal::manager().check_if_update_needs_to_continue();
}

} // namespace wcam