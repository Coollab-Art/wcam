#if defined(_WIN32)
#include "wcam_windows.hpp"
#include <dshow.h>
#include <cstdlib>
#include <format>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include "wcam/wcam.hpp"

/// NB: we use DirectShow and not MediaFoundation
/// because OBS Virtual Camera only works with DirectShow
/// https://github.com/obsproject/obs-studio/issues/8057
/// Apparently Windows 11 adds this capability (https://medium.com/deelvin-machine-learning/how-does-obs-virtual-camera-plugin-work-on-windows-e92ab8986c4e)
/// So in a very distant future, when Windows 11 is on 99.999% of the machines, and when OBS implements a MediaFoundation backend and a virtual camera for it, then we can switch to MediaFoundation

namespace wcam::internal {
using namespace std; // TODO remove

auto convert_wstr_to_str(BSTR const& wstr) -> std::string
{
    int const wstr_len = static_cast<int>(SysStringLen(wstr));
    // Determine the size of the resulting string
    int const res_len = WideCharToMultiByte(CP_UTF8, 0, wstr, wstr_len, nullptr, 0, nullptr, nullptr);
    if (res_len == 0)
        return "";

    // Allocate the necessary buffer
    auto res = std::string(res_len, 0);

    // Perform the conversion
    WideCharToMultiByte(CP_UTF8, 0, wstr, wstr_len, res.data(), res_len, nullptr, nullptr);
    return res;
}

static auto hr2err(HRESULT hr) -> std::string
{
    char* error_message{nullptr};
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        hr,
        MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
        reinterpret_cast<LPSTR>(&error_message), // NOLINT(*reinterpret-cast)
        0,
        nullptr
    );
    auto message = std::string{error_message};
    LocalFree(error_message);
    return message;
}

static void throw_error(HRESULT hr, std::string_view code_that_failed, std::source_location location = std::source_location::current())
{
    throw std::runtime_error{std::format("{}(During `{}`, at {}({}:{}))", hr2err(hr), code_that_failed, location.file_name(), location.line(), location.column())};
}

#define THROW_IF_ERR(exp) /*NOLINT(*macro*)*/ \
    {                                         \
        HRESULT hresult = exp;                \
        if (FAILED(hresult))                  \
            throw_error(hresult, #exp);       \
    }
#define THROW_IF_ERR2(exp, location) /*NOLINT(*macro*)*/ \
    {                                                    \
        HRESULT hresult = exp;                           \
        if (FAILED(hresult))                             \
            throw_error(hresult, #exp, location);        \
    }

template<typename T>
class AutoRelease {
public:
    AutoRelease() = default;
    explicit AutoRelease(REFCLSID class_id, std::source_location location = std::source_location::current())
    {
        THROW_IF_ERR2(CoCreateInstance(class_id, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&_ptr)), location);
    }

    ~AutoRelease()
    {
        if (_ptr != nullptr)
            _ptr->Release();
    }

    AutoRelease(AutoRelease const&)                = delete;
    AutoRelease& operator=(AutoRelease const&)     = delete;
    AutoRelease(AutoRelease&&) noexcept            = delete;
    AutoRelease& operator=(AutoRelease&&) noexcept = delete;

    auto operator->() -> T* { return _ptr; }
    auto operator->() const -> T const* { return _ptr; }
    auto operator*() -> T& { return *_ptr; }
    auto operator*() const -> T const& { return *_ptr; }
    operator T*() { return _ptr; } // NOLINT(*explicit*)
    T** operator&()                // NOLINT(*runtime-operator)
    {
        return &_ptr;
    }

private:
    T* _ptr{nullptr};
};

static void CoInitializeIFN()
{
    struct Raii { // NOLINT(*special-member-functions)
        Raii() { THROW_IF_ERR(CoInitializeEx(nullptr, COINIT_MULTITHREADED)); }
        ~Raii() { CoUninitialize(); }
    };
    thread_local Raii instance{}; // Each thread needs to call CoInitializeEx once
}

STDMETHODIMP_(ULONG)
CaptureImpl::AddRef()
{
    return InterlockedIncrement(&_ref_count);
}

STDMETHODIMP_(ULONG)
CaptureImpl::Release()
{
    return InterlockedDecrement(&_ref_count);
}

STDMETHODIMP CaptureImpl::QueryInterface(REFIID riid, void** ppv)
{
    if (riid == IID_ISampleGrabberCB || riid == IID_IUnknown)
    {
        *ppv = reinterpret_cast<void*>(this); // NOLINT(*reinterpret-cast*)
        return NOERROR;
    }
    return E_NOINTERFACE;
}

auto find_webcam_name(IMoniker* pMoniker) -> std::string
{
    auto pPropBag = AutoRelease<IPropertyBag>{};
    THROW_IF_ERR(pMoniker->BindToStorage(nullptr, nullptr, IID_PPV_ARGS(&pPropBag))); // TODO should we continue the loop if there is an error here ?

    VARIANT webcam_name_wstr;
    VariantInit(&webcam_name_wstr);
    HRESULT hr = pPropBag->Read(L"FriendlyName", &webcam_name_wstr, nullptr); // TODO what happens if friendly name is missing ?
    // if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    // {
    //     hr = pPropBag->Read(L"DevicePath", &webcam_name_wstr, nullptr);
    // }
    if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        return "Unnamed webcam";
    }
    auto res = convert_wstr_to_str(webcam_name_wstr.bstrVal);
    THROW_IF_ERR(VariantClear(&webcam_name_wstr)); // TODO should we throw here ? // TODO must clear before the early return above
    return res;
}

