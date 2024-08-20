#pragma once
#include "../MaybeImage.hpp"

namespace wcam::internal {

class ICaptureImpl {
public:
    virtual auto image() -> MaybeImage = 0;

    /// Throws a std::runtime_error if the creation of the Capture fails// TODO this isn't true anymore? it is true but only for weird cases, eg NOT if the webcam is unavailable
    ICaptureImpl()                                           = default;
    virtual ~ICaptureImpl()                                  = default;
    ICaptureImpl(ICaptureImpl const&)                        = delete;
    auto operator=(ICaptureImpl const&) -> ICaptureImpl&     = delete;
    ICaptureImpl(ICaptureImpl&&) noexcept                    = delete;
    auto operator=(ICaptureImpl&&) noexcept -> ICaptureImpl& = delete;
};

} // namespace wcam::internal