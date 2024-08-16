#pragma once
#include "../DeviceId.hpp"

namespace wcam::internal {

auto is_plugged_in(DeviceId const& id) -> bool;

}