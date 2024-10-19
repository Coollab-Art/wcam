#pragma once
#if defined(__APPLE__)
#include "ICaptureImpl.hpp"
#include "wcam/wcam.hpp"

namespace wcam::internal {

class CaptureImpl : public ICaptureImpl {
public:
    CaptureImpl(DeviceId const& id, Resolution const& resolution);
    ~CaptureImpl() override;
    CaptureImpl(CaptureImpl const&)                        = delete;
    auto operator=(CaptureImpl const&) -> CaptureImpl&     = delete;
    CaptureImpl(CaptureImpl&&) noexcept                    = delete;
    auto operator=(CaptureImpl&&) noexcept -> CaptureImpl& = delete;
};

} // namespace wcam::internal

#endif