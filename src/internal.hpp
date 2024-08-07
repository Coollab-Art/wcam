#pragma once
#include "MaybeImage.hpp"

namespace wcam::internal {

class ICapture {
public:
    virtual auto image() -> MaybeImage = 0;

    ICapture()                                       = default;
    virtual ~ICapture()                              = default;
    ICapture(ICapture const&)                        = delete;
    auto operator=(ICapture const&) -> ICapture&     = delete;
    ICapture(ICapture&&) noexcept                    = delete;
    auto operator=(ICapture&&) noexcept -> ICapture& = delete;
};

} // namespace wcam::internal