auto find_webcam_id(IMoniker* pMoniker) -> UniqueId
{
    auto pPropBag = AutoRelease<IPropertyBag>{};
    THROW_IF_ERR(pMoniker->BindToStorage(nullptr, nullptr, IID_PPV_ARGS(&pPropBag)));

    VARIANT webcam_name_wstr;
    VariantInit(&webcam_name_wstr);
    HRESULT hr = pPropBag->Read(L"DevicePath", &webcam_name_wstr, nullptr);
    if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
    {
        return UniqueId{find_webcam_name(pMoniker)};
    }
    auto res = convert_wstr_to_str(webcam_name_wstr.bstrVal);
    THROW_IF_ERR(VariantClear(&webcam_name_wstr)); // TODO should we throw here ?
    return UniqueId{res};
}

CaptureImpl::CaptureImpl(UniqueId const& unique_id, img::Size const& requested_resolution)
{
    CoInitializeIFN();

    auto pBuild = AutoRelease<ICaptureGraphBuilder2>{CLSID_CaptureGraphBuilder2};
    auto pGraph = AutoRelease<IGraphBuilder>{CLSID_FilterGraph};
    THROW_IF_ERR(pBuild->SetFiltergraph(pGraph));

    // Obtenir l'objet Moniker correspondant au périphérique sélectionné
    AutoRelease<ICreateDevEnum> pDevEnum{CLSID_SystemDeviceEnum};

    auto pEnum = AutoRelease<IEnumMoniker>{};
    THROW_IF_ERR(pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, 0));

    auto pMoniker = AutoRelease<IMoniker>{};
    while (pEnum->Next(1, &pMoniker, NULL) == S_OK)
    {
        if (find_webcam_id(pMoniker) == unique_id)
            break;
    }
    // TODO tell the moniker which resolution to use
    // Liaison au filtre de capture du périphérique sélectionné

    IBaseFilter* pCap = nullptr;
    THROW_IF_ERR(pMoniker->BindToObject(0, 0, IID_IBaseFilter, (void**)&pCap));

    // 3. Add the Webcam Filter to the Graph

    THROW_IF_ERR(pGraph->AddFilter(pCap, L"CaptureFilter"));

    // 4. Add and Configure the Sample Grabber

    AutoRelease<IBaseFilter>    pSampleGrabberFilter{CLSID_SampleGrabber};
    AutoRelease<ISampleGrabber> pSampleGrabber{};
    THROW_IF_ERR(pSampleGrabberFilter->QueryInterface(IID_ISampleGrabber, (void**)&pSampleGrabber));

    // Configure the sample grabber
    AM_MEDIA_TYPE mt;
    ZeroMemory(&mt, sizeof(AM_MEDIA_TYPE));
    mt.majortype = MEDIATYPE_Video;
    mt.subtype   = MEDIASUBTYPE_RGB24; // Or any other format you prefer
    THROW_IF_ERR(pSampleGrabber->SetMediaType(&mt));
    THROW_IF_ERR(pSampleGrabber->SetOneShot(FALSE));
    THROW_IF_ERR(pSampleGrabber->SetBufferSamples(TRUE));

    // Add the sample grabber to the graph
    THROW_IF_ERR(pGraph->AddFilter(pSampleGrabberFilter, L"Sample Grabber"));

    // 5. Render the Stream
    AutoRelease<IBaseFilter> pNullRenderer{CLSID_NullRenderer};
    THROW_IF_ERR(pGraph->AddFilter(pNullRenderer, L"Null Renderer"));

    THROW_IF_ERR(pBuild->RenderStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, pCap, pSampleGrabberFilter, pNullRenderer));
    // 6. Retrieve the Video Information Header

    AM_MEDIA_TYPE mtGrabbed;
    THROW_IF_ERR(pSampleGrabber->GetConnectedMediaType(&mtGrabbed));

    VIDEOINFOHEADER* pVih = (VIDEOINFOHEADER*)mtGrabbed.pbFormat;
    _resolution           = img::Size{
        static_cast<img::Size::DataType>(pVih->bmiHeader.biWidth),
        static_cast<img::Size::DataType>(pVih->bmiHeader.biHeight),
    };

    assert(pVih->bmiHeader.biSizeImage == _resolution.width() * _resolution.height() * 3);

    // 7. Start the Graph

    // IMediaControl *pControl = NULL;
    THROW_IF_ERR(pGraph->QueryInterface(IID_IMediaControl, (void**)&_media_control));

    // 8. Implement ISampleGrabberCB Interface

    THROW_IF_ERR(_media_control->Run());

    // Create an instance of the callback
    // _sgCallback = SampleGrabberCallback{resolution};

    THROW_IF_ERR(pSampleGrabber->SetCallback(this, 1));
}

