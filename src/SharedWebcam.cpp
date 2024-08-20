#include "SharedWebcam.hpp"
#include "internal/WebcamRequest.hpp"

namespace wcam {

auto SharedWebcam::image() const -> MaybeImage
{
    return _request->image();
}

} // namespace wcam