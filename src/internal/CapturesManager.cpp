#include "CapturesManager.hpp"
#include "../MaybeImage.hpp"
#include "utils.hpp"

namespace wcam::internal {

auto CapturesManager::image(CaptureStrongRef const& cap_ref) -> MaybeImage
{
    auto const id = cap_ref._ptr->id();
    if (!is_plugged_in(id))
        return CaptureError::WebcamUnplugged;
    return _open_captures[id]->image();
}

auto CapturesManager::start_capture(DeviceId const& id, img::Size const& resolution) -> CaptureStrongRef
{
    _open_captures[id] = std::make_shared<Capture>(id, resolution);
    return CaptureStrongRef{_open_captures[id]};
}

} // namespace wcam::internal