CaptureImpl::~CaptureImpl()
{
    _media_control->Stop();
    _media_control->Release();
}

// #if defined(GCC) || defined(__clang__)
// #pragma GCC diagnostic push
// #pragma GCC diagnostic ignored "-Wlanguage-extension-token"
// #endif

static auto get_video_parameters(IBaseFilter* pCaptureFilter) -> std::vector<img::Size>
{
    auto available_resolutions = std::vector<img::Size>{};

    auto pEnumPins = AutoRelease<IEnumPins>{};
    THROW_IF_ERR(pCaptureFilter->EnumPins(&pEnumPins));
    while (true)
    {
        auto pPin = AutoRelease<IPin>{}; // Declare pPin inside the loop so that it is freed at the end, Next doesn't Release the pin that you pass
        if (pEnumPins->Next(1, &pPin, nullptr) != S_OK)
            break;
        PIN_DIRECTION pinDirection; // NOLINT(*-init-variables)
        pPin->QueryDirection(&pinDirection);

        if (pinDirection != PINDIR_OUTPUT)
            continue;
        AutoRelease<IAMStreamConfig> pStreamConfig; // NOLINT(*-init-variables)
        THROW_IF_ERR(pPin->QueryInterface(IID_PPV_ARGS(&pStreamConfig)));
        int iCount; // NOLINT(*-init-variables)
        int iSize;  // NOLINT(*-init-variables)
        THROW_IF_ERR(pStreamConfig->GetNumberOfCapabilities(&iCount, &iSize));
        VIDEO_STREAM_CONFIG_CAPS caps;
        for (int i = 0; i < iCount; i++)
        {
            AM_MEDIA_TYPE* pmtConfig;                                                                  // NOLINT(*-init-variables)
            THROW_IF_ERR(pStreamConfig->GetStreamCaps(i, &pmtConfig, reinterpret_cast<BYTE*>(&caps))); // NOLINT(*-pro-type-reinterpret-cast)
            if (pmtConfig->formattype != FORMAT_VideoInfo)
                continue;
            auto* pVih = reinterpret_cast<VIDEOINFOHEADER*>(pmtConfig->pbFormat); // NOLINT(*-pro-type-reinterpret-cast)
            available_resolutions.push_back({static_cast<img::Size::DataType>(pVih->bmiHeader.biWidth), static_cast<img::Size::DataType>(pVih->bmiHeader.biHeight)});
        }
    }

    return available_resolutions;
}

auto grab_all_infos_impl() -> std::vector<Info>
{
    CoInitializeIFN();

    auto pDevEnum = AutoRelease<ICreateDevEnum>{CLSID_SystemDeviceEnum};
    auto pEnum    = AutoRelease<IEnumMoniker>{};
    THROW_IF_ERR(pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, 0));
    if (pEnum == nullptr) // Might still be nullptr after CreateClassEnumerator if the VideoInputDevice category is empty or missing (https://learn.microsoft.com/en-us/previous-versions/ms784969(v=vs.85))
        return {};

    thread_local auto resolutions_cache = std::unordered_map<std::string, std::vector<img::Size>>{}; // This cache limits the number of times we will allocate IBaseFilter which seems to leak because of a Windows bug.

    auto infos = std::vector<Info>{};

    while (true)
    {
        auto pMoniker = AutoRelease<IMoniker>{};
        if (pEnum->Next(1, &pMoniker, nullptr) != S_OK)
            break;

        // }
        // if (SUCCEEDED(hr))
        {
            auto       available_resolutions = std::vector<img::Size>{};
            auto const webcam_name           = find_webcam_name(pMoniker);
            auto const it                    = resolutions_cache.find(webcam_name);
            if (it != resolutions_cache.end())
            {
                available_resolutions = it->second;
            }
            else
            {
                auto pCaptureFilter = AutoRelease<IBaseFilter>{};
                THROW_IF_ERR(pMoniker->BindToObject(nullptr, nullptr, IID_PPV_ARGS(&pCaptureFilter)));
                available_resolutions          = get_video_parameters(pCaptureFilter);
                resolutions_cache[webcam_name] = available_resolutions;
            }
            if (!available_resolutions.empty())
            {
                infos.push_back({webcam_name, find_webcam_id(pMoniker), available_resolutions});
            }
        }
        // else
        // {
        //     throw 0;
        // }
    }

    return infos;
}

} // namespace wcam::internal

// #if defined(__GNUC__) || defined(__clang__)
// #pragma GCC diagnostic pop
// #endif

#endif