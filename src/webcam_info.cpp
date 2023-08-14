#include <algorithm>
#include <webcam_info/webcam_info.hpp>

namespace webcam_info {

auto grab_all_webcams_infos_impl() -> std::vector<Info>;

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
#include <cstdlib>
#include <unordered_map>

namespace webcam_info {

static auto convert_wstr_to_str(std::wstring const& wstr) -> std::string
{
    std::string res;
    res.reserve(wstr.size());
    for (wchar_t const c : wstr)
        res.push_back(static_cast<char>(c)); // TODO Support proper unicode strings
    return res;
}

static auto get_video_parameters(IBaseFilter* pCaptureFilter) -> std::vector<Resolution>
{
    std::vector<Resolution> available_resolutions;

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

static auto get_devices_info(IEnumMoniker* pEnum) -> std::vector<Info>
{
    thread_local auto resolutions_cache = std::unordered_map<std::string, std::vector<Resolution>>{}; // This cache limits the number of times we will allocate IBaseFilter which seems to leak because of a Windows bug.

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
        hr = pPropBag->Read(L"Description", &webcam_name_wstr, nullptr);
        if (FAILED(hr))
        {
            hr = pPropBag->Read(L"FriendlyName", &webcam_name_wstr, nullptr);
        }
        if (SUCCEEDED(hr))
        {
            auto       available_resolutions = std::vector<Resolution>{};
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
                list_webcam_info.push_back({webcam_name, available_resolutions});
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

auto webcam_info::grab_all_webcams_infos_impl() -> std::vector<Info>
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

} // namespace webcam_info

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#endif

#if defined(__linux__)

#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <filesystem>

static auto find_available_resolutions(int const video_device) -> std::vector<webcam_info::Resolution>
{
    std::vector<webcam_info::Resolution> available_resolutions;

    v4l2_fmtdesc format_description{};
    format_description.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    while (ioctl(video_device, VIDIOC_ENUM_FMT, &format_description) == 0)
    {
        v4l2_frmsizeenum frame_size{};
        frame_size.pixel_format = format_description.pixelformat;
        while (ioctl(video_device, VIDIOC_ENUM_FRAMESIZES, &frame_size) == 0)
        {
            v4l2_frmivalenum frame_interval{};
            frame_interval.pixel_format = format_description.pixelformat;
            frame_interval.width        = frame_size.discrete.width;
            frame_interval.height       = frame_size.discrete.height;

            while (ioctl(video_device, VIDIOC_ENUM_FRAMEINTERVALS, &frame_interval) == 0)
            {
                if (frame_interval.type == V4L2_FRMIVAL_TYPE_DISCRETE)
                {
                    // float fps = static_cast<float>(frameInterval.discrete.denominator) / static_cast<float>(frameInterval.discrete.numerator);
                    if (/*fps > 29. &&*/ frame_size.type == V4L2_FRMSIZE_TYPE_DISCRETE)
                    {
                        available_resolutions.push_back({static_cast<int>(frame_interval.width), static_cast<int>(frame_interval.height)});
                    }
                }
                frame_interval.index++;
            }
            frame_size.index++;
        }

        format_description.index++;
    }

    return available_resolutions;
}

auto webcam_info::grab_all_webcams_infos_impl() -> std::vector<Info>
{
    std::vector<Info> list_webcam_info{};

    for (auto const& entry : std::filesystem::directory_iterator("/dev"))
    {
        if (entry.path().string().find("video") == std::string::npos)
            continue;

        int const video_device = open(entry.path().c_str(), O_RDONLY);
        if (video_device == -1)
            continue;

        v4l2_capability cap{};
        if (ioctl(video_device, VIDIOC_QUERYCAP, &cap) == -1)
            continue;

        if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
            continue;

        std::string const webcam_name = reinterpret_cast<char const*>(cap.card); // NOLINT(*-pro-type-reinterpret-cast)

        std::vector<webcam_info::Resolution> const available_resolutions = find_available_resolutions(video_device);
        if (available_resolutions.empty())
            continue;

        list_webcam_info.push_back({webcam_name, available_resolutions});
    }

    return list_webcam_info;
}

#endif