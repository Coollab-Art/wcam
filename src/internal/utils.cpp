#include "utils.hpp"
#include "InfosManager.hpp"

namespace wcam::internal {

auto is_plugged_in(DeviceId const& id) -> bool
{
    auto infos = infos_manager().infos();
    return infos.end() != std::find_if(infos.begin(), infos.end(), [&](Info const& info) {
               return info.id == id;
           });
}

} // namespace wcam::internal