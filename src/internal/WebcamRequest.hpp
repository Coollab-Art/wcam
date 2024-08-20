#pragma once
#include <variant>
#include "../DeviceId.hpp"
#include "Capture.hpp"

namespace wcam::internal {

struct CaptureNotInitYet {};
using MaybeCapture = std::variant<Capture, CaptureError, CaptureNotInitYet>;

class WebcamRequest {
public:
    explicit WebcamRequest(DeviceId const& id)
        : _id{id}
    {}

    [[nodiscard]] auto image() const -> MaybeImage; // TODO return by const& ?

    [[nodiscard]] auto id() const -> DeviceId const& { return _id; }
    [[nodiscard]] auto maybe_capture() const -> MaybeCapture const& { return _maybe_capture; }
    [[nodiscard]] auto maybe_capture() -> MaybeCapture& { return _maybe_capture; }

private:
    DeviceId             _id;
    mutable MaybeCapture _maybe_capture{CaptureNotInitYet{}};
};

} // namespace wcam::internal