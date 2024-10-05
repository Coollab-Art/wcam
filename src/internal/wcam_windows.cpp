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

static void delete_media_type(AM_MEDIA_TYPE* pmt)
{
    if (pmt == nullptr)
        return;

    if (pmt->cbFormat != 0)
    {
        CoTaskMemFree((PVOID)pmt->pbFormat); // NOLINT(*readability-casting)
        pmt->cbFormat = 0;
        pmt->pbFormat = nullptr;
    }
    if (pmt->pUnk != nullptr)
    {
        pmt->pUnk->Release();
        pmt->pUnk = nullptr;
    }
    CoTaskMemFree(pmt);
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

CaptureImpl::CaptureImpl(DeviceId const& device_id, Resolution const& requested_resolution)
{
    CoInitializeIFN();

    auto builder = AutoRelease<ICaptureGraphBuilder2>{CLSID_CaptureGraphBuilder2};
    auto graph   = AutoRelease<IGraphBuilder>{CLSID_FilterGraph};
    THROW_IF_ERR(builder->SetFiltergraph(graph));
    auto         moniker = AutoRelease<IMoniker>{find_moniker(device_id)};
    IBaseFilter* pCap    = nullptr;
    THROW_IF_ERR(moniker->BindToObject(0, 0, IID_IBaseFilter, (void**)&pCap));
    THROW_IF_ERR(graph->AddFilter(pCap, L"CaptureFilter"));

    IAMStreamConfig* pConfig = NULL;
    HRESULT          hr      = builder->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, pCap, IID_IAMStreamConfig, (void**)&pConfig);
    AM_MEDIA_TYPE*   pmt;
    hr = pConfig->GetFormat(&pmt); // Get the current format

    if (SUCCEEDED(hr))
    {
        // VIDEOINFOHEADER structure contains the format details
        VIDEOINFOHEADER* pVih = (VIDEOINFOHEADER*)pmt->pbFormat;

        // Set desired width and height
        pVih->bmiHeader.biWidth  = requested_resolution.width();
        pVih->bmiHeader.biHeight = requested_resolution.height();

        // Set the modified format
        hr = pConfig->SetFormat(pmt);

        if (FAILED(hr))
        {
            // Handle error
        }

        delete_media_type(pmt);
    }
    else
    {
        // Handle error
    }

    // 4. Add and Configure the Sample Grabber

    AutoRelease<IBaseFilter>    pSampleGrabberFilter{CLSID_SampleGrabber};
    AutoRelease<ISampleGrabber> pSampleGrabber{};
    THROW_IF_ERR(pSampleGrabberFilter->QueryInterface(IID_ISampleGrabber, (void**)&pSampleGrabber));

    // Configure the sample grabber
    if (device_id.as_string().find("OBS") != std::string::npos
        || device_id.as_string().find("Streamlabs") != std::string::npos)
    {
        // OBS Virtual Camera always returns S_OK on SetFormat(), even if it doesn't support
        // the actual format. So we have to choose a format that it supports manually, e.g. NV12.
        // https://github.com/opencv/opencv/issues/19746#issuecomment-1383056787
        _video_format = MEDIASUBTYPE_NV12;
    }
    else
    {
        _video_format = MEDIASUBTYPE_RGB24;
    }
    AM_MEDIA_TYPE mt;
    ZeroMemory(&mt, sizeof(AM_MEDIA_TYPE));
    mt.majortype = MEDIATYPE_Video;
    mt.subtype   = _video_format;
    THROW_IF_ERR(pSampleGrabber->SetMediaType(&mt));
    THROW_IF_ERR(pSampleGrabber->SetOneShot(false));
    THROW_IF_ERR(pSampleGrabber->SetBufferSamples(false));

    // Add the sample grabber to the graph
    THROW_IF_ERR(graph->AddFilter(pSampleGrabberFilter, L"Sample Grabber"));
    // 6. Retrieve the Video Information Header

    { // Tell the graph to process the samples as fast as possible, instead of trying to sync to a clock (cf. https://learn.microsoft.com/en-us/windows/win32/api/strmif/nf-strmif-imediafilter-setsyncsource)
        auto media_filter = AutoRelease<IMediaFilter>{};
        THROW_IF_ERR(graph->QueryInterface(IID_IMediaFilter, reinterpret_cast<void**>(&media_filter))); // NOLINT(*reinterpret-cast)
        media_filter->SetSyncSource(nullptr);
    }
    // 5. Render the Stream
    AutoRelease<IBaseFilter> pNullRenderer{CLSID_NullRenderer};
    THROW_IF_ERR(graph->AddFilter(pNullRenderer, L"Null Renderer"));

    THROW_IF_ERR(builder->RenderStream(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, pCap, pSampleGrabberFilter, pNullRenderer)); // Check that PIN_CATEGORY_PREVIEW is indeed more performant than PIN_CATEGORY_CAPTURE

    // 7. Start the Graph
    THROW_IF_ERR(graph->QueryInterface(IID_IMediaControl, (void**)&_media_control));
    THROW_IF_ERR(_media_control->Run());
    {
        auto _media_event = AutoRelease<IMediaEventEx>{};
        THROW_IF_ERR(graph->QueryInterface(IID_IMediaEventEx, (void**)&_media_event));
        long     evCode;
        LONG_PTR param1, param2;
        bool     disconnected = false;

        while (S_OK == _media_event->GetEvent(&evCode, &param1, &param2, 0))
        {
            if (evCode == EC_ERRORABORT)
                disconnected = true;
            _media_event->FreeEventParams(evCode, param1, param2);
        }
        if (disconnected)
            throw CaptureException{Error_WebcamAlreadyUsedInAnotherApplication{}};
    }

    AM_MEDIA_TYPE mtGrabbed;
    THROW_IF_ERR(pSampleGrabber->GetConnectedMediaType(&mtGrabbed));

    VIDEOINFOHEADER* pVih = (VIDEOINFOHEADER*)mtGrabbed.pbFormat;
    _resolution           = Resolution{
        static_cast<Resolution::DataType>(pVih->bmiHeader.biWidth),
        static_cast<Resolution::DataType>(pVih->bmiHeader.biHeight),
    };

    assert(
        (_video_format == MEDIASUBTYPE_RGB24 && pVih->bmiHeader.biSizeImage == _resolution.pixels_count() * 3)
        || (_video_format == MEDIASUBTYPE_NV12 && pVih->bmiHeader.biSizeImage == _resolution.pixels_count() * 3 / 2)
    );

    THROW_IF_ERR(pSampleGrabber->SetCallback(this, 1));
}

STDMETHODIMP CaptureImpl::BufferCB(double /* Time */, BYTE* pBuffer, long BufferLen)
{
    auto image = image_factory().make_image();
    image->set_resolution(_resolution);
    if (_video_format == MEDIASUBTYPE_RGB24)
    {
        image->set_row_order(wcam::FirstRowIs::Bottom);
        image->set_data(ImageDataView<BGR24>{pBuffer, static_cast<size_t>(BufferLen), _resolution});
    }
    else if (_video_format == MEDIASUBTYPE_NV12)
    {
        image->set_row_order(wcam::FirstRowIs::Top);
        image->set_data(ImageDataView<NV12>{pBuffer, static_cast<size_t>(BufferLen), _resolution});
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