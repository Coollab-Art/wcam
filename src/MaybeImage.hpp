#pragma once
#include <string>
#include <variant>
#include "Image.hpp"

namespace wcam {

struct Error_WebcamAlreadyUsedInAnotherApplication {};
struct Error_WebcamUnplugged {};
struct Error_Unknown {
    std::string message;
};

using CaptureError = std::variant<
    Error_WebcamAlreadyUsedInAnotherApplication,
    Error_WebcamUnplugged,
    Error_Unknown>;

auto to_string(CaptureError const&) -> std::string;

struct ImageNotInitYet {};

using MaybeImage = std::variant<
    ImageNotInitYet,              // The capture has just been started / restarted and no image is available yet. If you held onto a previous image, you should clear it, it is not valid anymore (although you might want to freeze on the last image until a new one is available, that's a valid choice too).
    std::shared_ptr<Image const>, // The capture is working and this is the latest image.
    CaptureError                  // The camera is currently unavailable. You should display an error message to the user (you can use `wcam::to_string(maybe_image)`). If you held onto a previous image, you should clear it, it is not valid anymore (although you might want to freeze on the last image until a new one is available, that's a valid choice too).
    >;

} // namespace wcam