#pragma once
#include "MaybeImage.hpp"
#include "internal/Capture.hpp"

namespace wcam {

namespace internal {
class CapturesManager;
}

class CaptureStrongRef { // TODO rename as SharedCapture?
public:
    /// Returns the latest image that has been captured, or nullopt if this is the same as the image that was retrieved during the previous call to image() (or if no image has been captured yet)
    [[nodiscard]] auto image() -> MaybeImage;

private:
    friend class internal::CapturesManager;
    CaptureStrongRef(std::shared_ptr<internal::Capture> ptr)
        : _ptr{std::move(ptr)}
    {}

private:
    std::shared_ptr<internal::Capture> _ptr;
};

} // namespace wcam