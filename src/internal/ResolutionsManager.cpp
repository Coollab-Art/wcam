#include "ResolutionsManager.hpp"
#include "Manager.hpp"

namespace wcam::internal {

auto ResolutionsManager::selected_resolution(DeviceId const& id) const -> Resolution
{
    auto const it = _selected_resolutions.find(id);
    if (it != _selected_resolutions.end())
        return it->second;
    return manager()->default_resolution(id);
}

void ResolutionsManager::set_selected_resolution(DeviceId const& id, Resolution resolution)
{
    auto const it = _selected_resolutions.find(id);
    if (it != _selected_resolutions.end() && it->second == resolution)
        return; // The resolution is already set, no need to do anything, and we don't need to restart the capture
    _selected_resolutions[id] = resolution;
    manager()->restart_capture(id);
}

} // namespace wcam::internal