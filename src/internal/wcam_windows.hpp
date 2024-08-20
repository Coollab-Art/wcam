#pragma once
#include "ICaptureImpl.hpp"
#if defined(_WIN32)
#include <mutex>
#include "qedit.h"
#include "wcam/wcam.hpp"

namespace wcam::internal {

inline int clamp(int value)
{
    return value < 0 ? 0 : (value > 255 ? 255 : value);
}

inline void NV12ToRGB24(uint8_t* nv12Data, uint8_t* rgbData, img::Size::DataType width, img::Size::DataType height)
{
    img::Size::DataType frameSize = width * height;

    uint8_t* yPlane  = nv12Data;
    uint8_t* uvPlane = nv12Data + frameSize;

    for (img::Size::DataType j = 0; j < height; j++)
    {
        for (img::Size::DataType i = 0; i < width; i++)
        {
            img::Size::DataType yIndex  = j * width + i;
            img::Size::DataType uvIndex = (j / 2) * (width / 2) + (i / 2);

            uint8_t Y = yPlane[yIndex];
            uint8_t U = uvPlane[uvIndex * 2];
            uint8_t V = uvPlane[uvIndex * 2 + 1];

            int C = Y - 16;
            int D = U - 128;
            int E = V - 128;

            int R = clamp((298 * C + 409 * E + 128) >> 8);
            int G = clamp((298 * C - 100 * D - 208 * E + 128) >> 8);
            int B = clamp((298 * C + 516 * D + 128) >> 8);

            rgbData[(i + j * width) * 3 + 0] = (uint8_t)R;
            rgbData[(i + j * width) * 3 + 1] = (uint8_t)G;
            rgbData[(i + j * width) * 3 + 2] = (uint8_t)B;
        }
    }
}

EXTERN_C const IID IID_ISampleGrabberCB;
class CaptureImpl : public ISampleGrabberCB
    , public ICaptureImpl {
public:
    CaptureImpl(DeviceId const& id, img::Size const& resolution);
    ~CaptureImpl() override;

    STDMETHODIMP_(ULONG)
    AddRef() override;
    STDMETHODIMP_(ULONG)
    Release() override;
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP SampleCB(double /* Time */, IMediaSample* /* pSample */) override { return E_NOTIMPL; }

    // TODO apparently SampleCB has less overhead than BufferCB
    //     STDMETHODIMP SampleCB(double , IMediaSample *pSample){
    //     if(WaitForSingleObject(hEvent, 0) == WAIT_OBJECT_0) return S_OK;

    //     HRESULT hr = pSample->GetPointer(&ptrBuffer);

    //     if(hr == S_OK){
    //         latestBufferLength = pSample->GetActualDataLength();
    //           if(latestBufferLength == numBytes){
    //             EnterCriticalSection(&critSection);
    //                   memcpy(pixels, ptrBuffer, latestBufferLength);
    //                 newFrame    = true;
    //                 freezeCheck = 1;
    //             LeaveCriticalSection(&critSection);
    //             SetEvent(hEvent);
    //         }else{
    //             DebugPrintOut("ERROR: SampleCB() - buffer sizes do not match\n");
    //         }
    //     }

    //     return S_OK;
    // }

    STDMETHODIMP BufferCB(double /* Time */, BYTE* pBuffer, long BufferLen) override
    {
        auto*            buffer = new uint8_t[_resolution.pixels_count() * 3]; // NOLINT(*owning-memory)
        img::PixelFormat pixel_format;
        img::FirstRowIs  row_order;
        if (_video_format == MEDIASUBTYPE_RGB24)
        {
            assert(_resolution.pixels_count() * 3 == static_cast<img::Size::DataType>(BufferLen));
            memcpy(buffer, pBuffer, BufferLen * sizeof(uint8_t));
            pixel_format = img::PixelFormat::BGR;
            row_order    = img::FirstRowIs::Bottom;
        }
        else
        {
            assert(_video_format == MEDIASUBTYPE_NV12);
            assert(_resolution.pixels_count() * 3 / 2 == static_cast<img::Size::DataType>(BufferLen));
            NV12ToRGB24(pBuffer, buffer, _resolution.width(), _resolution.height());
            pixel_format = img::PixelFormat::RGB;
            row_order    = img::FirstRowIs::Top;
        }
        {
            std::unique_lock lock{_mutex};
            _image = img::Image{_resolution, pixel_format, row_order, buffer};
        }
        return 0;
    }

    auto image() -> MaybeImage override;

private:
    auto is_disconnected() -> bool;

private:
    MaybeImage _image{ImageNotInitYet{}};
    img::Size  _resolution;
    GUID       _video_format; // At the moment we support MEDIASUBTYPE_RGB24 and MEDIASUBTYPE_NV12 (which is required for the OBS virtual camera)
    std::mutex _mutex{};

    IMediaControl* _media_control{};
    IMediaEventEx* _media_event{};

    ULONG _ref_count{0};
};

} // namespace wcam::internal

#endif