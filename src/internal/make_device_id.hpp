#pragma once
#include "../DeviceId.hpp"

namespace wcam::internal {

inline auto make_device_id(std::string const& id) -> DeviceId
{
    return DeviceId{id};
}

} // namespace wcam::internal