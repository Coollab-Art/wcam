#pragma once
#include <string>
#include <vector>

namespace webcam_info {

struct info {
    std::string name{};
    int         width{};
    int         height{};
};

auto get_all_webcams() -> std::vector<info>;

} // namespace webcam_info
