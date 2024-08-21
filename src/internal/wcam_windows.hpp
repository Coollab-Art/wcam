#pragma once
#include "ICaptureImpl.hpp"
#if defined(_WIN32)
#include <mutex>
#include "qedit.h"
#include "wcam/wcam.hpp"

namespace wcam::internal {

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

    STDMETHODIMP BufferCB(double /* Time */, BYTE* pBuffer, long BufferLen) override;

    auto image() -> MaybeImage override;

private:
    MaybeImage _image{ImageNotInitYet{}};
    img::Size  _resolution;
    GUID       _video_format; // At the moment we support MEDIASUBTYPE_RGB24 and MEDIASUBTYPE_NV12 (which is required for the OBS virtual camera)
    std::mutex _mutex{};

    IMediaControl* _media_control{};

    ULONG _ref_count{0};
};

} // namespace wcam::internal

#endif