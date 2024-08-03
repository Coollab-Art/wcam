#pragma once
#include <atomic>
#include <img/img.hpp>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <dshow.h>
#include <windows.h>
#include <iostream>
#include <shared_mutex>
#include "../../src/qedit.h"

namespace wcam {
// Déclaration de l'identifiant de l'interface ISampleGrabberCB
EXTERN_C const IID IID_ISampleGrabberCB;

class SampleGrabberCallback : public ISampleGrabberCB {
public:
    SampleGrabberCallback(img::Size resolution)
        : _resolution{resolution}
    {
    }

    STDMETHODIMP_(ULONG)
    AddRef() { return 1; }
    STDMETHODIMP_(ULONG)
    Release() { return 2; }
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv)
    {
        if (riid == IID_ISampleGrabberCB || riid == IID_IUnknown)
        {
            *ppv = (void*)this;
            return NOERROR;
        }
        return E_NOINTERFACE;
    }

    STDMETHODIMP SampleCB(double Time, IMediaSample* pSample)
    {
        return 0;
    }

    STDMETHODIMP BufferCB(double Time, BYTE* pBuffer, long BufferLen)
    {
        assert(BufferLen == _resolution.width() * _resolution.height() * 3);
        auto buffer = new uint8_t[BufferLen];
        memcpy(buffer, pBuffer, BufferLen * sizeof(uint8_t));
        {
            std::unique_lock lock{*_mutex};
            _image = img::Image(_resolution, 3, buffer);
        }
        return 0;
    }

    std::optional<img::Image> image()
    {
        std::unique_lock lock{*_mutex};
        auto             res = std::move(_image);
        _image               = std::nullopt;
        return std::move(res);
    }

private:
    std::optional<img::Image>          _image{};
    img::Size                          _resolution;
    std::unique_ptr<std::shared_mutex> _mutex{std::make_unique<std::shared_mutex>()};
};

class UniqueId {
private:
    std::string device_path;

public:
    UniqueId() = default;
    explicit UniqueId(const std::string& val)
        : device_path(val) {}

    std::string getDevicePath() const { return device_path; }
};

struct Info {
    std::string              name{}; /// Name that can be displayed in the UI
    UniqueId                 unique_id{};
    std::vector<img::Size>   available_resolutions{};
    std::vector<std::string> pixel_formats{};
};

auto grab_all_infos() -> std::vector<Info>;

class Capture {
public:
    Capture(UniqueId unique_id, img::Size resolution);
    //     : _unique_id{unique_id}, _thread{[&]()
    //                                      { thread_job(*this, resolution); }}
    // {
    // }
    // ~Capture()
    // {
    //     stop_capture();
    //     if (_thread.joinable())
    //     {
    //         _thread.join();
    //     }
    // }
    Capture(Capture const&)                        = delete;
    auto operator=(Capture const&) -> Capture&     = delete;
    Capture(Capture&&) noexcept                    = default;
    auto operator=(Capture&&) noexcept -> Capture& = default;

    [[nodiscard]] auto has_stopped() const -> bool { return _has_stopped; }
    [[nodiscard]] auto unique_id() const -> UniqueId { return _unique_id; }
    [[nodiscard]] auto image() -> std::optional<img::Image>
    {
        return std::move(_sgCallback.image());
    }

private:
    // static void thread_job(Capture& This, img::Size);
    void update_webcam_ifn();

private:
    // void stop_capture()
    //     {
    //         std::lock_guard<std::mutex> lock(_mutex);
    //         _has_stopped = true;
    //         _wants_to_stop_thread = true;
    //     }
    //     void thread_job(Capture &This);

private:
    // std::mutex _mutex;
    std::optional<img::Image> _available_image;
    bool                      _has_stopped{false};
    UniqueId                  _unique_id;
    std::atomic<bool>         _wants_to_stop_thread{false};
    // std::thread _thread{};
    SampleGrabberCallback _sgCallback{{1, 1}};
};

std::vector<Info> getWebcamsInfo();

} // namespace wcam

// #pragma once
// #include <string>
// #include <vector>

// namespace webcam {

// struct Resolution {
//     int         width{1};
//     int         height{1};
//     friend auto operator==(Resolution const& a, Resolution const& b) -> bool
//     {
//         return a.width == b.width && a.height == b.height;
//     };
// };

// class UniqueId {
// private:
//     std::string device_path; // TODO sur windows utiliser le DevicePath https://learn.microsoft.com/en-us/windows/win32/directshow/selecting-a-capture-device
//                              // Sur mac, voir si on a un équivalent, sinon utiliser le nom de la webcam / leur type de uniqueid propre à eux (ptet un int ?)
// };

// class Capture {
// public:
//     Capture(UniqueId unique_id, webcam_info::Resolution resolution)
//         : _webcam_index{index}
//         , _thread{&Capture::thread_job, std::ref(*this), resolution}
//     {}
//     ~Capture();
//     Capture(Capture const&)                        = delete;
//     auto operator=(Capture const&) -> Capture&     = delete;
//     Capture(Capture&&) noexcept                    = delete;
//     auto operator=(Capture&&) noexcept -> Capture& = delete;

//     [[nodiscard]] auto has_stopped() const -> bool { return _has_stopped; }
//     [[nodiscard]] auto unique_id() const -> UniqueId { return _webcam_index; }
//     [[nodiscard]] auto image() const -> img::Image { return TODO; } // TODO récupérer l'image depuis le thread à ce moment là, et s'assurer que tant que qqun a une ref verse une image on ne l'écrase pas

// private:
//     static void thread_job(WebcamCapture& This, webcam_info::Resolution);
//     void        update_webcam_ifn();

// private:
//     std::mutex        _mutex{};
//     cv::Mat           _available_image{};
//     bool              _has_stopped{false};
//     size_t            _webcam_index;
//     std::atomic<bool> _wants_to_stop_thread{false};
//     std::thread       _thread{};
// };

// struct Info {
//     std::string name{}; /// Name that can be displayed in the UI
//     // std::string             description{}; /// Description that can be displayed in the UI // TODO get description from API
//     std::vector<Resolution> available_resolutions{};
//     UniqueId                unique_id{}; // TODO get unique_id from API
// };

// auto grab_all_infos() -> std::vector<Info>;

// auto start_capture(UniqueId const& unique_id, webcam_info::Resolution resolution) -> Capture;

// } // namespace webcam