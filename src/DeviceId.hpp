#pragma once
// #include <functional>
#include <string>

namespace wcam {

class DeviceId;
namespace internal {
auto make_device_id(std::string const& id) -> DeviceId;
}

class DeviceId {
public:
    auto        as_string() const -> std::string const& { return _device_path; }
    friend auto operator==(DeviceId const&, DeviceId const&) -> bool = default;

private:
    friend auto internal::make_device_id(std::string const& id) -> DeviceId;
    explicit DeviceId(std::string device_path) // Only the library is allowed to create ids, this is an opaque type
        : _device_path{std::move(device_path)}
    {}

private:
    friend struct std::hash<DeviceId>;
    std::string _device_path;
};

} // namespace wcam

namespace std {
template<>
struct hash<wcam::DeviceId> {
    auto operator()(wcam::DeviceId const& id) const noexcept -> std::size_t
    {
        return std::hash<std::string>()(id._device_path);
    }
};
} // namespace std