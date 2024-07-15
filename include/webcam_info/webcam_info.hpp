#pragma once
#include <string>
#include <vector>

namespace webcam {

struct Resolution {
    int         width{1};
    int         height{1};
    friend auto operator==(Resolution const& a, Resolution const& b) -> bool
    {
        return a.width == b.width && a.height == b.height;
    };
};

class UniqueId {
private:
    std::string device_path; // TODO sur windows utiliser le DevicePath https://learn.microsoft.com/en-us/windows/win32/directshow/selecting-a-capture-device
                             // Sur mac, voir si on a un équivalent, sinon utiliser le nom de la webcam / leur type de uniqueid propre à eux (ptet un int ?)
};

class Capture {
public:
    Capture(UniqueId unique_id, webcam_info::Resolution resolution)
        : _webcam_index{index}
        , _thread{&Capture::thread_job, std::ref(*this), resolution}
    {}
    ~Capture();
    Capture(Capture const&)                        = delete;
    auto operator=(Capture const&) -> Capture&     = delete;
    Capture(Capture&&) noexcept                    = delete;
    auto operator=(Capture&&) noexcept -> Capture& = delete;

    [[nodiscard]] auto has_stopped() const -> bool { return _has_stopped; }
    [[nodiscard]] auto unique_id() const -> UniqueId { return _webcam_index; }
    [[nodiscard]] auto image() const -> img::Image { return TODO; } // TODO récupérer l'image depuis le thread à ce moment là, et s'assurer que tant que qqun a une ref verse une image on ne l'écrase pas

private:
    static void thread_job(WebcamCapture& This, webcam_info::Resolution);
    void        update_webcam_ifn();

private:
    std::mutex        _mutex{};
    cv::Mat           _available_image{};
    bool              _has_stopped{false};
    size_t            _webcam_index;
    std::atomic<bool> _wants_to_stop_thread{false};
    std::thread       _thread{};
};

struct Info {
    std::string name{}; /// Name that can be displayed in the UI
    // std::string             description{}; /// Description that can be displayed in the UI // TODO get description from API
    std::vector<Resolution> available_resolutions{};
    UniqueId                unique_id{}; // TODO get unique_id from API
};

auto grab_all_infos() -> std::vector<Info>;

auto start_capture(UniqueId const& unique_id, webcam_info::Resolution resolution) -> Capture;

} // namespace webcam