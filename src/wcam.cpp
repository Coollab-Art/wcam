#include <img/img.hpp>
#define WIN32_LEAN_AND_MEAN
#include <dshow.h>
#include <windows.h>
#include <iostream>

// #include <conio.h>
#include "qedit.h"
#include "wcam/wcam.hpp"

namespace wcam {

// Définition de la méthode Capture::image()
// std::optional<img::Image> Capture::image()
// {
//     std::lock_guard<std::mutex> lock(_mutex); // Assurez-vous que _mutex n'est pas const
//     if (_available_image)
//     {
//         return std::move(*_available_image); // Déplacement au lieu de copie
//     }
//     // Retourner une image vide ou gérer le cas d'absence d'image
//     img::Size empty_size{0, 0}; // Utiliser la classe Size
//     auto empty_data = std::make_unique<uint8_t[]>(0); // Allocation d'un tableau vide pour les données
//     return img::Image(empty_size, 3, empty_data.release()); // Utiliser release() pour obtenir un pointeur brut
// }

// Définition de la méthode Capture::thread_job()
// void Capture::thread_job(Capture &This, img::Size resolution)
// {
//     while (!This._wants_to_stop_thread)
//     {
//         // Logique de capture ici
//         img::Size captured_size{resolution}; // Exemple de taille, à remplacer par les dimensions réelles
//         int channels = 3; // Exemple de nombre de canaux, à remplacer par le nombre réel
//         auto data = std::make_unique<uint8_t[]>(captured_size.width() * captured_size.height() * channels); // Allouer la mémoire pour les données de l'image
//         // Récupérer l'image ici
//         img::Image captured_image(captured_size, channels, data.release()); // Utiliser release() pour obtenir un pointeur brut

//         {
//             std::lock_guard<std::mutex> lock(This._mutex);
//             This._available_image.emplace(std::move(captured_image)); // Utiliser emplace() pour ajouter l'image capturée
//         }

//         // Simuler un délai de capture
//         std::this_thread::sleep_for(std::chrono::milliseconds(100));
//     }
//     This._has_stopped = true;
// }

// Définition de la fonction getWebcamsInfo()
std::vector<Info> getWebcamsInfo()
{
    std::vector<Info> info_list;

    // Exemple simplifié. Remplacez par le code réel pour détecter les webcams.
    Info info;
    info.name      = "Webcam Example";
    info.unique_id = UniqueId("example_id");
    info_list.push_back(info);

    // Ajoutez ici le code pour détecter les webcams disponibles et remplir info_list

    return info_list;
}

using namespace std;

static auto convert_wstr_to_str(std::wstring const& wstr) -> std::string
{
    std::string res;
    res.reserve(wstr.size());
    for (wchar_t const c : wstr)
        res.push_back(static_cast<char>(c)); // TODO Support proper unicode strings
    return res;
}

// int main() {

//     HRESULT hr;

//     ICaptureGraphBuilder2 *pBuild = nullptr;
//     IGraphBuilder *pGraph = nullptr;
//     IMoniker *pMoniker = nullptr;
//     IEnumMoniker *pEnum = nullptr;
//     ICreateDevEnum *pDevEnum = nullptr;

//     int NumeroAppareil;
//       hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
//     if (FAILED(hr)) {
//         cout << "Échec de CoInitializeEx!" << endl;
//         return -1;
//     }

//     hr = CoCreateInstance(CLSID_CaptureGraphBuilder2, NULL, CLSCTX_INPROC_SERVER, IID_ICaptureGraphBuilder2, (void**)&pBuild);
//     if (FAILED(hr)) {
//         cout << "Échec de la création de CaptureGraphBuilder2!" << endl;
//         return -1;
//     }

//     // 2. Create the Filter Graph Manager

//     hr = CoCreateInstance(CLSID_FilterGraph, 0, CLSCTX_INPROC_SERVER, IID_IFilterGraph, (void**)&pGraph);
//     if (FAILED(hr)) {
//         cout << "Échec de la création de FilterGraph!" << endl;
//         return -1;
//     }

//     hr = pBuild->SetFiltergraph(pGraph); // Utilisation de SetFiltergraph
//     if (FAILED(hr)) {
//         cout << "Échec de l'appel de SetFiltergraph!" << endl;
//         return -1;
//     }

//     hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDevEnum));
//     if (FAILED(hr)) {
//         cout << "Échec de la création de SystemDeviceEnum!" << endl;
//         return -1;
//     }

//     // Création d'un énumérateur pour les périphériques vidéo :

//     hr = pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, 0);
//     if (hr != S_OK) {
//         cout << "Échec de la création de l'énumérateur de périphériques ou aucun périphérique trouvé!" << endl;
//         return -1;
//     }

//     cout << "Sélectionnez le périphérique :" << endl;
//     int index = 0;

//     // Affichage des périphériques disponibles

//     while (pEnum->Next(1, &pMoniker, NULL) == S_OK)
//     {
//         IPropertyBag *pPropBag;
//         hr = pMoniker->BindToStorage(0, 0, IID_PPV_ARGS(&pPropBag));
//         if (FAILED(hr)) {
//             pMoniker->Release();
//             continue;
//         }

//         VARIANT var;
//         VariantInit(&var);

//         hr = pPropBag->Read(L"FriendlyName", &var, 0);
//         if (SUCCEEDED(hr)) {
//             wcout << index++ << ": " << (wchar_t*)var.bstrVal << endl;
//             VariantClear(&var);
//         }

//         pPropBag->Release();
//         pMoniker->Release();
//     }

//     pEnum->Reset();

//     // Sélection de l'appareil par l'utilisateur

//     cin >> NumeroAppareil;

//     webcam::Capture capture{UniqueId{""}, {1, 1}};

//     while (true) {
//         if(capture.image().has_value()){
//             std::cout << capture.image()->width() << std::endl;
//         }

//     }
// }

Capture::Capture(UniqueId unique_id, img::Size requested_resolution)
{
    HRESULT                hr;
    IEnumMoniker*          pEnum    = nullptr;
    IMoniker*              pMoniker = nullptr;
    ICaptureGraphBuilder2* pBuild   = nullptr;
    IGraphBuilder*         pGraph   = nullptr;
    ICreateDevEnum*        pDevEnum = nullptr;
    IBaseFilter*           pCap     = nullptr;
    IMediaControl*         pControl = nullptr;
    IVideoWindow*          pWindow  = nullptr;

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
            if (convert_wstr_to_str(var.bstrVal) == unique_id.getDevicePath())
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
    img::Size        resolution{
        static_cast<img::Size::DataType>(pVih->bmiHeader.biWidth),
        static_cast<img::Size::DataType>(pVih->bmiHeader.biHeight),
    };

    assert(pVih->bmiHeader.biSizeImage == resolution.width() * resolution.height() * 3);

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
    _sgCallback = SampleGrabberCallback{resolution};

    pSampleGrabber->SetCallback(&_sgCallback, 1);

    MSG msg;

    //     while (true) {
    //    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) > 0) {
    //         // DefWindowProc(msg.hwnd, msg.message, msg.wParam, msg.lParam);
    //     }
    // }

    // Boucle de message pour la gestion des événements

    // Libération des ressources et dé-initialisation de COM
}
} // namespace wcam

