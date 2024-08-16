#include "MaybeImage.hpp"

namespace wcam {

auto to_string(CaptureError err) -> std::string
{
    switch (err)
    {
    case CaptureError::WebcamAlreadyUsedInAnotherApplication:
        return "Another application is already using the camera. You need to close that other application in order for us to be able to access the camera.";
    case CaptureError::WebcamUnplugged:
        return "The camera has been unplugged. You need to re-plug it in.";
    }
    assert(false);
    return "";
}

} // namespace wcam