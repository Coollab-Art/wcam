#pragma once
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>
#include "../Info.hpp"
#include "../Resolution.hpp"
#include "../SharedWebcam.hpp"
#include "WebcamRequest.hpp"

namespace wcam::internal {

class Manager {
public:
    Manager();
    ~Manager();
    Manager(Manager const&)                        = delete;
    auto operator=(Manager const&) -> Manager&     = delete;
    Manager(Manager&&) noexcept                    = delete;
    auto operator=(Manager&&) noexcept -> Manager& = delete;

    [[nodiscard]] auto infos() const -> std::vector<Info>;
    [[nodiscard]] auto open_or_get_webcam(DeviceId const& id) -> SharedWebcam;
    [[nodiscard]] auto default_resolution(DeviceId const& id) const -> Resolution;
    void               request_a_restart_of_the_capture_if_it_exists(DeviceId const& id);

    void update();

private:
    auto is_plugged_in(DeviceId const& id) const -> bool;

    static void thread_job(Manager& self);

private:
    std::vector<Info>                                          _infos{};
    std::unordered_map<DeviceId, std::weak_ptr<WebcamRequest>> _current_requests{};

    mutable std::mutex _infos_mutex{};
    mutable std::mutex _captures_mutex{};
    std::atomic<bool>  _wants_to_stop_thread{false};
    std::thread        _thread{}; // Must be initialized last, to make sure that everything else is init when the thread starts its job and uses those other things

#ifndef NDEBUG
    static std::atomic<int> _managers_alive_count;
#endif
};

auto manager() -> std::shared_ptr<Manager>;
auto manager_unchecked() -> std::shared_ptr<Manager>;

} // namespace wcam::internal