namespace wcam {

auto grab_all_infos_impl() -> std::vector<Info>;

auto grab_all_infos() -> std::vector<Info>
{
    auto list_webcams_infos = grab_all_infos_impl();
    for (auto& webcam_info : list_webcams_infos)
    {
        auto& resolutions = webcam_info.available_resolutions;
        std::sort(resolutions.begin(), resolutions.end(), [](img::Size const& res_a, img::Size const& res_b) {
            return res_a.width() > res_b.width()
                   || (res_a.width() == res_b.width() && res_a.height() > res_b.height());
        });
        resolutions.erase(std::unique(resolutions.begin(), resolutions.end()), resolutions.end());
    }
    return list_webcams_infos;
}

} // namespace wcam

#if defined(_WIN32)

#if defined(GCC) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wlanguage-extension-token"
#endif

#include <dshow.h>
#include <cstdlib>
#include <unordered_map>

namespace wcam {

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

} // namespace wcam

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#endif

#if defined(__linux__)

#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <filesystem>

static auto find_available_resolutions(int const video_device) -> std::vector<webcam_info::img::Size>
{
    std::vector<webcam_info::img::Size> available_resolutions;

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

class CloseFileAtExit {
public:
    explicit CloseFileAtExit(int file_descriptor)
        : _file_descriptor{file_descriptor}
    {}

    ~CloseFileAtExit()
    {
        close(_file_descriptor);
    }

private:
    int _file_descriptor{};
};

auto webcam_info::grab_all_infos_impl() -> std::vector<Info>
{
    std::vector<Info> list_webcam_info{};

    for (auto const& entry : std::filesystem::directory_iterator("/dev"))
    {
        if (entry.path().string().find("video") == std::string::npos)
            continue;

        int const video_device = open(entry.path().c_str(), O_RDONLY);
        if (video_device == -1)
            continue;
        auto const scope_guard = CloseFileAtExit{video_device};

        v4l2_capability cap{};
        if (ioctl(video_device, VIDIOC_QUERYCAP, &cap) == -1)
            continue;

        if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
            continue;

        std::string const webcam_name = reinterpret_cast<char const*>(cap.card); // NOLINT(*-pro-type-reinterpret-cast)

        std::vector<webcam_info::img::Size> const available_resolutions = find_available_resolutions(video_device);
        if (available_resolutions.empty())
            continue;

        list_webcam_info.push_back({webcam_name, available_resolutions});
    }

    return list_webcam_info;
}

#endif