#pragma once
#include <algorithm>
#include <cstdint>
#include <string>

namespace wcam {

/// Represents the width and height of an axis-aligned rectangle.
/// width and height are guaranteed to be >= 1
class Resolution {
public:
    /// The type of width and height
    using DataType = uint32_t;

    Resolution() = default;
    Resolution(DataType w, DataType h)
        : _width{make_valid_size(w)}
        , _height{make_valid_size(h)}
    {}
    friend auto operator==(Resolution const& lhs, Resolution const& rhs) -> bool = default;

    auto width() const -> DataType { return _width; }
    auto height() const -> DataType { return _height; }
    auto pixels_count() const -> uint64_t { return static_cast<uint64_t>(_width) * static_cast<uint64_t>(_height); }

    /// Sets the width. If w < 1, it will be set to 1.
    void set_width(DataType w)
    {
        _width = make_valid_size(w);
    }

    /// Sets the height. If h < 1, it will be set to 1.
    void set_height(DataType h)
    {
        _height = make_valid_size(h);
    }

private:
    auto make_valid_size(DataType x) const -> DataType
    {
        return std::max(x, static_cast<DataType>(1));
    }

private:
    DataType _width{1};
    DataType _height{1};
};

auto to_string(Resolution) -> std::string;

} // namespace wcam