#pragma once
#include <img/img.hpp>
#include "../../src/internal.hpp"

namespace wcam {

class UniqueId {
public:
    explicit UniqueId(std::string device_path)
        : _device_path{std::move(device_path)}
    {}

    friend auto operator==(UniqueId const&, UniqueId const&) -> bool = default;

    // private:
    //     auto device_path() const -> std::string const& { return _device_path; }

private:
    std::string _device_path;
};

struct Info {
    std::string            name{}; /// Name that can be displayed in the UI
    UniqueId               unique_id;
    std::vector<img::Size> available_resolutions{};
    // std::vector<std::string> pixel_formats{}; // TODO
};

auto grab_all_infos() -> std::vector<Info>;

class Capture {
public:
    /// Throws a std::runtime_error if the creation of the Capture fails
    Capture(UniqueId const& unique_id, img::Size const& resolution);

    /// Returns the latest image that has been captured, or nullopt if this is the same as the image that was retrieved during the previous call to image() (or if no image has been captured yet)
    [[nodiscard]] auto image() -> std::optional<img::Image> { return _pimpl->image(); } // We don't use std::move because it would prevent copy elision

private:
    std::unique_ptr<internal::ICapture> _pimpl;
};

std::vector<Info> getWebcamsInfo();

} // namespace wcam