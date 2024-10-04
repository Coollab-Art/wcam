#pragma once
#include <cassert>
#include <cstdint>
#include <memory>
#include "FirstRowIs.hpp"
#include "Resolution.hpp"

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
    ImageDataView(uint8_t const* data, size_t data_length, Resolution resolution)
        : _data{data}
        , _resolution{resolution}
    {
        assert(PixelFormatT::data_length(_resolution) == data_length);
    }
    auto copy() const -> ImageData<PixelFormatT> // TODO replace with to_owning ? Which will make a copy if we are not owning the _data pttr, and to a move if we are already owning the _data (eg when converting from nv12 to rgb then passing this copy to the rgb constructor)
    {
        auto* data = new uint8_t[PixelFormatT::data_length(_resolution)]; // NOLINT(*owning-memory)
        memcpy(data, _data, PixelFormatT::data_length(_resolution));
        return ImageData<PixelFormatT>{std::shared_ptr<uint8_t const>{data}, _resolution};
    }
    auto data() const -> uint8_t const* { return _data; }
    auto resolution() const -> Resolution { return _resolution; }

private:
    uint8_t const* _data{};
    Resolution     _resolution{};
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

    virtual void set_data(ImageDataView<RGB24>) = 0;
    virtual void set_data(ImageDataView<BGR24>);
    virtual void set_data(ImageDataView<NV12>);

    void set_resolution(Resolution resolution) { _resolution = resolution; }
    void set_row_order(wcam::FirstRowIs row_order) { _row_order = row_order; }

private:
    Resolution       _resolution{};
    wcam::FirstRowIs _row_order{};
};

} // namespace wcam