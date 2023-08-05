#include <algorithm>
#include <webcam_info/webcam_info.hpp>

namespace webcam_info {

static auto grab_all_webcams_infos_impl() -> std::vector<Info>;

auto grab_all_webcams_infos() -> std::vector<Info>
{
    auto list_webcams_infos = grab_all_webcams_infos_impl();
    for (auto& webcam_info : list_webcams_infos)
    {
        auto& resolutions = webcam_info.available_resolutions;
        std::sort(resolutions.begin(), resolutions.end(), [](Resolution const& res_a, Resolution const& res_b) {
            return res_a.width > res_b.width
                   || (res_a.width == res_b.width && res_a.height > res_b.height);
        });
        resolutions.erase(std::unique(resolutions.begin(), resolutions.end()), resolutions.end());
    }
    return list_webcams_infos;
}

} // namespace webcam_info

#if defined(_WIN32)

#if defined(GCC) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wlanguage-extension-token"
#endif

#include <dshow.h>
#include <array>
#include <clocale>
#include <cstdlib>

static auto convert_wstr_to_str(std::wstring const& wstr) -> std::string
{
    std::string res;
    res.reserve(wstr.size());
    for (wchar_t const c : wstr)
        res.push_back(static_cast<char>(c)); // TODO Support proper unicode strings
    return res;
}

static auto get_video_parameters(IBaseFilter* pCaptureFilter) -> std::vector<webcam_info::Resolution>
{
    std::vector<webcam_info::Resolution> available_resolutions;

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
                                    available_resolutions.push_back({pVih->bmiHeader.biWidth, pVih->bmiHeader.biHeight});
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

static auto get_devices_info(IEnumMoniker* pEnum) -> std::vector<webcam_info::Info>
{
    std::vector<webcam_info::Info> list_webcam_info{};

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
        VARIANT webcam_name;
        VariantInit(&webcam_name);
        hr = pPropBag->Read(L"Description", &webcam_name, nullptr);
        if (FAILED(hr))
        {
            hr = pPropBag->Read(L"FriendlyName", &webcam_name, nullptr);
        }
        if (SUCCEEDED(hr))
        {
            IBaseFilter* pCaptureFilter; // NOLINT(*-init-variables)
            hr = pMoniker->BindToObject(nullptr, nullptr, IID_PPV_ARGS(&pCaptureFilter));
            if (SUCCEEDED(hr))
            {
                std::vector<webcam_info::Resolution> available_resolutions = get_video_parameters(pCaptureFilter);
                pCaptureFilter->Release();

                if (!available_resolutions.empty())
                {
                    list_webcam_info.push_back({convert_wstr_to_str(webcam_name.bstrVal), available_resolutions});
                }
            }
        }
        VariantClear(&webcam_name);
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

static auto webcam_info::grab_all_webcams_infos_impl() -> std::vector<Info>
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

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#endif

#if defined(__linux__)

#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <algorithm>
#include <cstdio>

static auto webcam_info::grab_all_webcams_infos_impl() -> std::vector<info>
{
    std::vector<info> list_webcam_info{};

    std::vector<std::string> list_camera_path{};
    std::string              camera_path = "/dev/video0";
    for (const auto& entry : std::filesystem::directory_iterator("/dev"))
    {
        if (entry.path().string().find("video") == std::string::npos)
            continue;

        int video_device = open(entry.path().c_str(), O_RDONLY);
        if (video_device == -1)
            continue;

        v4l2_capability cap{};

        if (ioctl(video_device, VIDIOC_QUERYCAP, &cap) == -1)
        {
            std::cout << "Erreur lors de l'obtention des informations du périphérique";
            continue;
        }

        // Vérification si le périphérique est capable de capturer des flux vidéo
        if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
        {
            std::cout << "Le périphérique n'est pas capable de capturer des flux vidéo\n";
            continue;
        }

        // Récupération du nom du périphérique
        char deviceName[256];
        strcpy(deviceName, (char*)cap.card);

        std::vector<webcam_info::resolution> available_resolutions{};
        pixel_format                         format{};

        v4l2_fmtdesc formatDescription{};
        formatDescription.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        while (ioctl(video_device, VIDIOC_ENUM_FMT, &formatDescription) == 0)
        {
            printf("  - Format : %s\n", formatDescription.description);
            // Récupérer les résolutions associées à chaque format
            v4l2_frmsizeenum frameSize{};
            frameSize.pixel_format = formatDescription.pixelformat;
            while (ioctl(video_device, VIDIOC_ENUM_FRAMESIZES, &frameSize) == 0)
            {
                v4l2_frmivalenum frameInterval{};
                frameInterval.pixel_format = formatDescription.pixelformat;
                frameInterval.width        = frameSize.discrete.width;
                frameInterval.height       = frameSize.discrete.height;

                while (ioctl(video_device, VIDIOC_ENUM_FRAMEINTERVALS, &frameInterval) == 0)
                {
                    if (frameInterval.type == V4L2_FRMIVAL_TYPE_DISCRETE)
                    {
                        // float fps = 1.0f * static_cast<float>(frameInterval.discrete.denominator) / static_cast<float>(frameInterval.discrete.numerator);
                        if (/*fps > 29. &&*/ frameSize.type == V4L2_FRMSIZE_TYPE_DISCRETE)
                        {
                            available_resolutions.push_back({width, height});
                            std::string format_name(reinterpret_cast<char*>(formatDescription.description), 32);

                            if (format_name.find("Motion-JPEG") != std::string::npos)
                                format = pixel_format::mjpeg;

                            else if (format_name.find("YUYV") != std::string::npos)
                                format = pixel_format::yuyv;
                            else
                                format = pixel_format::unknown;
                        }
                    }
                    frameInterval.index++;
                }
                frameSize.index++;
            }

            formatDescription.index++;
        }
        if (width <= 0 || height <= 0)
            continue;
        list_webcam_info.push_back(info{std::string(deviceName), available_resolutions, format});
    }
    return list_webcam_info;
}

#endif