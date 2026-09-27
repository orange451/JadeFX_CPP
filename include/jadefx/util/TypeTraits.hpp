#pragma once

#include <istream>
#include <ostream>
#include <type_traits>
#include <utility>

// Which operators a type has, so templates can pick a default that compiles.
namespace jadefx::detail {

template <typename T, typename = void>
struct Streamable : std::false_type {};
template <typename T>
struct Streamable<T, std::void_t<decltype(std::declval<std::ostream&>() << std::declval<const T&>())>>
    : std::true_type {};

template <typename T, typename = void>
struct Parsable : std::false_type {};
template <typename T>
struct Parsable<T, std::void_t<decltype(std::declval<std::istream&>() >> std::declval<T&>())>> : std::true_type {};

template <typename T, typename = void>
struct LessComparable : std::false_type {};
template <typename T>
struct LessComparable<T, std::void_t<decltype(std::declval<const T&>() < std::declval<const T&>())>>
    : std::true_type {};

}  // namespace jadefx::detail
