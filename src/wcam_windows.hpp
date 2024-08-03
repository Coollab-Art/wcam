#pragma once
#if defined(_WIN32)
#include <shared_mutex>
#include "img/img.hpp"
#include "internal.hpp"
#include "qedit.h"
#include "wcam/wcam.hpp"

namespace wcam::internal {

EXTERN_C const IID IID_ISampleGrabberCB;
class CaptureImpl : public ISampleGrabberCB
    , public ICapture {
public:
    CaptureImpl(UniqueId const& unique_id, img::Size const& resolution);

    STDMETHODIMP_(ULONG)
    AddRef() { return 1; }
    STDMETHODIMP_(ULONG)
    Release() { return 2; }
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv)
    {
        if (riid == IID_ISampleGrabberCB || riid == IID_IUnknown)
        {
            *ppv = (void*)this;
            return NOERROR;
        }
        return E_NOINTERFACE;
    }

    STDMETHODIMP SampleCB(double Time, IMediaSample* pSample)
    {
        return 0;
    }

    STDMETHODIMP BufferCB(double Time, BYTE* pBuffer, long BufferLen)
    {
        assert(BufferLen == _resolution.width() * _resolution.height() * 3);
        auto buffer = new uint8_t[BufferLen];
        memcpy(buffer, pBuffer, BufferLen * sizeof(uint8_t));
        {
            std::unique_lock lock{*_mutex};
            _image = img::Image(_resolution, 3, buffer);
        }
        return 0;
    }

    auto image() -> std::optional<img::Image>
    {
        std::unique_lock lock{*_mutex};
        auto             res = std::move(_image);
        _image               = std::nullopt;
        return res; // We don't use std::move because it would prevent copy elision
    }

private:
    std::optional<img::Image>          _image{};
    img::Size                          _resolution;
    std::unique_ptr<std::shared_mutex> _mutex{std::make_unique<std::shared_mutex>()};
};

} // namespace wcam::internal

#endif