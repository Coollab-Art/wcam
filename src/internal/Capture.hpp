#pragma once
#include <memory>
#include "../DeviceId.hpp"
#include "../MaybeImage.hpp"
#include "ICaptureImpl.hpp"

namespace wcam::internal {

class Capture {
public:
    Capture(DeviceId const& id, Resolution const& resolution);

    [[nodiscard]] auto image() -> MaybeImage { return _pimpl->image(); }

private:
    std::unique_ptr<internal::ICaptureImpl> _pimpl;
};

} // namespace wcam::internal