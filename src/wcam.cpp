#include "wcam/wcam.hpp"
#include <img/img.hpp>
#include "wcam_linux.hpp"
#include "wcam_macos.hpp"
#include "wcam_windows.hpp"

namespace wcam {

namespace internal {
auto grab_all_infos_impl() -> std::vector<Info>;
}

auto grab_all_infos() -> std::vector<Info>
{
    auto list_webcams_infos = internal::grab_all_infos_impl();
    for (auto& webcam_info : list_webcams_infos)
    {
        auto& resolutions = webcam_info.available_resolutions;
        std::sort(resolutions.begin(), resolutions.end(), [](img::Size const& res_a, img::Size const& res_b) {
            return res_a.width() > res_b.width()
                   || (res_a.width() == res_b.width() && res_a.height() > res_b.height());
        });
        resolutions.erase(std::unique(resolutions.begin(), resolutions.end()), resolutions.end());
    }
    return list_webcams_infos;
}

Capture::Capture(UniqueId const& unique_id, img::Size const& resolution)
    : _pimpl{std::make_unique<internal::CaptureImpl>(unique_id, resolution)}
{
}

} // namespace wcam