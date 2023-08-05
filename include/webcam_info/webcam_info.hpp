#pragma once
#include <string>
#include <vector>

namespace webcam_info {

struct Resolution {
    int         width{1};
    int         height{1};
    friend auto operator==(Resolution const& a, Resolution const& b) -> bool
    {
        return a.width == b.width && a.height == b.height;
    };
};

struct Info {
    std::string             name{};
    std::vector<Resolution> available_resolutions{};
};

auto grab_all_webcams_infos() -> std::vector<Info>;

} // namespace webcam_info
