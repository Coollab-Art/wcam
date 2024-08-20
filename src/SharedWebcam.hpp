#pragma once
#include "MaybeImage.hpp"
#include "internal/WebcamRequest.hpp"

namespace wcam {

namespace internal {
class Manager;
// class WebcamRequest; // We must not include WebcamRequest in our public headers, because it would include Capture, which in turn includes a lot of platform-specific implementation details (and especially on Windows, it would include windows.h, which is an annoying header which can cause compilation issues if not included in the right order / with the right #defines) // TODO remove ?
} // namespace internal

///
class SharedWebcam {
public:
    // TODO update documentation
    /// Returns the latest image that has been captured, or nullopt if this is the same as the image that was retrieved during the previous call to image() (or if no image has been captured yet)
    [[nodiscard]] auto image() const -> MaybeImage;

private:
    friend class internal::Manager;
    explicit SharedWebcam(std::shared_ptr<internal::WebcamRequest> request)
        : _request{std::move(request)}
    {}

private:
    std::shared_ptr<internal::WebcamRequest> _request;
};

} // namespace wcam