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

void CapturesManager::on_webcam_plugged_in(DeviceId const& id)
{
    // Check for captures that were started with this webcam, then the webcam was unplugged, and then plugged back
    // We need to restart those captures, otherwise they will remain frozen (at least on Windows, I haven't checked on other OSes)
    auto const it = _open_captures.find(id);
    if (it == _open_captures.end())
        return;
    it->second = std::make_shared<Capture>(id, img::Size{});
}

} // namespace wcam::internal