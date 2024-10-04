#pragma once
#include "../ResolutionsMap.hpp"

namespace wcam::internal {

class ResolutionsManager {
public:
    auto selected_resolution(DeviceId const&) const -> Resolution;
    void set_selected_resolution(DeviceId const&, Resolution);

    auto get_map() -> ResolutionsMap& { return _selected_resolutions; }

private:
    ResolutionsMap _selected_resolutions{};
};

inline auto resolutions_manager() -> ResolutionsManager& // This is not part of the Manager, because we don't want it to be destroyed when all KeepLibraryAlive go out of scope. We want to remember the selected resolutions for as long as possible (until the program exits).
{
    static auto instance = ResolutionsManager{};
    return instance;
}

} // namespace wcam::internal