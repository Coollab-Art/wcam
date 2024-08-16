#pragma once
#include "../DeviceId.hpp"
#include "../MaybeImage.hpp"

namespace wcam::internal {

class ICapture {
public:
    virtual auto image() -> MaybeImage = 0;

    /// Throws a std::runtime_error if the creation of the Capture fails// TODO this isn't true anymore? it is true but only for weird cases, eg NOT if the webcam is unavailable
    explicit ICapture(DeviceId const& id)
        : _id{id} {}
    virtual ~ICapture()                              = default;
    ICapture(ICapture const&)                        = delete;
    auto operator=(ICapture const&) -> ICapture&     = delete;
    ICapture(ICapture&&) noexcept                    = delete;
    auto operator=(ICapture&&) noexcept -> ICapture& = delete;

    auto id() const -> DeviceId const& { return _id; }

private:
    DeviceId _id;
};

} // namespace wcam::internal