#pragma once
#include <string>
#include <vector>
#include "DeviceId.hpp"
#include "img/img.hpp"

namespace wcam {

struct Info {
    std::string            name{};        /// Name that can be displayed in the UI
    DeviceId               id;            /// A unique ID that identifies the device (don't use the name to identify the device, use the ID !)
    std::vector<img::Size> resolutions{}; /// Lists all the resolutions that the camera can produce
    // std::vector<std::string> pixel_formats{}; // TODO
};

} // namespace wcam