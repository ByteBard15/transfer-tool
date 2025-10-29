#ifndef UTILS_H
#define UTILS_H

#include <type_traits>
#include <ostream>
#include <sstream>

template<typename T, typename = std::void_t<>>
struct is_streamable: std::false_type {};

template<typename T>
struct is_streamable<T, std::void_t<decltype(std::declval<std::ostream&>() << std::declval<T>())>> : std::true_type {};

template<typename T>
std::string to_string_or_default(const T& arg, std::string default_value) {
    if constexpr (is_streamable<T>::value) {
        std::ostringstream stream;
        stream << arg;
        return stream.str();
    }
    return default_value;
}

template<typename T>
std::string to_string(const T& arg) {
    return to_string_or_default(arg, "");
}


#endif
