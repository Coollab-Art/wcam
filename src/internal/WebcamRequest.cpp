#include "WebcamRequest.hpp"
#include "../overloaded.hpp"

namespace wcam::internal {

auto WebcamRequest::image() const -> MaybeImage
{
    return std::visit(
        wcam::overloaded{
            [](Capture& capture) -> MaybeImage {
                return capture.image();
            },
            [](CaptureError const& err) -> MaybeImage {
                return err;
            },
            [](CaptureNotInitYet const&) -> MaybeImage {
                return ImageNotInitYet{};
            },
        },
        _maybe_capture
    );
}

} // namespace wcam::internal