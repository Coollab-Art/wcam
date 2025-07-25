#pragma once
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>
#include <vector>
#include "../Info.hpp"
#include "../Resolution.hpp"
#include "../ResolutionsMap.hpp"
#include "../SharedWebcam.hpp"
#include "WebcamRequest.hpp"

namespace wcam::internal {

class Manager {
public:
    Manager() = default;
    ~Manager();
    Manager(Manager const&)                        = delete;
    auto operator=(Manager const&) -> Manager&     = delete;
    Manager(Manager&&) noexcept                    = delete;
    auto operator=(Manager&&) noexcept -> Manager& = delete;

    [[nodiscard]] auto infos() const -> std::vector<Info>;
    [[nodiscard]] auto open_or_get_webcam(DeviceId const& id) -> SharedWebcam;
    [[nodiscard]] auto default_resolution(DeviceId const& id) const -> Resolution;
    [[nodiscard]] auto get_name(DeviceId const& id) const -> std::string;
    void               request_a_restart_of_the_capture_if_it_exists(DeviceId const& id);

    void check_if_update_needs_to_continue();

    auto selected_resolution(DeviceId const&) const -> Resolution;
    void set_selected_resolution(DeviceId const&, Resolution);

    auto get_resolutions_map() -> ResolutionsMap& { return _selected_resolutions; }

private:
    auto is_plugged_in(DeviceId const& id) const -> bool;

    void start_thread_ifn();
    void stop_thread_ifn();

    void        update();
    static void thread_job(Manager& self);

private:
    std::vector<Info>                                          _infos{};
    std::unordered_map<DeviceId, std::weak_ptr<WebcamRequest>> _current_requests{};

    mutable std::mutex         _infos_mutex{};
    mutable std::mutex         _captures_mutex{};
    std::atomic<bool>          _wants_to_stop_thread{false};
    mutable std::atomic<bool>  _infos_have_been_requested_this_frame{false};
    std::optional<std::thread> _thread{};

    ResolutionsMap _selected_resolutions{};
};

inline auto manager() -> Manager&
{
    static auto instance = internal::Manager{};
    return instance;
}

} // namespace wcam::internal