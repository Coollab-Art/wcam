#if defined(_WIN32)
#include "wcam_windows.hpp"
#include <fmt/format.h>
#include <cstdlib>
#include <source_location>
#include <string>
#include <string_view>
#include <unordered_map>
#include "../Info.hpp"
#include "ImageFactory.hpp"
#include "fallback_webcam_name.hpp"
#include "make_device_id.hpp"

/// NB: we use DirectShow and not MediaFoundation
/// because OBS Virtual Camera only works with DirectShow
/// https://github.com/obsproject/obs-studio/issues/8057
/// Apparently Windows 11 adds this capability (https://medium.com/deelvin-machine-learning/how-does-obs-virtual-camera-plugin-work-on-windows-e92ab8986c4e)
/// So in a very distant future, when Windows 11 is on 99.999% of the machines, and when OBS implements a MediaFoundation backend and a virtual camera for it, then we can switch to MediaFoundation

#if defined(GCC) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wlanguage-extension-token"
#endif

namespace wcam::internal {

static auto convert_wstr_to_str(BSTR const& wstr) -> std::string
{
    auto const wstr_len = static_cast<int>(SysStringLen(wstr));
    auto const res_len  = WideCharToMultiByte(CP_UTF8, 0, wstr, wstr_len, nullptr, 0, nullptr, nullptr);
    if (res_len == 0)
        return "";

    auto res = std::string(static_cast<size_t>(res_len), 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr, wstr_len, res.data(), res_len, nullptr, nullptr);
    return res;
}

static auto hr_to_string(HRESULT hr) -> std::string
{
    char* error_message{nullptr};
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        static_cast<DWORD>(hr),
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
    throw CaptureException{Error_Unknown{fmt::format("{}(During `{}`, at {}({}:{}))", hr_to_string(hr), code_that_failed, location.file_name(), location.line(), location.column())}};
}

#define THROW_IF_ERR(exp) /*NOLINT(*macro*)*/ \
    {                                         \
        HRESULT const hresult = exp;          \
        if (FAILED(hresult))                  \
            throw_error(hresult, #exp);       \
    }
#define THROW_IF_ERR2(exp, location) /*NOLINT(*macro*)*/ \
    {                                                    \
        HRESULT const hresult = exp;                     \
        if (FAILED(hresult))                             \
            throw_error(hresult, #exp, location);        \
    }
#define ASSERT_AND_RETURN_IF_ERR(exp, default_value) /*NOLINT(*macro*)*/ \
    {                                                                    \
        HRESULT const hresult = exp;                                     \
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

    auto move() -> T*
    {
        T* res = _ptr;
        _ptr   = nullptr;
        return res;
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
        auto const hr = VariantClear(&_variant);
        assert(hr == S_OK);
        std::ignore = hr; // Silence warning in Release
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
    ASSERT_AND_RETURN_IF_ERR(moniker->BindToStorage(nullptr, nullptr, IID_PPV_ARGS(&prop_bag)), fallback_webcam_name());

    auto          webcam_name_wstr = VariantRAII{};
    HRESULT const hr               = prop_bag->Read(L"FriendlyName", &webcam_name_wstr, nullptr);
    if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
        return fallback_webcam_name();
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
    if (enumerator == nullptr) // Might still be nullptr after CreateClassEnumerator if the VideoInputDevice category is empty or missing (https://learn.microsoft.com/en-us/previous-versions/ms784969(v=vs.85))
        throw CaptureException{Error_WebcamUnplugged{}};

    while (true) // while(true) because we want to declare the moniker inside the loop so that it gets destroyed properly during each iteration of the loop
    {
        auto moniker = AutoRelease<IMoniker>{};
        if (enumerator->Next(1, &moniker, nullptr) != S_OK)
            break;

        if (get_webcam_id(moniker) == device_id)
            return moniker.move(); // move() to make sure it's not destroyed automatically at the end of this function
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
        ICaptureImpl::set_image(Error_Unknown{"Unsupported pixel format"});
        return S_OK;
    }

    ICaptureImpl::set_image(std::move(image));
    return S_OK;
}

CaptureImpl::~CaptureImpl()
{
    _media_control->Stop();
    _media_control->Release();
}

static auto get_resolutions(IBaseFilter* capture_filter) -> std::vector<Resolution>
{
    auto resolutions = std::vector<Resolution>{};

    auto pins_enumerator = AutoRelease<IEnumPins>{};
    THROW_IF_ERR(capture_filter->EnumPins(&pins_enumerator));
    while (true)
    {
        auto pin = AutoRelease<IPin>{}; // Declare pin inside the loop so that it is freed at the end, Next doesn't Release the pin that you pass
        if (pins_enumerator->Next(1, &pin, nullptr) != S_OK)
            break;

        PIN_DIRECTION pin_direction; // NOLINT(*-init-variables)
        pin->QueryDirection(&pin_direction);
        if (pin_direction != PINDIR_OUTPUT)
            continue;

        auto stream_config = AutoRelease<IAMStreamConfig>{};
        THROW_IF_ERR(pin->QueryInterface(IID_PPV_ARGS(&stream_config)));
        int count, size; // NOLINT(*-init-variables, *isolate-declaration)
        THROW_IF_ERR(stream_config->GetNumberOfCapabilities(&count, &size));
        VIDEO_STREAM_CONFIG_CAPS caps;
        for (int i = 0; i < count; i++)
        {
            auto media_type = MediaTypeRAII{};
            THROW_IF_ERR(stream_config->GetStreamCaps(i, &media_type, reinterpret_cast<BYTE*>(&caps))); // NOLINT(*-pro-type-reinterpret-cast)
            if (media_type->formattype != FORMAT_VideoInfo)
                continue;
            auto* const video_info = reinterpret_cast<VIDEOINFOHEADER*>(media_type->pbFormat); // NOLINT(*-pro-type-reinterpret-cast)
            resolutions.emplace_back(
                static_cast<Resolution::DataType>(video_info->bmiHeader.biWidth),
                static_cast<Resolution::DataType>(video_info->bmiHeader.biHeight)
            );
        }
    }

    return resolutions;
}

auto grab_all_infos_impl() -> std::vector<Info>
{
    CoInitializeIFN();

    auto dev_enum   = AutoRelease<ICreateDevEnum>{CLSID_SystemDeviceEnum};
    auto enumerator = AutoRelease<IEnumMoniker>{};
    THROW_IF_ERR(dev_enum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &enumerator, 0));
    if (enumerator == nullptr) // Might still be nullptr after CreateClassEnumerator if the VideoInputDevice category is empty or missing (https://learn.microsoft.com/en-us/previous-versions/ms784969(v=vs.85))
        return {};

    thread_local auto resolutions_cache = std::unordered_map<DeviceId, std::vector<Resolution>>{}; // This cache limits the number of times we will allocate IBaseFilter which seems to leak because of a Windows bug.

    auto infos = std::vector<Info>{};
    while (true) // while(true) because we want to declare the moniker inside the loop so that it gets destroyed properly during each iteration of the loop
    {
        auto moniker = AutoRelease<IMoniker>{};
        if (enumerator->Next(1, &moniker, nullptr) != S_OK)
            break;

        auto const webcam_id   = get_webcam_id(moniker);
        auto const resolutions = [&]() -> std::vector<Resolution> {
            auto const it = resolutions_cache.find(webcam_id);
            if (it != resolutions_cache.end())
                return it->second;

            auto capture_filter = AutoRelease<IBaseFilter>{};
            THROW_IF_ERR(moniker->BindToObject(nullptr, nullptr, IID_PPV_ARGS(&capture_filter)));
            return resolutions_cache.insert(std::make_pair(webcam_id, get_resolutions(capture_filter))).first->second;
        }();

        if (!resolutions.empty())
            infos.push_back({get_webcam_name(moniker), webcam_id, resolutions});
    }

    return infos;
}

} // namespace wcam::internal

#if defined(GCC) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#endif