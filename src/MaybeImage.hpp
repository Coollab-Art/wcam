#pragma once
#include <string>
#include <variant>
#include "img/img.hpp"

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

struct NoNewImageAvailableYet {};
struct ImageNotInitYet {};

using MaybeImage = std::variant<
    img::Image,             // TODO store a shared ptr so that we know if ppl are holding ref on that or not, to know when we can switch to a new image?
    NoNewImageAvailableYet, // The previous image that you received is still valid, it is the most up to date one
    ImageNotInitYet,        // The capture has just been started / restarted and no image is available yet. If you held onto a previous image, you should clear it, it is not valid anymore (although you might want to freeze on the last image until a new one is available, that's a valid choice too).
    CaptureError            // The camera is currently unavailable. You should display an error message to the user (you can use `wcam::to_string(maybe_image)`). If you held onto a previous image, you should clear it, it is not valid anymore (although you might want to freeze on the last image until a new one is available, that's a valid choice too).
    >;

} // namespace wcam