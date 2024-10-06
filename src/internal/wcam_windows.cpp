#if defined(_WIN32)
#include "wcam_windows.hpp"
#include <cstdlib>
#include <format>
#include <source_location>
#include <string>
#include <string_view>
#include <unordered_map>
#include "../Info.hpp"
#include "ImageFactory.hpp"
#include "make_device_id.hpp"

/// NB: we use DirectShow and not MediaFoundation
/// because OBS Virtual Camera only works with DirectShow
/// https://github.com/obsproject/obs-studio/issues/8057
/// Apparently Windows 11 adds this capability (https://medium.com/deelvin-machine-learning/how-does-obs-virtual-camera-plugin-work-on-windows-e92ab8986c4e)
/// So in a very distant future, when Windows 11 is on 99.999% of the machines, and when OBS implements a MediaFoundation backend and a virtual camera for it, then we can switch to MediaFoundation

namespace wcam::internal {

static auto convert_wstr_to_str(BSTR const& wstr) -> std::string
{
    int const wstr_len = static_cast<int>(SysStringLen(wstr));
    int const res_len  = WideCharToMultiByte(CP_UTF8, 0, wstr, wstr_len, nullptr, 0, nullptr, nullptr);
    if (res_len == 0)
        return "";

    auto res = std::string(res_len, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr, wstr_len, res.data(), res_len, nullptr, nullptr);
    return res;
}

static auto hr_to_string(HRESULT hr) -> std::string
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
    throw CaptureException{Error_Unknown{std::format("{}(During `{}`, at {}({}:{}))", hr_to_string(hr), code_that_failed, location.file_name(), location.line(), location.column())}};
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
#define ASSERT_AND_RETURN_IF_ERR(exp, default_value) /*NOLINT(*macro*)*/ \
    {                                                                    \
        HRESULT hresult = exp;                                           \
        if (FAILED(hresult))                                             \
        {                                                                \
            assert(false);                                               \
            return default_value;                                        \
        }                                                                \
    }

template<typename T>
class AutoRelease {
public:
    AutoRelease() = default;
    explicit AutoRelease(T* ptr)
        : _ptr{ptr}
    {}
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

class VariantRAII {
public:
    VariantRAII()
    {
        VariantInit(&_variant);
    }
    ~VariantRAII()
    {
        assert(VariantClear(&_variant) == S_OK);
    }
    VariantRAII(VariantRAII const&)                = delete;
    VariantRAII& operator=(VariantRAII const&)     = delete;
    VariantRAII(VariantRAII&&) noexcept            = delete;
    VariantRAII& operator=(VariantRAII&&) noexcept = delete;

    auto operator->() -> VARIANT* { return &_variant; }
    auto operator->() const -> VARIANT const* { return &_variant; }
    operator VARIANT() { return _variant; } // NOLINT(*explicit*)
    VARIANT* operator&()                    // NOLINT(*runtime-operator)
    {
        return &_variant;
    }

private:
    VARIANT _variant{};
};

class MediaTypeRAII {
public:
    MediaTypeRAII() = default;
    ~MediaTypeRAII()
    {
        if (_media_type == nullptr)
            return;

        if (_media_type->cbFormat != 0)
        {
            CoTaskMemFree((PVOID)_media_type->pbFormat); // NOLINT(*readability-casting)
            _media_type->cbFormat = 0;
            _media_type->pbFormat = nullptr;
        }
        if (_media_type->pUnk != nullptr)
        {
            _media_type->pUnk->Release();
            _media_type->pUnk = nullptr;
        }
        CoTaskMemFree(_media_type);
    }
    MediaTypeRAII(MediaTypeRAII const&)                = delete;
    MediaTypeRAII& operator=(MediaTypeRAII const&)     = delete;
    MediaTypeRAII(MediaTypeRAII&&) noexcept            = delete;
    MediaTypeRAII& operator=(MediaTypeRAII&&) noexcept = delete;

