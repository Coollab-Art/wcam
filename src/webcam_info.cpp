#include <vector>
#include <webcam_info/webcam_info.hpp>

#if defined(_WIN32)

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <winerror.h>
#include <codecvt>
#include <locale>

auto ConvertWCHARToString(const WCHAR* wstr) -> std::string
{
    // Créer un codecvt_utf8 pour la conversion
    std::wstring_convert<std::codecvt_utf8_utf16<WCHAR>, WCHAR> converter;
    // Convertir la chaîne wide (WCHAR*) en std::wstring
    std::wstring wtemp = wstr;
    // Convertir la std::wstring en std::string
    return converter.to_bytes(wtemp);
}

auto webcam_info::get_all_webcams() -> std::vector<info>
{
    std::vector<info> list_infos{};

    UINT32 count = 0;

    IMFAttributes* pConfig   = NULL;
    IMFActivate**  ppDevices = NULL;
    HRESULT        hr        = MFCreateAttributes(&pConfig, 1);

    // Request video capture devices.
    if (SUCCEEDED(hr))
    {
        hr = pConfig->SetGUID(
            MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
            MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID
        );
    }

    // Enumerate the devices,
    if (SUCCEEDED(hr))
    {
        hr = MFEnumDeviceSources(pConfig, &ppDevices, &count);
    }
    if (SUCCEEDED(hr))
    {
        for (int i = 0; i < count; i++)
        {
            list_infos.push_back(info{"default name", 0, 0});

            WCHAR* szFriendlyName = nullptr;
            UINT32 cchName        = 0;
            hr                    = ppDevices[i]->GetAllocatedString(
                MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME,
                &szFriendlyName, &cchName
            );

            if (SUCCEEDED(hr))
            {
                list_infos.back().name = ConvertWCHARToString(szFriendlyName);
            }

            // Now get the resolution
            IMFMediaSource* pMediaSource = nullptr;
            hr                           = ppDevices[i]->ActivateObject(IID_PPV_ARGS(&pMediaSource));
            if (SUCCEEDED(hr))
            {
                // Créer le lecteur source pour la source de média
                IMFSourceReader* pSourceReader = nullptr;
                hr                             = MFCreateSourceReaderFromMediaSource(pMediaSource, pConfig, &pSourceReader);
                if (SUCCEEDED(hr))
                {
                    // Obtenir le premier flux vidéo disponible
                    IMFMediaType* pMediaType = nullptr;
                    hr                       = pSourceReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pMediaType);
                    if (SUCCEEDED(hr))
                    {
                        UINT32 width  = 0;
                        UINT32 height = 0;
                        hr            = MFGetAttributeSize(pMediaType, MF_MT_FRAME_SIZE, &width, &height);
                        // hr = MFGetAttributeRatio(pConfig, MF_MT_PIXEL_ASPECT_RATIO, &width, &height);
                        if (SUCCEEDED(hr))
                        {
                            list_infos.back().width  = width;
                            list_infos.back().height = height;
                        }

                        pMediaType->Release();
                    }

                    pSourceReader->Release();
                }

                pMediaSource->Release();
            }
        }
    }
    return list_infos;
}

#endif