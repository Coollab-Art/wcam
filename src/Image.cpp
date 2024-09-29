#include "Image.hpp"
#include <memory>

namespace wcam {

static int clamp(int value)
{
    return value < 0 ? 0 : (value > 255 ? 255 : value);
}

static auto BGR24ToRGB24(uint8_t const* bgrData, Resolution resolution) -> std::unique_ptr<uint8_t const>
{
    auto*      rgbData = new uint8_t[resolution.pixels_count() * 3];
    auto const width   = resolution.width();
    auto const height  = resolution.height();

    for (Resolution::DataType j = 0; j < height; j++)
    {
        for (Resolution::DataType i = 0; i < width; i++)
        {
            rgbData[(i + j * width) * 3 + 0] = bgrData[(i + j * width) * 3 + 2];
            rgbData[(i + j * width) * 3 + 1] = bgrData[(i + j * width) * 3 + 1];
            rgbData[(i + j * width) * 3 + 2] = bgrData[(i + j * width) * 3 + 0];
        }
    }

    return std::unique_ptr<uint8_t const>(rgbData);
}

static auto NV12ToRGB24(uint8_t const* nv12Data, Resolution resolution) -> std::unique_ptr<uint8_t const>
{
    auto*                rgbData   = new uint8_t[resolution.pixels_count() * 3];
    Resolution::DataType frameSize = resolution.pixels_count();
    auto const           width     = resolution.width();
    auto const           height    = resolution.height();

    uint8_t const* yPlane  = nv12Data;
    uint8_t const* uvPlane = nv12Data + frameSize;

    for (Resolution::DataType j = 0; j < height; j++)
    {
        for (Resolution::DataType i = 0; i < width; i++)
        {
            Resolution::DataType yIndex  = j * width + i;
            Resolution::DataType uvIndex = (j / 2) * (width / 2) + (i / 2);

            uint8_t Y = yPlane[yIndex];
            uint8_t U = uvPlane[uvIndex * 2];
            uint8_t V = uvPlane[uvIndex * 2 + 1];

            int C = Y - 16;
            int D = U - 128;
            int E = V - 128;

            int R = clamp((298 * C + 409 * E + 128) >> 8);
            int G = clamp((298 * C - 100 * D - 208 * E + 128) >> 8);
            int B = clamp((298 * C + 516 * D + 128) >> 8);

            rgbData[(i + j * width) * 3 + 0] = (uint8_t)R;
            rgbData[(i + j * width) * 3 + 1] = (uint8_t)G;
            rgbData[(i + j * width) * 3 + 2] = (uint8_t)B;
        }
    }

    return std::unique_ptr<uint8_t const>(rgbData);
}

void Image::set_data(ImageDataView<BGR24> bgrData)
{
    std::unique_ptr<uint8_t const> const rgb24_data = BGR24ToRGB24(bgrData.data(), bgrData.resolution());
    set_data(ImageDataView<RGB24>{rgb24_data.get(), RGB24::data_length(bgrData.resolution()), bgrData.resolution()});
}

void Image::set_data(ImageDataView<NV12> nv12_data)
{
    std::unique_ptr<uint8_t const> const rgb24_data = NV12ToRGB24(nv12_data.data(), nv12_data.resolution());
    set_data(ImageDataView<RGB24>{rgb24_data.get(), RGB24::data_length(nv12_data.resolution()), nv12_data.resolution()});
}

} // namespace wcam