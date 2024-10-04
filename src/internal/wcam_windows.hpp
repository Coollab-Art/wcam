#pragma once
#if defined(_WIN32)
#include <mutex>
#include "../DeviceId.hpp"
#include "ICaptureImpl.hpp"
#include "qedit.h"

namespace wcam::internal {

EXTERN_C const IID IID_ISampleGrabberCB;
class CaptureImpl : public ISampleGrabberCB
    , public ICaptureImpl {
public:
    CaptureImpl(DeviceId const& id, Resolution const& resolution);
    ~CaptureImpl() override;
    CaptureImpl(CaptureImpl const&)                        = delete;
    auto operator=(CaptureImpl const&) -> CaptureImpl&     = delete;
    CaptureImpl(CaptureImpl&&) noexcept                    = delete;
    auto operator=(CaptureImpl&&) noexcept -> CaptureImpl& = delete;

    auto image() -> MaybeImage override;

    // clang-format off
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;
    // clang-format on
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP SampleCB(double /* Time */, IMediaSample* /* pSample */) override { return E_NOTIMPL; }
    STDMETHODIMP BufferCB(double /* Time */, BYTE* pBuffer, long BufferLen) override; // NOLINT(*runtime-int)

private:
    MaybeImage _image{ImageNotInitYet{}};
    Resolution _resolution;
    GUID       _video_format; // At the moment we support MEDIASUBTYPE_RGB24 and MEDIASUBTYPE_NV12 (which is required for the OBS virtual camera)
    std::mutex _mutex{};

    IMediaControl* _media_control{};
    ULONG          _ref_count{0};
};

} // namespace wcam::internal

#endif