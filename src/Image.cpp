#include "Image.hpp"
#include <cstddef>
#include <memory>

namespace wcam {

static auto clamp(int x) -> int
{
    return x < 0 ? 0 : (x > 255 ? 255 : x);
}

static auto BGR24_to_RGB24(uint8_t const* bgr_data, Resolution resolution) -> std::shared_ptr<uint8_t const>
{
    auto       rgb_data = std::shared_ptr<uint8_t>{new uint8_t[resolution.pixels_count() * 3], std::default_delete<uint8_t[]>()}; // NOLINT(*c-arrays)
    auto const width    = resolution.width();
    auto const height   = resolution.height();

    for (Resolution::DataType y = 0; y < height; y++)
    {
        for (Resolution::DataType x = 0; x < width; x++)
        {
            rgb_data.get()[(x + y * width) * 3 + 0] = bgr_data[(x + y * width) * 3 + 2]; // NOLINT(*pointer-arithmetic)
            rgb_data.get()[(x + y * width) * 3 + 1] = bgr_data[(x + y * width) * 3 + 1]; // NOLINT(*pointer-arithmetic)
            rgb_data.get()[(x + y * width) * 3 + 2] = bgr_data[(x + y * width) * 3 + 0]; // NOLINT(*pointer-arithmetic)
        }
    }

    return rgb_data;
}

static auto RGBA24_to_RGB24(uint8_t const* rgba_data, Resolution resolution) -> std::shared_ptr<uint8_t const>
{
    auto       rgb_data = std::shared_ptr<uint8_t>{new uint8_t[resolution.pixels_count() * 3], std::default_delete<uint8_t[]>()}; // NOLINT(*c-arrays)
    auto const width    = resolution.width();
    auto const height   = resolution.height();

    for (Resolution::DataType y = 0; y < height; y++)
    {
        for (Resolution::DataType x = 0; x < width; x++)
        {
            rgb_data.get()[(x + y * width) * 3 + 0] = rgba_data[(x + y * width) * 4 + 0]; // R
            rgb_data.get()[(x + y * width) * 3 + 1] = rgba_data[(x + y * width) * 4 + 1]; // G
            rgb_data.get()[(x + y * width) * 3 + 2] = rgba_data[(x + y * width) * 4 + 2]; // B
            // On ignore le canal Alpha (rgba_data[(x + y * width) * 4 + 3])
        }
    }

    return rgb_data;
}


static auto NV12_to_RGB24(uint8_t const* nv12Data, Resolution resolution) -> std::shared_ptr<uint8_t const>
{
    auto       rgb_data   = std::shared_ptr<uint8_t>{new uint8_t[resolution.pixels_count() * 3], std::default_delete<uint8_t[]>()}; // NOLINT(*c-arrays)
    auto const frame_size = resolution.pixels_count();
    auto const width      = resolution.width();
    auto const height     = resolution.height();

    uint8_t const* const y_plane  = nv12Data;
    uint8_t const* const uv_plane = nv12Data + frame_size; // NOLINT(*pointer-arithmetic)

    for (Resolution::DataType y = 0; y < height; y++)
    {
        for (Resolution::DataType x = 0; x < width; x++)
        {
            auto const y_index  = y * width + x;
            auto const uv_index = (y / 2) * (width / 2) + (x / 2);

            uint8_t const Y = y_plane[y_index];                            // NOLINT(*pointer-arithmetic)
            uint8_t const U = uv_plane[static_cast<size_t>(uv_index * 2)]; // NOLINT(*pointer-arithmetic)
            uint8_t const V = uv_plane[uv_index * 2 + 1];                  // NOLINT(*pointer-arithmetic)

            int const C = Y - 16;
            int const D = U - 128;
            int const E = V - 128;

            int const R = clamp((298 * C + 409 * E + 128) >> 8);
            int const G = clamp((298 * C - 100 * D - 208 * E + 128) >> 8);
            int const B = clamp((298 * C + 516 * D + 128) >> 8);

            rgb_data.get()[(x + y * width) * 3 + 0] = static_cast<uint8_t>(R); // NOLINT(*pointer-arithmetic)
            rgb_data.get()[(x + y * width) * 3 + 1] = static_cast<uint8_t>(G); // NOLINT(*pointer-arithmetic)
            rgb_data.get()[(x + y * width) * 3 + 2] = static_cast<uint8_t>(B); // NOLINT(*pointer-arithmetic)
        }
    }

    return rgb_data;
}

static auto YUYV_to_RGB24(uint8_t const* yuyv, Resolution resolution) -> std::shared_ptr<uint8_t const>
{
    auto rgb_data = std::shared_ptr<uint8_t>{new uint8_t[resolution.pixels_count() * 3], std::default_delete<uint8_t[]>()}; // NOLINT(*c-arrays)
    for (uint64_t i = 0; i < resolution.pixels_count() * 2; i += 4)
    {
        auto const y0 = static_cast<int>(yuyv[i + 0] << 8);  // NOLINT(*pointer-arithmetic)
        auto const u  = static_cast<int>(yuyv[i + 1] - 128); // NOLINT(*pointer-arithmetic)
        auto const y1 = static_cast<int>(yuyv[i + 2] << 8);  // NOLINT(*pointer-arithmetic)
        auto const v  = static_cast<int>(yuyv[i + 3] - 128); // NOLINT(*pointer-arithmetic)

        int const r0 = (y0 + 359 * v) >> 8;
        int const g0 = (y0 - 88 * u - 183 * v) >> 8;
        int const b0 = (y0 + 454 * u) >> 8;
        int const r1 = (y1 + 359 * v) >> 8;
        int const g1 = (y1 - 88 * u - 183 * v) >> 8;
        int const b1 = (y1 + 454 * u) >> 8;

        rgb_data.get()[i * 3 / 2 + 0] = static_cast<uint8_t>(std::clamp(r0, 0, 255)); // NOLINT(*pointer-arithmetic)
        rgb_data.get()[i * 3 / 2 + 1] = static_cast<uint8_t>(std::clamp(g0, 0, 255)); // NOLINT(*pointer-arithmetic)
        rgb_data.get()[i * 3 / 2 + 2] = static_cast<uint8_t>(std::clamp(b0, 0, 255)); // NOLINT(*pointer-arithmetic)
        rgb_data.get()[i * 3 / 2 + 3] = static_cast<uint8_t>(std::clamp(r1, 0, 255)); // NOLINT(*pointer-arithmetic)
        rgb_data.get()[i * 3 / 2 + 4] = static_cast<uint8_t>(std::clamp(g1, 0, 255)); // NOLINT(*pointer-arithmetic)
        rgb_data.get()[i * 3 / 2 + 5] = static_cast<uint8_t>(std::clamp(b1, 0, 255)); // NOLINT(*pointer-arithmetic)
    }

    return rgb_data;
}

void Image::set_data(ImageDataView<BGR24> const& bgrData)
{
    set_data(ImageDataView<RGB24>{
        BGR24_to_RGB24(bgrData.data(), bgrData.resolution()),
        RGB24::data_length(bgrData.resolution()),
        bgrData.resolution(),
        bgrData.row_order()
    });
}

void Image::set_data(ImageDataView<RGBA24> const& rgbaData)
{
    set_data(ImageDataView<RGB24>{
        RGBA24_to_RGB24(rgbaData.data(), rgbaData.resolution()),
        RGB24::data_length(rgbaData.resolution()),
        rgbaData.resolution(),
        rgbaData.row_order()
    });
}

void Image::set_data(ImageDataView<NV12> const& nv12_data)
{
    set_data(ImageDataView<RGB24>{
        NV12_to_RGB24(nv12_data.data(), nv12_data.resolution()),
        RGB24::data_length(nv12_data.resolution()),
        nv12_data.resolution(),
        nv12_data.row_order()
    });
}

void Image::set_data(ImageDataView<YUYV> const& yuyv_data)
{
    set_data(ImageDataView<RGB24>{
        YUYV_to_RGB24(yuyv_data.data(), yuyv_data.resolution()),
        RGB24::data_length(yuyv_data.resolution()),
        yuyv_data.resolution(),
        yuyv_data.row_order()
    });
}

} // namespace wcam