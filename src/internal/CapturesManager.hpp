#pragma once
#include <unordered_map>
#include "../CaptureStrongRef.hpp"
#include "../DeviceId.hpp"
#include "../MaybeImage.hpp"
#include "Capture.hpp"

namespace wcam::internal {

class CapturesManager {
public:
    auto image(CaptureStrongRef const&) -> MaybeImage;
    auto start_capture(DeviceId const& id, img::Size const& resolution) -> CaptureStrongRef;

    void on_webcam_plugged_in(DeviceId const& id);

private:
    std::unordered_map<DeviceId, std::weak_ptr<Capture>> _open_captures;
};

inline auto captures_manager() -> CapturesManager&
{
    static auto instance = CapturesManager{};
    return instance;
}

} // namespace wcam::internal