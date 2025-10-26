#ifndef UTILS_H
#define UTILS_H

#include <type_traits>
#include <ostream>

template<typename T, typename = std::void_t<>>
struct is_streamable: std::false_type {};

template<typename T>
struct is_streamable<T, std::void_t<decltype(std::declval<std::ostream&>() << std::declval<T>())>> : std::true_type {};



#endif
