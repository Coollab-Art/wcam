#pragma once
#include "MaybeImage.hpp"

namespace wcam {

namespace internal {
class Manager;
class WebcamRequest; // We must not include WebcamRequest in our public headers, because it would include Capture, which in turn includes a lot of platform-specific implementation details (and especially on Windows, it would include windows.h, which is an annoying header which can cause compilation issues if not included in the right order / with the right #defines)
} // namespace internal

///
class SharedWebcam {
public:
    /// Returns a new image that has just been captured, or an info telling you what to do (see the definition of MaybeImage for more details)
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