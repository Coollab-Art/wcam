#pragma once
#if defined(_WIN32)
#include <mutex>
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
    ~CaptureImpl() override;

    STDMETHODIMP_(ULONG)
    AddRef() override;
    STDMETHODIMP_(ULONG)
    Release() override;
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP SampleCB(double /* Time */, IMediaSample* /* pSample */) override { return 0; }

    STDMETHODIMP BufferCB(double /* Time */, BYTE* pBuffer, long BufferLen) override
    {
        assert(static_cast<img::Size::DataType>(BufferLen) == _resolution.width() * _resolution.height() * 3);
        auto buffer = new uint8_t[BufferLen];
        memcpy(buffer, pBuffer, BufferLen * sizeof(uint8_t));
        {
            std::unique_lock lock{_mutex};
            _image = img::Image(_resolution, 3, buffer);
        }
        return 0;
    }

    auto image() -> std::optional<img::Image> override
    {
        std::lock_guard lock{_mutex};
        auto            res = std::move(_image);
        _image              = std::nullopt;
        return res; // We don't use std::move because it would prevent copy elision
    }

private:
    std::optional<img::Image> _image{};
    img::Size                 _resolution;
    std::mutex                _mutex{};

    IMediaControl* _media_control{};

    ULONG _ref_count{0};
};

} // namespace wcam::internal

#endif