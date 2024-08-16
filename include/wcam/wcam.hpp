#pragma once
#include <vector>
#include "../../../src/CaptureStrongRef.hpp"
#include "../../../src/DeviceId.hpp"
#include "../../../src/Info.hpp"
#include "../../../src/MaybeImage.hpp"
#include "img/img.hpp"

namespace wcam {

/// Returns a list of descriptions of all the cameras that are currently plugged in
auto all_webcams_info() -> std::vector<Info>;

/// Starts capturing the requested camera. If it safe to call it an a camera that is already captured, we will just reuse the existing capture.
auto start_capture(DeviceId const& id, img::Size const& resolution) -> CaptureStrongRef;

} // namespace wcam