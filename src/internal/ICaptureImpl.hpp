#pragma once
#include "../MaybeImage.hpp"

namespace wcam::internal {

class ICaptureImpl {
public:
    virtual auto image() -> MaybeImage = 0;

    /// Throws a CaptureError if the creation of the Capture fails
    ICaptureImpl()                                           = default;
    virtual ~ICaptureImpl()                                  = default;
    ICaptureImpl(ICaptureImpl const&)                        = delete;
    auto operator=(ICaptureImpl const&) -> ICaptureImpl&     = delete;
    ICaptureImpl(ICaptureImpl&&) noexcept                    = delete;
    auto operator=(ICaptureImpl&&) noexcept -> ICaptureImpl& = delete;
};

} // namespace wcam::internal