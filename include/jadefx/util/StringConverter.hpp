#pragma once

#include <functional>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>

namespace jadefx {

namespace detail {

template <typename T, typename = void>
struct Streamable : std::false_type {};
template <typename T>
struct Streamable<T, std::void_t<decltype(std::declval<std::ostream&>() << std::declval<const T&>())>>
    : std::true_type {};

template <typename T, typename = void>
struct Parsable : std::false_type {};
template <typename T>
struct Parsable<T, std::void_t<decltype(std::declval<std::istream&>() >> std::declval<T&>())>> : std::true_type {};

}  // namespace detail

// Text for a value, the way a cell shows it. A string is itself, a bool is true
// or false, and a number or any other type with operator<< is what that writes.
// Types without one show nothing unless a converter says otherwise.
template <typename T>
std::string toDisplayString(const T& value) {
    if constexpr (std::is_convertible_v<const T&, std::string>) {
        return std::string(value);
    } else if constexpr (std::is_same_v<T, bool>) {
        return value ? "true" : "false";
    } else if constexpr (detail::Streamable<T>::value) {
        std::ostringstream out;
        out << value;
        return out.str();
    } else {
        return {};
    }
}

// Turns a value into text and back, in the shape of JavaFX's StringConverter.
// An editable cell shows toString and commits fromString. fromString returns
// nothing for text it cannot read, and the edit is then cancelled.
// A default-made converter uses toDisplayString, and reads a string as itself
// and anything else with operator>>.
template <typename T>
struct StringConverter {
    std::function<std::string(const T&)> toString;
    std::function<std::optional<T>(const std::string&)> fromString;

    std::string format(const T& value) const { return toString ? toString(value) : toDisplayString(value); }

    std::optional<T> parse(const std::string& text) const {
        if (fromString) {
            return fromString(text);
        }
        if constexpr (std::is_constructible_v<T, const std::string&>) {
            return T(text);
        } else if constexpr (detail::Parsable<T>::value && std::is_default_constructible_v<T>) {
            std::istringstream in(text);
            T value{};
            if (in >> value && (in >> std::ws).eof()) {
                return value;
            }
            return std::nullopt;
        } else {
            return std::nullopt;
        }
    }
};

}  // namespace jadefx
