#pragma once
#include <unordered_map>
#include "DeviceId.hpp"
#include "Resolution.hpp"

namespace wcam {

using ResolutionsMap = std::unordered_map<DeviceId, Resolution>;

}