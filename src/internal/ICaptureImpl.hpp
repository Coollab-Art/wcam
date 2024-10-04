#pragma once
#include <exception>
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
    virtual auto image() -> MaybeImage = 0;

    /// Throws a CaptureException if the creation of the Capture fails
    ICaptureImpl()                                           = default;
    virtual ~ICaptureImpl()                                  = default;
    ICaptureImpl(ICaptureImpl const&)                        = delete;
    auto operator=(ICaptureImpl const&) -> ICaptureImpl&     = delete;
    ICaptureImpl(ICaptureImpl&&) noexcept                    = delete;
    auto operator=(ICaptureImpl&&) noexcept -> ICaptureImpl& = delete;
};

} // namespace wcam::internal