    operator AM_MEDIA_TYPE*() { return _media_type; } // NOLINT(*explicit*)
    auto            operator->() -> AM_MEDIA_TYPE* { return _media_type; }
    auto            operator->() const -> AM_MEDIA_TYPE const* { return _media_type; }
    AM_MEDIA_TYPE** operator&() // NOLINT(*runtime-operator)
    {
        return &_media_type;
    }

private:
    AM_MEDIA_TYPE* _media_type{};
};

// clang-format off
STDMETHODIMP_(ULONG) CaptureImpl::AddRef() { return InterlockedIncrement(&_ref_count); }
STDMETHODIMP_(ULONG) CaptureImpl::Release() { return InterlockedDecrement(&_ref_count); }
// clang-format on

STDMETHODIMP CaptureImpl::QueryInterface(REFIID riid, void** ppv)
{
    if (riid == IID_ISampleGrabberCB || riid == IID_IUnknown)
    {
        *ppv = reinterpret_cast<void*>(this); // NOLINT(*reinterpret-cast*)
        return NOERROR;
    }
    return E_NOINTERFACE;
}

static void CoInitializeIFN()
{
    struct Raii { // NOLINT(*special-member-functions)
        Raii() { THROW_IF_ERR(CoInitializeEx(nullptr, COINIT_MULTITHREADED)); }
        ~Raii() { CoUninitialize(); }
    };
    thread_local Raii instance{}; // Each thread needs to call CoInitializeEx once
}

static auto get_webcam_name(IMoniker* moniker) -> std::string
{
    auto prop_bag = AutoRelease<IPropertyBag>{};
    ASSERT_AND_RETURN_IF_ERR(moniker->BindToStorage(nullptr, nullptr, IID_PPV_ARGS(&prop_bag)), "Unnamed webcam");

    auto          webcam_name_wstr = VariantRAII{};
    HRESULT const hr               = prop_bag->Read(L"FriendlyName", &webcam_name_wstr, nullptr);
    if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
        return "Unnamed webcam";
    return convert_wstr_to_str(webcam_name_wstr->bstrVal);
}

static auto get_webcam_id(IMoniker* moniker) -> DeviceId
{
    auto prop_bag = AutoRelease<IPropertyBag>{};
    ASSERT_AND_RETURN_IF_ERR(moniker->BindToStorage(nullptr, nullptr, IID_PPV_ARGS(&prop_bag)), make_device_id(get_webcam_name(moniker)));

    auto          device_path = VariantRAII{};
    HRESULT const hr          = prop_bag->Read(L"DevicePath", &device_path, nullptr);
    if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) // It can happen, for example OBS Virtual Camera doesn't have a DevicePath
        return make_device_id(get_webcam_name(moniker));
    return make_device_id(convert_wstr_to_str(device_path->bstrVal));
}

static auto find_moniker(DeviceId const& device_id) -> IMoniker*
{
    auto dev_enum   = AutoRelease<ICreateDevEnum>{CLSID_SystemDeviceEnum};
    auto enumerator = AutoRelease<IEnumMoniker>{};
    THROW_IF_ERR(dev_enum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &enumerator, 0));

    IMoniker* moniker{};
    while (enumerator->Next(1, &moniker, nullptr) == S_OK)
    {
        if (get_webcam_id(moniker) == device_id)
            return moniker;
    }
    throw CaptureException{Error_WebcamUnplugged{}}; // Webcam not found
}

static void set_resolution(IAMStreamConfig* config, Resolution const& resolution)
{
    auto media_type = MediaTypeRAII{};
    THROW_IF_ERR(config->GetFormat(&media_type));

    // Change resolution
    reinterpret_cast<VIDEOINFOHEADER*>(media_type->pbFormat)->bmiHeader.biWidth  = static_cast<LONG>(resolution.width());  // NOLINT(*reinterpret-cast)
    reinterpret_cast<VIDEOINFOHEADER*>(media_type->pbFormat)->bmiHeader.biHeight = static_cast<LONG>(resolution.height()); // NOLINT(*reinterpret-cast)

    THROW_IF_ERR(config->SetFormat(media_type));
}

