#pragma once
#include <exception>
#include <mutex>
#include "../MaybeImage.hpp"

namespace wcam::internal {

class CaptureException : public std::exception {
public:
    explicit CaptureException(CaptureError capture_error)
        : capture_error{std::move(capture_error)}
    {}
    CaptureError capture_error;
};

class ICaptureImpl {
public:
    /// Throws a CaptureException if the creation of the Capture fails
    ICaptureImpl()                                           = default;
    virtual ~ICaptureImpl()                                  = default;
    ICaptureImpl(ICaptureImpl const&)                        = delete;
    auto operator=(ICaptureImpl const&) -> ICaptureImpl&     = delete;
    ICaptureImpl(ICaptureImpl&&) noexcept                    = delete;
    auto operator=(ICaptureImpl&&) noexcept -> ICaptureImpl& = delete;

    auto image() -> MaybeImage;

protected:
    void set_image(MaybeImage);

private:
    MaybeImage _image{ImageNotInitYet{}};
    std::mutex _mutex{};
};

} // namespace wcam::internal