#pragma once
#include <optional>
#include <vector>
#include "../../src/DeviceId.hpp"
#include "../../src/FirstRowIs.hpp"
#include "../../src/Image.hpp"
#include "../../src/Info.hpp"
#include "../../src/MaybeImage.hpp"
#include "../../src/Resolution.hpp"
#include "../../src/ResolutionsMap.hpp"
#include "../../src/SharedWebcam.hpp"
#include "../../src/internal/ImageFactory.hpp"
#include "../../src/overloaded.hpp"

namespace wcam {

/// Returns a list of descriptions of all the cameras that are currently plugged in
auto all_webcams_info() -> std::vector<Info>;

/// Starts capturing the requested camera. If it safe to call it an a camera that is already captured, we will just reuse the existing capture.
auto open_webcam(DeviceId const&) -> SharedWebcam;

auto get_selected_resolution(DeviceId const&) -> Resolution;
void set_selected_resolution(DeviceId const&, Resolution);

/// Might return nullopt if the webcam is not plugged in
auto get_name(DeviceId const&) -> std::optional<std::string>;

template<typename ImageT>
void set_image_type()
{
    assert(!internal::image_factory_pointer() && "You already called set_image_type. You must only call it once."); // NB: actually this isn't a problem to call it several times, but is it really what you want? If yes, then you can comment out this assert and everything will work O:)
    internal::image_factory_pointer() = std::make_unique<internal::ImageFactory<ImageT>>();
}

auto get_resolutions_map() -> ResolutionsMap&;

/// Must be called once every frame
void update();

} // namespace wcam