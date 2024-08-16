#include "InfosManager.hpp"
#include "CapturesManager.hpp"
#include "wcam_linux.hpp"
#include "wcam_macos.hpp"
#include "wcam_windows.hpp"

namespace wcam::internal {

auto grab_all_infos_impl() -> std::vector<Info>;

auto grab_all_infos() -> std::vector<Info>
{
    auto list_webcams_infos = internal::grab_all_infos_impl();
    for (auto& webcam_info : list_webcams_infos)
    {
        auto& resolutions = webcam_info.resolutions;
        std::sort(resolutions.begin(), resolutions.end(), [](img::Size const& res_a, img::Size const& res_b) {
            return res_a.width() > res_b.width()
                   || (res_a.width() == res_b.width() && res_a.height() > res_b.height());
        });
        resolutions.erase(std::unique(resolutions.begin(), resolutions.end()), resolutions.end());
    }
    return list_webcams_infos;
}

auto InfosManager::infos() -> std::vector<Info>
{
    // TODO do something smart, like keeping them in cache and only refreshing every 0.5 seconds?
    auto const new_infos = grab_all_infos();
    /// Signal the captures manager when a webcam is plugged back in
    for (auto const& info : new_infos)
    {
        if (_infos.end() == std::find_if(_infos.begin(), _infos.end(), [&](Info const& info2) {
                return info.id == info2.id;
            }))
        {
            captures_manager().on_webcam_plugged_in(info.id);
        }
    }

    _infos = new_infos;
    return new_infos;
}

} // namespace wcam::internal