#include <vector>
#include <webcam_info/webcam_info.hpp>

#if defined(_WIN32)

#include <dshow.h>
#include <codecvt>

std::string ConvertWCharToString(const wchar_t* wcharStr)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.to_bytes(wcharStr);
}

HRESULT GetVideoResolution(IBaseFilter* pCaptureFilter, int& width, int& height)
{
    HRESULT        hr         = S_OK;
    IEnumPins*     pEnumPins  = NULL;
    IPin*          pPin       = NULL;
    AM_MEDIA_TYPE* pMediaType = NULL;

    // Trouver la première sortie vidéo du filtre d'entrée
    hr = pCaptureFilter->EnumPins(&pEnumPins);
    if (SUCCEEDED(hr))
    {
        while (pEnumPins->Next(1, &pPin, NULL) == S_OK)
        {
            PIN_DIRECTION pinDirection;
            pPin->QueryDirection(&pinDirection);

            if (pinDirection == PINDIR_OUTPUT)
            {
                // Obtenir l'interface IAMStreamConfig pour manipuler les paramètres de capture
                IAMStreamConfig* pStreamConfig = NULL;
                hr                             = pPin->QueryInterface(IID_PPV_ARGS(&pStreamConfig));
                if (SUCCEEDED(hr))
                {
                    int iCount = 0, iSize = 0;
                    hr = pStreamConfig->GetNumberOfCapabilities(&iCount, &iSize);
                    if (SUCCEEDED(hr))
                    {
                        // Obtenir les capacités de capture (résolutions)
                        VIDEO_STREAM_CONFIG_CAPS caps;
                        for (int i = 0; i < iCount; i++)
                        {
                            AM_MEDIA_TYPE* pmtConfig;
                            hr = pStreamConfig->GetStreamCaps(i, &pmtConfig, (BYTE*)&caps);
                            if (SUCCEEDED(hr))
                            {
                                if (pmtConfig->formattype == FORMAT_VideoInfo)
                                {
                                    VIDEOINFOHEADER* pVih = reinterpret_cast<VIDEOINFOHEADER*>(pmtConfig->pbFormat);
                                    width                 = max(width, pVih->bmiHeader.biWidth);
                                    height                = max(height, pVih->bmiHeader.biHeight);
                                    // break;
                                    int a;
                                }
                            }
                        }
                    }
                    pStreamConfig->Release();
                }
                pPin->Release();
                break;
            }
            pPin->Release();
        }
        pEnumPins->Release();
    }

    return hr;
}

auto get_devices_info(IEnumMoniker* pEnum) -> std::vector<webcam_info::info>
{
    std::vector<webcam_info::info> list_webcam_info{};
    IMoniker*                      pMoniker = NULL;

    while (pEnum->Next(1, &pMoniker, NULL) == S_OK)
    {
        int width{};
        int height{};

        IPropertyBag* pPropBag;
        HRESULT       hr = pMoniker->BindToStorage(0, 0, IID_PPV_ARGS(&pPropBag));
        if (FAILED(hr))
        {
            pMoniker->Release();
            continue;
        }

        VARIANT var;
        VariantInit(&var);

        // Get description or friendly name.
        hr = pPropBag->Read(L"Description", &var, 0);
        if (FAILED(hr))
        {
            hr = pPropBag->Read(L"FriendlyName", &var, 0);
        }
        if (SUCCEEDED(hr))
        {
            IBaseFilter* pCaptureFilter = NULL;
            hr                          = pMoniker->BindToObject(0, 0, IID_PPV_ARGS(&pCaptureFilter));
            if (SUCCEEDED(hr))
            {
                hr = GetVideoResolution(pCaptureFilter, width, height);
                pCaptureFilter->Release();

                if (SUCCEEDED(hr))
                {
                    list_webcam_info.push_back(webcam_info::info{ConvertWCharToString(var.bstrVal), width, height});
                }

                // printf("%S\n", var.bstrVal);

                VariantClear(&var);
            }

            hr = pPropBag->Write(L"FriendlyName", &var);

            pPropBag->Release();
            pMoniker->Release();
        }
    }
    return list_webcam_info;
}

auto EnumerateDevices(REFGUID category, IEnumMoniker** ppEnum) -> HRESULT
{
    // Create the System Device Enumerator.
    ICreateDevEnum* pDevEnum;
    HRESULT         hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDevEnum));

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

auto webcam_info::get_all_webcams() -> std::vector<info>
{
    std::vector<info> list_webcam_info{};
    HRESULT           hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr))
    {
        IEnumMoniker* pEnum;

        hr = EnumerateDevices(CLSID_VideoInputDeviceCategory, &pEnum);
        if (SUCCEEDED(hr))
        {
            list_webcam_info = get_devices_info(pEnum);
            pEnum->Release();
        }
        CoUninitialize();
    }
    return list_webcam_info;
}

#endif
