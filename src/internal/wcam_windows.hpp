#pragma once
#if defined(_WIN32)
#include "../DeviceId.hpp"
#include "ICaptureImpl.hpp"
#include "qedit.h"

namespace wcam::internal {

class MediaControlRAII {
public:
    MediaControlRAII() = default;
    ~MediaControlRAII();
    MediaControlRAII(MediaControlRAII const&)                = delete;
    MediaControlRAII& operator=(MediaControlRAII const&)     = delete;
    MediaControlRAII(MediaControlRAII&&) noexcept            = delete;
    MediaControlRAII& operator=(MediaControlRAII&&) noexcept = delete;

    auto operator->() -> IMediaControl* { return _media_control; }
    auto operator->() const -> IMediaControl const* { return _media_control; }
    auto operator*() -> IMediaControl& { return *_media_control; }
    auto operator*() const -> IMediaControl const& { return *_media_control; }
    operator IMediaControl*() { return _media_control; } // NOLINT(*explicit*)
    IMediaControl** operator&()                          // NOLINT(*runtime-operator)
    {
        return &_media_control;
    }

private:
    IMediaControl* _media_control{};
};

EXTERN_C const IID IID_ISampleGrabberCB;
class CaptureImpl : public ISampleGrabberCB
    , public ICaptureImpl {
public:
    CaptureImpl(DeviceId const& id, Resolution const& resolution);
    ~CaptureImpl() override                                = default;
    CaptureImpl(CaptureImpl const&)                        = delete;
    auto operator=(CaptureImpl const&) -> CaptureImpl&     = delete;
    CaptureImpl(CaptureImpl&&) noexcept                    = delete;
    auto operator=(CaptureImpl&&) noexcept -> CaptureImpl& = delete;

    // clang-format off
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;
    // clang-format on
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP SampleCB(double /* Time */, IMediaSample* /* pSample */) override { return E_NOTIMPL; }
    STDMETHODIMP BufferCB(double /* Time */, BYTE* pBuffer, long BufferLen) override; // NOLINT(*runtime-int)

private:
    void configure_sample_grabber(ISampleGrabber* sample_grabber);

private:
    Resolution _resolution{};
    GUID       _video_format{}; // At the moment we support MEDIASUBTYPE_RGB24 and MEDIASUBTYPE_NV12 (which is required for the OBS virtual camera)

    MediaControlRAII _media_control{};
    ULONG            _ref_count{0};
};

} // namespace wcam::internal

#endif