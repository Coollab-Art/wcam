#pragma once
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
    [[nodiscard]] auto infos() const -> std::vector<Info>;
    [[nodiscard]] auto open_or_get_webcam(DeviceId const& id) -> SharedWebcam;
    [[nodiscard]] auto selected_resolution(DeviceId const& id) const -> Resolution;

    void update();

private:
    auto is_plugged_in(DeviceId const& id) const -> bool;

private:
    std::vector<Info>                                          _infos{};
    std::unordered_map<DeviceId, std::weak_ptr<WebcamRequest>> _current_requests{};
};

inline auto manager() -> Manager&
{
    static auto instance = Manager{};
    return instance;
}

class MetaManager {
public:
    MetaManager()
        : _thread{[]() {
            while (true) // TODO stop the thread properly
                manager().update();
        }}
    {}

private:
    std::thread _thread;
};

// static MetaManager meta_manager{};

} // namespace wcam::internal