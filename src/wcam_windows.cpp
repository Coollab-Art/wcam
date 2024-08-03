#if defined(_WIN32)
#include "wcam_windows.hpp"
#include <dshow.h>
#include <cstdlib>
#include <iostream>
#include <unordered_map>
#include "wcam/wcam.hpp"

namespace wcam::internal {

static auto convert_wstr_to_str(std::wstring const& wstr) -> std::string
{
    std::string res;
    res.reserve(wstr.size());
    for (wchar_t const c : wstr)
        res.push_back(static_cast<char>(c)); // TODO Support proper unicode strings
    return res;
}
using namespace std; // TODO remove

STDMETHODIMP_(ULONG)
CaptureImpl::AddRef()
{
    return InterlockedIncrement(&_ref_count);
}

STDMETHODIMP_(ULONG)
CaptureImpl::Release()
{
    assert(false);
    ULONG new_count = InterlockedDecrement(&_ref_count);
    if (new_count == 0)
        delete this;
    return new_count;
}

STDMETHODIMP CaptureImpl::QueryInterface(REFIID riid, void** ppv)
{
    if (riid == IID_ISampleGrabberCB || riid == IID_IUnknown)
    {
        *ppv = (void*)this;
        return NOERROR;
    }
    return E_NOINTERFACE;
}

CaptureImpl::CaptureImpl(UniqueId const& unique_id, img::Size const& requested_resolution)
{
    HRESULT                hr;
    IEnumMoniker*          pEnum    = nullptr;
    IMoniker*              pMoniker = nullptr;
    ICaptureGraphBuilder2* pBuild   = nullptr;
    IGraphBuilder*         pGraph   = nullptr;
    ICreateDevEnum*        pDevEnum = nullptr;
    IBaseFilter*           pCap     = nullptr;

    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr))
    {
        cout << "Échec de CoInitializeEx!" << endl;
    }

    hr = CoCreateInstance(CLSID_CaptureGraphBuilder2, NULL, CLSCTX_INPROC_SERVER, IID_ICaptureGraphBuilder2, (void**)&pBuild);
    if (FAILED(hr))
    {
        cout << "Échec de la création de CaptureGraphBuilder2!" << endl;
    }

    // Création d'instances COM

    // 2. Create the Filter Graph Manager

    hr = CoCreateInstance(CLSID_FilterGraph, 0, CLSCTX_INPROC_SERVER, IID_IFilterGraph, (void**)&pGraph);
    if (FAILED(hr))
    {
        cout << "Échec de la création de FilterGraph!" << endl;
    }

    hr = pBuild->SetFiltergraph(pGraph); // Utilisation de SetFiltergraph
    if (FAILED(hr))
    {
        cout << "Échec de l'appel de SetFiltergraph!" << endl;
    }

    // Obtenir l'objet Moniker correspondant au périphérique sélectionné

    hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDevEnum));
    if (FAILED(hr))
    {
        cout << "Échec de la création de SystemDeviceEnum!" << endl;
    }

    hr = pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, 0);
    if (hr != S_OK)
    {
        cout << "Échec de la création de l'énumérateur de périphériques ou aucun périphérique trouvé!" << endl;
    }

    while (pEnum->Next(1, &pMoniker, NULL) == S_OK)
    {
        IPropertyBag* pPropBag;
        hr = pMoniker->BindToStorage(0, 0, IID_PPV_ARGS(&pPropBag));
        if (FAILED(hr))
        {
            pMoniker->Release();
            continue;
        }

        VARIANT var;
        VariantInit(&var);

        hr = pPropBag->Read(L"FriendlyName", &var, 0);
        if (SUCCEEDED(hr))
        {
            if (UniqueId{convert_wstr_to_str(var.bstrVal)} == unique_id)
            {
                break;
            }
            VariantClear(&var);
        }

        pPropBag->Release();
        pMoniker->Release();
    }
    // TODO tell the moniker which resolution to use
    // Liaison au filtre de capture du périphérique sélectionné

    hr = pMoniker->BindToObject(0, 0, IID_IBaseFilter, (void**)&pCap);
    if (FAILED(hr))
    {
        cout << "Échec de la liaison à l'objet du périphérique!" << endl;
    }

    // 3. Add the Webcam Filter to the Graph

    hr = pGraph->AddFilter(pCap, L"CaptureFilter");
    if (FAILED(hr))
    {
        cout << "Échec de l'ajout du filtre de capture!" << endl;
    }

    // 4. Add and Configure the Sample Grabber

    IBaseFilter*    pSampleGrabberFilter = NULL;
    ISampleGrabber* pSampleGrabber       = NULL;

    hr = CoCreateInstance(CLSID_SampleGrabber, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)&pSampleGrabberFilter);
    if (FAILED(hr))
    {
        std::cout << "3";
    }

    hr = pSampleGrabberFilter->QueryInterface(IID_ISampleGrabber, (void**)&pSampleGrabber);
    if (FAILED(hr))
    {
        std::cout << "2";
    }

    // Configure the sample grabber
    AM_MEDIA_TYPE mt;
    ZeroMemory(&mt, sizeof(AM_MEDIA_TYPE));
    mt.majortype = MEDIATYPE_Video;
    mt.subtype   = MEDIASUBTYPE_RGB24; // Or any other format you prefer
    pSampleGrabber->SetMediaType(&mt);
    pSampleGrabber->SetOneShot(FALSE);
    pSampleGrabber->SetBufferSamples(TRUE);

    // Add the sample grabber to the graph
    hr = pGraph->AddFilter(pSampleGrabberFilter, L"Sample Grabber");
    if (FAILED(hr))
    {
        std::cout << "ça marche";
    }

    // 5. Render the Stream
    IBaseFilter* pNullRenderer = NULL;
    hr                         = CoCreateInstance(CLSID_NullRenderer, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)&pNullRenderer);
    // CHECK_HR(hr);
    hr = pGraph->AddFilter(pNullRenderer, L"Null Renderer");
    // CHECK_HR(hr);

    hr = pBuild->RenderStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, pCap, pSampleGrabberFilter, pNullRenderer);
    if (FAILED(hr))
    {
        cout << "Échec de la configuration du flux de prévisualisation!" << endl;
    }

    // 6. Retrieve the Video Information Header

    AM_MEDIA_TYPE mtGrabbed;
    hr = pSampleGrabber->GetConnectedMediaType(&mtGrabbed);
    if (FAILED(hr))
    {
        std::cout << "1";
    }

    VIDEOINFOHEADER* pVih = (VIDEOINFOHEADER*)mtGrabbed.pbFormat;
    _resolution           = img::Size{
        static_cast<img::Size::DataType>(pVih->bmiHeader.biWidth),
        static_cast<img::Size::DataType>(pVih->bmiHeader.biHeight),
    };

    assert(pVih->bmiHeader.biSizeImage == _resolution.width() * _resolution.height() * 3);

    // 7. Start the Graph

    // IMediaControl *pControl = NULL;
    pGraph->QueryInterface(IID_IMediaControl, (void**)&pControl);

    // 8. Implement ISampleGrabberCB Interface

    hr = pControl->Run();

    if (FAILED(hr))
    {
        std::cout << "ça marche";
    }

    // Create an instance of the callback
    // _sgCallback = SampleGrabberCallback{resolution};

    pSampleGrabber->SetCallback(this, 1);

    MSG msg;

    //     while (true) {
    //    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) > 0) {
    //         // DefWindowProc(msg.hwnd, msg.message, msg.wParam, msg.lParam);
    //     }
    // }

    // Boucle de message pour la gestion des événements

    // Libération des ressources et dé-initialisation de COM
}

