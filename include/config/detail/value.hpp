#pragma once

#include <cmath>
#include <concepts>
#include <cstdint>
#include <limits>
#include <nlohmann/json.hpp>
#include <ranges>
#include <stdexcept>
#include <type_traits>

namespace config::detail
{
// Custom deserializers and container elements must also own their returned data.
template <class T>
concept ReadableValue = std::is_object_v<T> && std::same_as<T, std::remove_cv_t<T>> && !std::is_pointer_v<T> &&
                        !std::ranges::view<T> && requires(const nlohmann::json &value) { value.template get<T>(); };

template <class T, class Value>
concept FallbackValue =
    std::convertible_to<Value, T> &&
    (!std::is_arithmetic_v<T> || (std::is_arithmetic_v<std::remove_cvref_t<Value>> &&
                                  (std::same_as<T, bool> == std::same_as<std::remove_cvref_t<Value>, bool>)));

template <class T, class Number> T checked_number(Number value)
{
    if constexpr (std::is_integral_v<T> && std::is_integral_v<Number>)
    {
        if constexpr (std::is_signed_v<Number>)
        {
            const auto wide = static_cast<std::intmax_t>(value);
            if constexpr (std::is_signed_v<T>)
            {
                if (wide < static_cast<std::intmax_t>((std::numeric_limits<T>::lowest)()) ||
                    wide > static_cast<std::intmax_t>((std::numeric_limits<T>::max)()))
                    throw std::range_error("Integer is outside the requested range");
            }
            else if (wide < 0 ||
                     static_cast<std::uintmax_t>(wide) > static_cast<std::uintmax_t>((std::numeric_limits<T>::max)()))
                throw std::range_error("Integer is outside the requested range");
        }
        else if (static_cast<std::uintmax_t>(value) > static_cast<std::uintmax_t>((std::numeric_limits<T>::max)()))
            throw std::range_error("Integer is outside the requested range");
    }
    else if constexpr (std::is_integral_v<T>)
    {
        // An exclusive power-of-two bound avoids rounding INT64_MAX to 2^63.
        const auto upper = std::ldexp(Number{1}, std::numeric_limits<T>::digits);
        const auto lower = std::is_signed_v<T> ? -upper : Number{0};
        if (!std::isfinite(value) || std::trunc(value) != value || value < lower || value >= upper)
            throw std::range_error("Number is fractional or outside the requested integer range");
    }
    else
    {
        const auto wide = static_cast<long double>(value);
        if (!std::isfinite(wide) || wide < static_cast<long double>((std::numeric_limits<T>::lowest)()) ||
            wide > static_cast<long double>((std::numeric_limits<T>::max)()))
            throw std::range_error("Number is outside the requested floating-point range");
        const auto result = static_cast<T>(value);
        if (value != 0 && result == 0)
            throw std::range_error("Number underflows the requested floating-point type");
        return result;
    }
    if constexpr (std::is_integral_v<T>)
        return static_cast<T>(value);
}

template <ReadableValue T> T value_as(const nlohmann::json &value)
{
    if constexpr (std::is_arithmetic_v<T> && !std::same_as<T, bool>)
    {
        if (value.is_number_unsigned())
            return checked_number<T>(value.template get<nlohmann::json::number_unsigned_t>());
        if (value.is_number_integer())
            return checked_number<T>(value.template get<nlohmann::json::number_integer_t>());
        if (value.is_number_float())
            return checked_number<T>(value.template get<nlohmann::json::number_float_t>());
        throw std::range_error("Expected a JSON number");
    }
    else
        return value.template get<T>();
}
} // namespace config::detail
