#include "Manager.hpp"
#include <mutex>
#include <variant>
#include "WebcamRequest.hpp"
#include "wcam_linux.hpp"
#include "wcam_macos.hpp"
#include "wcam_windows.hpp"

namespace wcam::internal {

#ifndef NDEBUG
std::atomic<int> Manager::_managers_alive_count{0};
#endif

Manager::Manager()
    : _thread{&Manager::thread_job, std::ref(*this)}
{
#ifndef NDEBUG
    assert(_managers_alive_count.load() == 0);
    _managers_alive_count.fetch_add(1);
#endif
}

void Manager::thread_job(Manager& self)
{
    while (!self._wants_to_stop_thread.load())
        self.update();
}

Manager::~Manager()
{
    _wants_to_stop_thread.store(true);
    _thread.join();

#ifndef NDEBUG
    _managers_alive_count.fetch_add(-1);
#endif
}

auto grab_all_infos_impl() -> std::vector<Info>;

static auto grab_all_infos() -> std::vector<Info>
{
    auto list_webcams_infos = internal::grab_all_infos_impl();
    for (auto& webcam_info : list_webcams_infos)
    {
        auto& resolutions = webcam_info.resolutions;
        std::sort(resolutions.begin(), resolutions.end(), [](img::Size const& res_a, img::Size const& res_b) {
            return res_a.width() > res_b.width()
                   || (res_a.width() == res_b.width() && res_a.height() > res_b.height());
        });
        resolutions.erase(std::unique(resolutions.begin(), resolutions.end()), resolutions.end());
    }
    return list_webcams_infos;
}

auto Manager::infos() const -> std::vector<Info>
{
    std::scoped_lock lock{_infos_mutex};
    return _infos;
}

auto Manager::open_or_get_webcam(DeviceId const& id) -> SharedWebcam
{
    auto const it = _current_requests.find(id);
    if (it != _current_requests.end())
    {
        std::shared_ptr<WebcamRequest> const request = it->second.lock();
        if (request) // A capture is still alive, we don't want to recreate a new one (we can't capture the same webcam twice anyways)
            return SharedWebcam{request};
    }
    auto const request    = std::make_shared<WebcamRequest>(id);
    _current_requests[id] = request; // Store a weak_ptr in the current requests
    return SharedWebcam{request};
}

auto Manager::selected_resolution(DeviceId const& id) const -> Resolution
{
    return {1920, 1080}; // TODO
}

auto Manager::is_plugged_in(DeviceId const& id) const -> bool
{
    std::scoped_lock lock{_infos_mutex};
    return _infos.end() != std::find_if(_infos.begin(), _infos.end(), [&](Info const& info) {
               return info.id == id;
           });
}

void Manager::update()
{
    { // TODO do this only every 0.5s, and sleep if we arrive here to fast ?
        auto infos = grab_all_infos();

        std::scoped_lock lock{_infos_mutex};
        _infos = std::move(infos);
    }

    {
        auto const       current_requests = _current_requests;
        std::scoped_lock lock{_captures_mutex};

        for (auto const& [_, request_weak_ptr] : current_requests) // Iterate on a copy of _current_requests, because we might add elements in the latter in parallel, and this would mess up the iteration (and we don't want to lock the map, otherwise it would slow down creating a new SharedWebcam)
        {
            std::shared_ptr<WebcamRequest> const request = request_weak_ptr.lock();
            if (!request) // There is currently no request for that webcam, nothing to do
                continue;
            if (!is_plugged_in(request->id()))
            {
                request->maybe_capture() = Error_WebcamUnplugged{};
                continue;
            }
            if (std::holds_alternative<Capture>(request->maybe_capture()))
                continue; // The capture is valid, nothing to do
            // Otherwise, the webcam is plugged in but the capture is not valid, so we should try to (re)create it
            try
            {
                request->maybe_capture() = Capture{request->id(), selected_resolution(request->id())};
            }
            catch (CaptureError const& err)
            {
                request->maybe_capture() = err;
            }
        }
    }
}

} // namespace wcam::internal