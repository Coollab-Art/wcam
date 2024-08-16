#pragma once
#if defined(__APPLE__)
#include "wcam/wcam.hpp"

namespace wcam::internal {

class CaptureImpl : public ICapture {
public:
    CaptureImpl(DeviceId const& id, img::Size const& resolution);
    ~CaptureImpl() override;

    auto image() -> MaybeImage override
    {
        std::lock_guard lock{_mutex};

        auto res = std::move(_image);
        if (std::holds_alternative<img::Image>(_image))
            _image = NoNewImageAvailableYet{}; // Make sure we know that the current image has been consumed

        return res; // We don't use std::move here because it would prevent copy elision
    }

private:
    auto is_disconnected() -> bool;

private:
    MaybeImage _image{NoNewImageAvailableYet{}};
    img::Size  _resolution;
    std::mutex _mutex{};
};

} // namespace wcam::internal

#endif