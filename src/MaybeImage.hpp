#pragma once
#include <string>
#include <variant>
#include "img/img.hpp"

namespace wcam {

enum class CaptureError {
    WebcamAlreadyUsedInAnotherApplication,
    WebcamUnplugged,
};

auto to_string(CaptureError) -> std::string;

struct NoNewImageAvailableYet {};

using MaybeImage = std::variant<img::Image, NoNewImageAvailableYet, CaptureError>;

} // namespace wcam