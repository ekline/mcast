/* LICENSE_BEGIN

    Apache 2.0 License

    SPDX:Apache-2.0

    https://spdx.org/licenses/Apache-2.0

    See LICENSE file in the top level directory.

LICENSE_END */

#ifndef MCAST_ERROR_H
#define MCAST_ERROR_H

#include "netdb.h"

#include <cerrno>
#include <cstring>
#include <variant>

namespace mcast {
namespace error {

enum class Category {
    ERRNO,
    ADDRINFO,
};

struct Error {
    int num{0};
    Category category{Category::ERRNO};
};

inline constexpr Error success() noexcept { return Error{0}; }

inline bool ok(const Error& e) noexcept {
    return (e.num == 0);
}

inline void clear() noexcept {
    errno = 0;
}

inline Error&& current() noexcept {
    return std::move(Error{errno});
}

inline Error&& from(int rval) noexcept {
    return std::move(Error{(rval == 0) ? 0 : errno});
}

inline const char* to_string(const Error& e) {
    switch (e.category) {
        case Category::ERRNO: return std::strerror(e.num);
        case Category::ADDRINFO: return ::gai_strerror(e.num);
    }
}

}  // namespace error

template<typename T>
using ErrorOr = std::variant<mcast::error::Error, T>;

template<typename T>
error::Error get_error(const ErrorOr<T>& e) noexcept {
    if (std::holds_alternative<mcast::error::Error>(e)) {
        return *(std::get_if<mcast::error::Error>(&e));
    }
    return error::success();
}

template<typename T>
T& get_valueref_unsafe(ErrorOr<T>& e) noexcept {
    return *(std::get_if<T>(&e));
}

template<typename T>
const T& get_valueref_unsafe(const ErrorOr<T>& e) noexcept {
    return *(std::get_if<T>(&e));
}

template<typename T>
inline bool ok(const ErrorOr<T>& e) {
    return error::ok(get_error(e));
}

template<typename T>
inline const char* to_string(const ErrorOr<T>& e) {
    return error::to_string(get_error(e));
}

}  // namespace mcast

#endif  // MCAST_ERROR_H
