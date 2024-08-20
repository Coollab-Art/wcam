#include "Manager.hpp"
#include <variant>
#include "WebcamRequest.hpp"
#include "wcam_linux.hpp"
#include "wcam_macos.hpp"
#include "wcam_windows.hpp"

namespace wcam::internal {

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
    return _infos; // TODO lock
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
    return {1, 1}; // TODO
}

auto Manager::is_plugged_in(DeviceId const& id) const -> bool
{
    return _infos.end() != std::find_if(_infos.begin(), _infos.end(), [&](Info const& info) {
               return info.id == id;
           });
}

void Manager::update()
{
    // TODO do something smart, like keeping them in cache and only refreshing every 0.5 seconds?
    auto const new_infos = grab_all_infos();
    /// Signal the captures manager when a webcam is plugged back in
    // TODO
    // for (auto const& info : new_infos)
    // {
    //     if (_infos.end() == std::find_if(_infos.begin(), _infos.end(), [&](Info const& info2) {
    //             return info.id == info2.id;
    //         }))
    //     {
    //         // captures_manager().on_webcam_plugged_in(info.id);
    //     }
    // }

    _infos = new_infos;

    for (auto const& [_, request_weak_ptr] : _current_requests)
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

} // namespace wcam::internal