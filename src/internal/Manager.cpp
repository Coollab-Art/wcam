#include "Manager.hpp"
#include <mutex>
#include <variant>
#include "WebcamRequest.hpp"

namespace wcam::internal {

Manager::~Manager()
{
    stop_thread_ifn();
}

void Manager::start_thread_ifn()
{
    if (_thread.has_value())
        return;

    _wants_to_stop_thread.store(false);
    _thread.emplace(&Manager::thread_job, std::ref(*this));
}

void Manager::stop_thread_ifn()
{
    if (!_thread.has_value())
        return;

    _wants_to_stop_thread.store(true);
    _thread->join();
    _thread.reset();
}

void Manager::check_if_update_needs_to_continue()
{
    bool needs_to_update{};
    {
        std::scoped_lock lock{_captures_mutex};
        for (auto it = _current_requests.begin(); it != _current_requests.end();)
        {
            if (it->second.expired())
                it = _current_requests.erase(it); // Erase and move iterator to the next element
            else
                ++it; // Move to the next element
        }

        needs_to_update = !_current_requests.empty()
                          || _infos_have_been_requested_this_frame.load();
    }

    if (needs_to_update)
        start_thread_ifn();
    else
        stop_thread_ifn();

    _infos_have_been_requested_this_frame.store(false);
}

void Manager::thread_job(Manager& self)
{
    while (!self._wants_to_stop_thread.load())
        self.update();
}

auto grab_all_infos_impl() -> std::vector<Info>;

static auto grab_all_infos() -> std::vector<Info>
{
    auto list_webcams_infos = internal::grab_all_infos_impl();
    for (auto& webcam_info : list_webcams_infos)
    {
        auto& resolutions = webcam_info.resolutions;
        std::sort(resolutions.begin(), resolutions.end(), [](Resolution const& res_a, Resolution const& res_b) {
            return res_a.pixels_count() > res_b.pixels_count()
                   || (res_a.pixels_count() == res_b.pixels_count() && res_a.width() > res_b.width());
        });
        resolutions.erase(std::unique(resolutions.begin(), resolutions.end()), resolutions.end());
    }
    return list_webcams_infos;
}

auto Manager::infos() const -> std::vector<Info>
{
    _infos_have_been_requested_this_frame.store(true);
    std::scoped_lock lock{_infos_mutex};
    return _infos;
}

/// Iterates over the map + Might add a new element to the map
auto Manager::open_or_get_webcam(DeviceId const& id) -> SharedWebcam
{
    std::scoped_lock lock{_captures_mutex};

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

/// Iterates over the map + might modify an element of the map
void Manager::request_a_restart_of_the_capture_if_it_exists(DeviceId const& id)
{
    std::scoped_lock lock{_captures_mutex};

    auto const it = _current_requests.find(id);
    if (it == _current_requests.end())
        return;
    auto const request = it->second.lock();
    if (!request)
        return;
    request->maybe_capture() = CaptureNotInitYet{};
}

auto Manager::default_resolution(DeviceId const& id) const -> Resolution
{
    std::scoped_lock lock{_infos_mutex};
    auto const       it = std::find_if(_infos.begin(), _infos.end(), [&](Info const& info) {
        return info.id == id;
    });
    if (it == _infos.end() || it->resolutions.empty())
        return {1, 1};
    return it->resolutions[0]; // We know that resolutions are sorted from largest to smallest, and we want to select the largest one by default
}

auto Manager::get_name(DeviceId const& id) const -> std::optional<std::string>
{
    std::scoped_lock lock{_infos_mutex};
    auto const       it = std::find_if(_infos.begin(), _infos.end(), [&](Info const& info) {
        return info.id == id;
    });
    if (it == _infos.end())
        return std::nullopt;
    return it->name;
}

auto Manager::is_plugged_in(DeviceId const& id) const -> bool
{
    std::scoped_lock lock{_infos_mutex};
    return _infos.end() != std::find_if(_infos.begin(), _infos.end(), [&](Info const& info) {
               return info.id == id;
           });
}

/// Iterates over the map + might modify an element of the map
void Manager::update()
{
    {
        auto infos = grab_all_infos();

        std::scoped_lock lock{_infos_mutex};
        _infos = std::move(infos);
    }

    {
        auto const current_requests = [&]() { // IIFE
            std::scoped_lock lock{_captures_mutex};
            return _current_requests;
        }();

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
            catch (CaptureException const& e)
            {
                request->maybe_capture() = e.capture_error;
            }
        }
    }
}

auto Manager::selected_resolution(DeviceId const& id) const -> Resolution
{
    auto const it = _selected_resolutions.find(id);
    if (it != _selected_resolutions.end())
        return it->second;
    return manager().default_resolution(id);
}

void Manager::set_selected_resolution(DeviceId const& id, Resolution resolution)
{
    auto const it = _selected_resolutions.find(id);
    if (it != _selected_resolutions.end() && it->second == resolution)
        return; // The resolution is already set, no need to do anything, and we don't need to restart the capture
    _selected_resolutions[id] = resolution;

    manager().request_a_restart_of_the_capture_if_it_exists(id);
}

} // namespace wcam::internal