#pragma once
#include <string>
#include <vector>
#include "DeviceId.hpp"
#include "Resolution.hpp"

namespace wcam {

struct Info {
    std::string             name{};        /// Name that can be displayed in the UI
    DeviceId                id;            /// A unique ID that identifies the device (don't use the name to identify the device, use the ID !)
    std::vector<Resolution> resolutions{}; /// Lists all the resolutions that the camera can produce
};

} // namespace wcam