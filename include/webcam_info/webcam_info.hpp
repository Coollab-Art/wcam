#pragma once
#include <string>
#include <vector>

namespace webcam_info {

enum class pixel_format { unknown,
                          yuyv,
                          mjpeg };

auto to_string(webcam_info::pixel_format format) -> std::string;

struct info {
    std::string  name{};
    int          width{};
    int          height{};
    pixel_format format{pixel_format::unknown};
};

auto get_all_webcams() -> std::vector<info>;

} // namespace webcam_info
