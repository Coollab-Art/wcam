#include "SharedWebcam.hpp"
#include "internal/WebcamRequest.hpp"

namespace wcam {

auto SharedWebcam::image() const -> MaybeImage
{
    return _request->image();
}

auto SharedWebcam::id() const -> DeviceId
{
    return _request->id();
}

} // namespace wcam