static auto select_video_format(DeviceId const& device_id) -> GUID
{
    if (device_id.as_string().find("OBS") != std::string::npos
        || device_id.as_string().find("Streamlabs") != std::string::npos)
    {
        // OBS Virtual Camera always returns S_OK on SetFormat(), even if it doesn't support the actual format.
        // So we have to choose a format that it supports manually, e.g. NV12.
        // See https://github.com/opencv/opencv/issues/19746#issuecomment-1383056787
        return MEDIASUBTYPE_NV12;
    }
    return MEDIASUBTYPE_RGB24;
}

void CaptureImpl::configure_sample_grabber(ISampleGrabber* sample_grabber)
{
    AM_MEDIA_TYPE media_type;
    ZeroMemory(&media_type, sizeof(AM_MEDIA_TYPE));
    media_type.majortype = MEDIATYPE_Video;
    media_type.subtype   = _video_format;
    THROW_IF_ERR(sample_grabber->SetMediaType(&media_type));
    THROW_IF_ERR(sample_grabber->SetOneShot(false));
    THROW_IF_ERR(sample_grabber->SetBufferSamples(false));
    THROW_IF_ERR(sample_grabber->SetCallback(this, 1));
}

static void tell_the_graph_to_process_samples_as_fast_as_possible(IGraphBuilder* graph)
{
    // Tell the graph to process the samples as fast as possible, instead of trying to sync to a clock (cf. https://learn.microsoft.com/en-us/windows/win32/api/strmif/nf-strmif-imediafilter-setsyncsource)
    auto media_filter = AutoRelease<IMediaFilter>{};
    THROW_IF_ERR(graph->QueryInterface(IID_IMediaFilter, (void**)&media_filter)); // NOLINT(*cstyle-cast)
    media_filter->SetSyncSource(nullptr);
}

static void throw_if_webcam_is_already_in_use(IGraphBuilder* graph)
{
    auto media_event = AutoRelease<IMediaEventEx>{};
    THROW_IF_ERR(graph->QueryInterface(IID_IMediaEventEx, (void**)&media_event)); // NOLINT(*cstyle-cast)

    long     event_code;     // NOLINT(*runtime-int, *init-variables)
    LONG_PTR param1, param2; // NOLINT(*isolate-declaration, *init-variables)
    while (S_OK == media_event->GetEvent(&event_code, &param1, &param2, 0))
    {
        bool const is_already_in_use = event_code == EC_ERRORABORT;
        media_event->FreeEventParams(event_code, param1, param2);
        if (is_already_in_use)
            throw CaptureException{Error_WebcamAlreadyUsedInAnotherApplication{}};
    }
}

static auto get_actual_resolution(ISampleGrabber* sample_grabber, GUID video_format) -> Resolution
{
    auto media_type = AM_MEDIA_TYPE{};
    THROW_IF_ERR(sample_grabber->GetConnectedMediaType(&media_type));

    auto* const video_info = reinterpret_cast<VIDEOINFOHEADER*>(media_type.pbFormat); // NOLINT(*reinterpret-cast)
    auto const  resolution = Resolution{
        static_cast<Resolution::DataType>(video_info->bmiHeader.biWidth),
        static_cast<Resolution::DataType>(video_info->bmiHeader.biHeight),
    };

    assert(
        (video_format == MEDIASUBTYPE_RGB24 && video_info->bmiHeader.biSizeImage == resolution.pixels_count() * 3)
        || (video_format == MEDIASUBTYPE_NV12 && video_info->bmiHeader.biSizeImage == resolution.pixels_count() * 3 / 2)
    );
    std::ignore = video_format; // Silence warning in release
    return resolution;
}

