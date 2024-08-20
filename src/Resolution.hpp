#pragma once
#include "img/img.hpp"

namespace wcam {

using Resolution = img::Size;

auto to_string(Resolution) -> std::string;

} // namespace wcam