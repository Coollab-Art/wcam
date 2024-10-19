#include "fallback_webcam_name.hpp"

namespace wcam::internal {

auto fallback_webcam_name() -> const char*
{
    return "Unnamed webcam";
}

} // namespace wcam::internal