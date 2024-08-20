#include "Resolution.hpp"
#include <string>

namespace wcam {

auto to_string(Resolution resolution) -> std::string
{
    return std::to_string(resolution.width()) + " x " + std::to_string(resolution.height());
}

} // namespace wcam