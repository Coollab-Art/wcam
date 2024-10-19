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

struct YUYV {
    static auto data_length(Resolution resolution) -> size_t
    {
        return resolution.pixels_count() * 2;
    }
};

template<typename PixelFormatT>
class ImageData {
public:
    ImageData(std::shared_ptr<uint8_t const> data, Resolution resolution, wcam::FirstRowIs row_order)
        : _data{std::move(data)}
        , _resolution{resolution}
        , _row_order{row_order}
    {}
    auto data() const -> uint8_t const* { return _data.get(); }
    auto resolution() const -> Resolution { return _resolution; }
    auto row_order() const -> wcam::FirstRowIs { return _row_order; }

private:
    std::shared_ptr<uint8_t const> _data{};
    Resolution                     _resolution{};
    wcam::FirstRowIs               _row_order{};
};

template<typename PixelFormatT>
class ImageDataView {
public:
    ImageDataView(std::variant<uint8_t const*, std::shared_ptr<uint8_t const>> data, size_t data_length, Resolution resolution, wcam::FirstRowIs row_order)
        : _data{std::move(data)}
        , _resolution{resolution}
        , _row_order{row_order}
    {
        assert(PixelFormatT::data_length(_resolution) == data_length);
        std::ignore = data_length; // Disable warning in release
    }

    auto to_owning() const -> ImageData<PixelFormatT>
    {
        return std::visit(
            overloaded{
                [&](uint8_t const* data) {
                    auto res = std::shared_ptr<uint8_t>{new uint8_t[PixelFormatT::data_length(_resolution)], std::default_delete<uint8_t[]>()}; // NOLINT(*c-arrays)
                    memcpy(res.get(), data, PixelFormatT::data_length(_resolution));
                    return ImageData<PixelFormatT>{std::move(res), _resolution, _row_order};
                },
                [&](std::shared_ptr<uint8_t const> const& data) {
                    return ImageData<PixelFormatT>{data, _resolution, _row_order};
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
    auto row_order() const -> wcam::FirstRowIs { return _row_order; }

private:
    std::variant<uint8_t const*, std::shared_ptr<uint8_t const>> _data{};
    Resolution                                                   _resolution{};
    wcam::FirstRowIs                                             _row_order{};
};

class Image {
public:
    Image()                                    = default;
    virtual ~Image()                           = default;
    Image(Image const&)                        = delete;
    auto operator=(Image const&) -> Image&     = delete;
    Image(Image&&) noexcept                    = delete;
    auto operator=(Image&&) noexcept -> Image& = delete;

    virtual void set_data(ImageDataView<RGB24> const&) = 0;
    virtual void set_data(ImageDataView<BGR24> const&);
    virtual void set_data(ImageDataView<NV12> const&);
    virtual void set_data(ImageDataView<YUYV> const&);
};

} // namespace wcam