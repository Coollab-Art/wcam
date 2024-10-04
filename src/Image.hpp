#pragma once
#include <cassert>
#include <cstdint>
#include <memory>
#include <utility>
#include <variant>
#include "FirstRowIs.hpp"
#include "Resolution.hpp"
#include "overloaded.hpp"

namespace wcam {

struct RGB24 {
    static auto data_length(Resolution resolution) -> size_t
    {
        return resolution.pixels_count() * 3;
    }
};

struct BGR24 {
    static auto data_length(Resolution resolution) -> size_t
    {
        return resolution.pixels_count() * 3;
    }
};

struct NV12 {
    static auto data_length(Resolution resolution) -> size_t
    {
        return resolution.pixels_count() * 3 / 2;
    }
};

template<typename PixelFormatT>
class ImageData {
public:
    ImageData(std::shared_ptr<uint8_t const> data, Resolution resolution)
        : _data{std::move(data)}
        , _resolution{resolution}
    {}
    auto data() const -> uint8_t const* { return _data.get(); }
    auto resolution() const -> Resolution { return _resolution; }

private:
    std::shared_ptr<uint8_t const> _data{};
    Resolution                     _resolution{};
};

template<typename PixelFormatT>
class ImageDataView {
public:
    ImageDataView(std::variant<uint8_t const*, std::shared_ptr<uint8_t const>> data, size_t data_length, Resolution resolution)
        : _data{std::move(data)}
        , _resolution{resolution}
    {
        assert(PixelFormatT::data_length(_resolution) == data_length);
    }

    auto to_owning() const -> ImageData<PixelFormatT>
    {
        return std::visit(
            overloaded{
                [&](uint8_t const* data) {
                    auto* res = new uint8_t[PixelFormatT::data_length(_resolution)]; // NOLINT(*owning-memory)
                    memcpy(res, data, PixelFormatT::data_length(_resolution));
                    return ImageData<PixelFormatT>{std::shared_ptr<uint8_t const>{res}, _resolution};
                },
                [&](std::shared_ptr<uint8_t const> const& data) {
                    return ImageData<PixelFormatT>{data, _resolution};
                },
            },
            _data
        );
    }

    auto data() const -> uint8_t const*
    {
        return std::visit(
            overloaded{
                [](uint8_t const* data) {
                    return data;
                },
                [](std::shared_ptr<uint8_t const> const& data) {
                    return data.get();
                },
            },
            _data
        );
    }

    auto resolution() const -> Resolution { return _resolution; }

private:
    std::variant<uint8_t const*, std::shared_ptr<uint8_t const>> _data{};
    Resolution                                                   _resolution{};
};

class Image {
public:
    Image()                                    = default;
    virtual ~Image()                           = default;
    Image(Image const&)                        = delete;
    auto operator=(Image const&) -> Image&     = delete;
    Image(Image&&) noexcept                    = delete;
    auto operator=(Image&&) noexcept -> Image& = delete;

    auto resolution() const -> Resolution { return _resolution; }
    auto width() const -> Resolution::DataType { return _resolution.width(); }
    auto height() const -> Resolution::DataType { return _resolution.height(); }
    auto row_order() const -> wcam::FirstRowIs { return _row_order; }

    virtual void set_data(ImageDataView<RGB24> const&) = 0;
    virtual void set_data(ImageDataView<BGR24> const&);
    virtual void set_data(ImageDataView<NV12> const&);

    void set_resolution(Resolution resolution) { _resolution = resolution; }
    void set_row_order(wcam::FirstRowIs row_order) { _row_order = row_order; }

private:
    Resolution       _resolution{};
    wcam::FirstRowIs _row_order{};
};

} // namespace wcam