#pragma once
#include <memory>
#include "../DeviceId.hpp"
#include "../MaybeImage.hpp"
#include "ICapture.hpp"

namespace wcam::internal {

class Capture {
public:
    Capture(DeviceId const& id, img::Size const& resolution);

    [[nodiscard]] auto image() -> MaybeImage { return _pimpl->image(); } // We don't use std::move because it would prevent copy elision
    [[nodiscard]] auto id() -> DeviceId const& { return _pimpl->id(); }

private:
    std::unique_ptr<internal::ICapture> _pimpl;
};

} // namespace wcam::internal