#pragma once
#include <vector>
#include "../../../src/DeviceId.hpp"
#include "../../../src/Info.hpp"
#include "../../../src/KeepLibraryAlive.hpp"
#include "../../../src/MaybeImage.hpp"
#include "../../../src/Resolution.hpp"
#include "../../../src/SharedWebcam.hpp"
#include "../../../src/overloaded.hpp"
#include "img/img.hpp"

namespace wcam {

/// Returns a list of descriptions of all the cameras that are currently plugged in
auto all_webcams_info() -> std::vector<Info>;

/// Starts capturing the requested camera. If it safe to call it an a camera that is already captured, we will just reuse the existing capture.
auto open_webcam(DeviceId const&) -> SharedWebcam;

auto get_selected_resolution(DeviceId const&) -> Resolution;
void set_selected_resolution(DeviceId const&, Resolution);

} // namespace wcam