#include "Resolution.hpp"
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <valarray>

namespace wcam {

struct Fraction {
    int numerator{};
    int denominator{};
};

static auto as_fraction(float x) -> Fraction
{
    const int sign     = x > 0 ? 1 : -1;
    x                  = std::abs(x);
    float decimal_part = x - std::floor(x);

    std::valarray<int> fraction{static_cast<int>(std::floor(x)), 1};
    std::valarray<int> previous_fraction{1, 0};

    int         max_iterations = 10;
    float const precision      = 1e-6f;
    while (max_iterations > 0
           && decimal_part > precision)
    {
        --max_iterations;
        const float new_x               = 1.f / decimal_part;
        const float whole_part_as_float = std::floor(new_x);
        const int   whole_part          = static_cast<int>(whole_part_as_float);

        const auto temporary = fraction;
        fraction             = whole_part * fraction + previous_fraction;
        previous_fraction    = temporary;

        decimal_part = new_x - whole_part_as_float;
    }

    return {sign * fraction[0], fraction[1]};
}

static auto with_3_decimals(float x) -> std::string
{
    std::stringstream ss;
    ss << std::fixed << std::setprecision(3) << x;
    return ss.str();
}

static auto fraction_to_string(float ratio) -> std::string
{
    if (std::abs(ratio - 1.41421356f) < 0.001f)
        return "A4";
    if (std::abs(ratio - 0.70710678f) < 0.001f)
        return "A4 Vertical";
    Fraction const fraction = as_fraction(ratio);

    bool const fraction_is_small_enough =
        std::abs(fraction.numerator) <= 30
        && std::abs(fraction.denominator) <= 30;

    return fraction_is_small_enough
               ? std::to_string(fraction.numerator) + "/" + std::to_string(fraction.denominator)
               : with_3_decimals(ratio);
}

auto to_string(Resolution resolution) -> std::string
{
    return std::to_string(resolution.width()) + " x " + std::to_string(resolution.height()) + " (" + fraction_to_string(static_cast<float>(resolution.width()) / static_cast<float>(resolution.height())) + ")";
}

} // namespace wcam