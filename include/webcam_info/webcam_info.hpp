#pragma once
#include <cstdint>
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

/// format : ordre des pixels,
auto grab_all_webcams_infos() -> std::vector<Info>;

class Image {
    std::vector<uint8_t>;
    Format     format;
    ColorSpace color_space;
}

class Webcam {
public:
    auto get_current_image() -> Image;

private:
    int pouet;
};

auto open_webcam(int index / std::string name) -> Webcam;

} // namespace webcam_info