CaptureImpl::CaptureImpl(DeviceId const& device_id, Resolution const& requested_resolution)
    : _video_format{select_video_format(device_id)}
{
    CoInitializeIFN();

    auto builder = AutoRelease<ICaptureGraphBuilder2>{CLSID_CaptureGraphBuilder2};
    auto graph   = AutoRelease<IGraphBuilder>{CLSID_FilterGraph};
    THROW_IF_ERR(builder->SetFiltergraph(graph));

    auto moniker = AutoRelease<IMoniker>{find_moniker(device_id)};

    auto capture_filter = AutoRelease<IBaseFilter>{};
    THROW_IF_ERR(moniker->BindToObject(nullptr, nullptr, IID_IBaseFilter, (void**)&capture_filter)); // NOLINT(*cstyle-cast)
    THROW_IF_ERR(graph->AddFilter(capture_filter, L"CaptureFilter"));

    auto config = AutoRelease<IAMStreamConfig>{};
    THROW_IF_ERR(builder->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, capture_filter, IID_IAMStreamConfig, (void**)&config)); // NOLINT(*cstyle-cast)
    set_resolution(config, requested_resolution);

    auto sample_grabber_filter = AutoRelease<IBaseFilter>{CLSID_SampleGrabber};
    auto sample_grabber        = AutoRelease<ISampleGrabber>{};
    THROW_IF_ERR(sample_grabber_filter->QueryInterface(IID_ISampleGrabber, (void**)&sample_grabber)); // NOLINT(*cstyle-cast)
    configure_sample_grabber(sample_grabber);
    THROW_IF_ERR(graph->AddFilter(sample_grabber_filter, L"Sample Grabber"));

    tell_the_graph_to_process_samples_as_fast_as_possible(graph);

    { // Disable the default output window by using a Null Renderer
        auto null_renderer = AutoRelease<IBaseFilter>{CLSID_NullRenderer};
        THROW_IF_ERR(graph->AddFilter(null_renderer, L"Null Renderer"));
        THROW_IF_ERR(builder->RenderStream(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, capture_filter, sample_grabber_filter, null_renderer));
    }

    // Start the Graph
    THROW_IF_ERR(graph->QueryInterface(IID_IMediaControl, (void**)&_media_control)); // NOLINT(*cstyle-cast)
    THROW_IF_ERR(_media_control->Run());
    throw_if_webcam_is_already_in_use(graph); // Must be done after the graph is started

    _resolution = get_actual_resolution(sample_grabber, _video_format);
}

STDMETHODIMP CaptureImpl::BufferCB(double /* time */, BYTE* buffer, long buffer_length) // NOLINT(*runtime-int)
{
    auto image = image_factory().make_image();
    if (_video_format == MEDIASUBTYPE_RGB24)
    {
        image->set_data(ImageDataView<BGR24>{buffer, static_cast<size_t>(buffer_length), _resolution, wcam::FirstRowIs::Bottom});
    }
    else if (_video_format == MEDIASUBTYPE_NV12)
    {
        image->set_data(ImageDataView<NV12>{buffer, static_cast<size_t>(buffer_length), _resolution, wcam::FirstRowIs::Top});
    }
    else
    {
        assert(false && "Unsupported pixel format! Please contact the library authors to ask them to add this format.");
    }

    set_image(std::move(image));
    return S_OK;
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

static auto get_video_parameters(IBaseFilter* pCaptureFilter) -> std::vector<Resolution>
{
    auto available_resolutions = std::vector<Resolution>{};

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
            available_resolutions.push_back({static_cast<Resolution::DataType>(pVih->bmiHeader.biWidth), static_cast<Resolution::DataType>(pVih->bmiHeader.biHeight)});
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

    thread_local auto resolutions_cache = std::unordered_map<std::string, std::vector<Resolution>>{}; // This cache limits the number of times we will allocate IBaseFilter which seems to leak because of a Windows bug.

    auto infos = std::vector<Info>{};

    while (true)
    {
        auto pMoniker = AutoRelease<IMoniker>{};
        if (pEnum->Next(1, &pMoniker, nullptr) != S_OK)
            break;

        // }
        // if (SUCCEEDED(hr))
        {
            auto       available_resolutions = std::vector<Resolution>{};
            auto const webcam_name           = get_webcam_name(pMoniker);
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
                infos.push_back({webcam_name, get_webcam_id(pMoniker), available_resolutions});
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