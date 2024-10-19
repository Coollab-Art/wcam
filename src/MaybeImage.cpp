#include "MaybeImage.hpp"
#include "overloaded.hpp"

namespace wcam {

auto to_string(CaptureError const& err) -> std::string
{
    using namespace std::literals;
    return std::visit(
        overloaded{
            [](Error_WebcamAlreadyUsedInAnotherApplication const&) {
                return "Another application is already using the camera. You need to close that other application in order for us to be able to access the camera."s;
            },
            [](Error_WebcamUnplugged const&) {
                return "The camera has been unplugged. You need to re-plug it in."s;
            },
            [](Error_Unknown const& err) {
                return "Unexpected error: "s + err.message;
            },
        },
        err
    );
}

} // namespace wcam