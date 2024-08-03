#pragma once
#include <optional>
#include "img/img.hpp"

namespace wcam::internal {

class ICapture {
public:
    virtual auto image() -> std::optional<img::Image> = 0;

    ICapture()                                       = default;
    ICapture(ICapture const&)                        = delete;
    auto operator=(ICapture const&) -> ICapture&     = delete;
    ICapture(ICapture&&) noexcept                    = delete;
    auto operator=(ICapture&&) noexcept -> ICapture& = delete;
    virtual ~ICapture()                              = default;
};

} // namespace wcam::internal