CaptureImpl::~CaptureImpl()
{
    pControl->Stop();
}

#if defined(GCC) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wlanguage-extension-token"
#endif

static auto get_video_parameters(IBaseFilter* pCaptureFilter) -> std::vector<img::Size>
{
    std::vector<img::Size> available_resolutions;

    IEnumPins* pEnumPins; // NOLINT(*-init-variables)
    HRESULT    hr = pCaptureFilter->EnumPins(&pEnumPins);
    if (SUCCEEDED(hr))
    {
        IPin* pPin; // NOLINT(*-init-variables)
        while (pEnumPins->Next(1, &pPin, nullptr) == S_OK)
        {
            PIN_DIRECTION pinDirection; // NOLINT(*-init-variables)
            pPin->QueryDirection(&pinDirection);

            if (pinDirection == PINDIR_OUTPUT)
            {
                IAMStreamConfig* pStreamConfig; // NOLINT(*-init-variables)
                hr = pPin->QueryInterface(IID_PPV_ARGS(&pStreamConfig));
                if (SUCCEEDED(hr))
                {
                    int iCount; // NOLINT(*-init-variables)
                    int iSize;  // NOLINT(*-init-variables)
                    hr = pStreamConfig->GetNumberOfCapabilities(&iCount, &iSize);
                    if (SUCCEEDED(hr))
                    {
                        VIDEO_STREAM_CONFIG_CAPS caps;
                        for (int i = 0; i < iCount; i++)
                        {
                            AM_MEDIA_TYPE* pmtConfig;                                                         // NOLINT(*-init-variables)
                            hr = pStreamConfig->GetStreamCaps(i, &pmtConfig, reinterpret_cast<BYTE*>(&caps)); // NOLINT(*-pro-type-reinterpret-cast)
                            if (SUCCEEDED(hr))
                            {
                                if (pmtConfig->formattype == FORMAT_VideoInfo)
                                {
                                    auto* pVih = reinterpret_cast<VIDEOINFOHEADER*>(pmtConfig->pbFormat); // NOLINT(*-pro-type-reinterpret-cast)
                                    available_resolutions.push_back({static_cast<img::Size::DataType>(pVih->bmiHeader.biWidth), static_cast<img::Size::DataType>(pVih->bmiHeader.biHeight)});
                                }
                            }
                        }
                    }
                    pStreamConfig->Release();
                }
            }
            pPin->Release();
        }
        pEnumPins->Release();
    }

    return available_resolutions;
}

static auto get_devices_info(IEnumMoniker* pEnum) -> std::vector<Info>
{
    thread_local auto resolutions_cache = std::unordered_map<std::string, std::vector<img::Size>>{}; // This cache limits the number of times we will allocate IBaseFilter which seems to leak because of a Windows bug.

    std::vector<Info> list_webcam_info{};

    IMoniker* pMoniker; // NOLINT(*-init-variables)
    while (pEnum->Next(1, &pMoniker, nullptr) == S_OK)
    {
        IPropertyBag* pPropBag; // NOLINT(*-init-variables)
        HRESULT       hr = pMoniker->BindToStorage(nullptr, nullptr, IID_PPV_ARGS(&pPropBag));
        if (FAILED(hr))
        {
            pMoniker->Release();
            continue;
        }

        // Get description or friendly name.
        VARIANT webcam_name_wstr;
        VariantInit(&webcam_name_wstr);
        // hr = pPropBag->Read(L"Description", &webcam_name_wstr, nullptr);// ?????
        // if (FAILED(hr))
        // {
        hr = pPropBag->Read(L"FriendlyName", &webcam_name_wstr, nullptr);
        // }
        if (SUCCEEDED(hr))
        {
            auto       available_resolutions = std::vector<img::Size>{};
            auto const webcam_name           = convert_wstr_to_str(webcam_name_wstr.bstrVal);
            auto const it                    = resolutions_cache.find(webcam_name);
            if (it != resolutions_cache.end())
            {
                available_resolutions = it->second;
            }
            else
            {
                IBaseFilter* pCaptureFilter; // NOLINT(*-init-variables)
                hr = pMoniker->BindToObject(nullptr, nullptr, IID_PPV_ARGS(&pCaptureFilter));
                if (SUCCEEDED(hr))
                {
                    available_resolutions = get_video_parameters(pCaptureFilter);
                    pCaptureFilter->Release();
                }
                resolutions_cache[webcam_name] = available_resolutions;
            }
            if (!available_resolutions.empty())
            {
                // TODO use device path instead of friendly name as the UniqueId
                list_webcam_info.push_back({webcam_name, UniqueId{webcam_name}, available_resolutions});
            }
        }
        VariantClear(&webcam_name_wstr);
        pPropBag->Release();
        pMoniker->Release();
    }

    return list_webcam_info;
}

static auto enumerate_devices(REFGUID category, IEnumMoniker** ppEnum) -> HRESULT
{
    // Create the System Device Enumerator.
    ICreateDevEnum* pDevEnum; // NOLINT(*-init-variables)
    HRESULT         hr = CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDevEnum));

    if (SUCCEEDED(hr))
    {
        // Create an enumerator for the category.
        hr = pDevEnum->CreateClassEnumerator(category, ppEnum, 0);
        if (hr == S_FALSE)
        {
            hr = VFW_E_NOT_FOUND; // The category is empty. Treat as an error.
        }
        pDevEnum->Release();
    }

    return hr;
}

auto grab_all_infos_impl() -> std::vector<Info>
{
    std::vector<Info> list_webcam_info{};
    HRESULT           hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr))
    {
        IEnumMoniker* pEnum; // NOLINT(*-init-variables)
        hr = enumerate_devices(CLSID_VideoInputDeviceCategory, &pEnum);
        if (SUCCEEDED(hr))
        {
            list_webcam_info = get_devices_info(pEnum);
            pEnum->Release();
        }
        CoUninitialize();
    }
    return list_webcam_info;
}

} // namespace wcam::internal

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#endif