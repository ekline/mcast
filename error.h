/* LICENSE_BEGIN

    Apache 2.0 License

    SPDX:Apache-2.0

    https://spdx.org/licenses/Apache-2.0

    See LICENSE file in the top level directory.

LICENSE_END */

#ifndef MCAST_ERROR_H
#define MCAST_ERROR_H

#include <cerrno>
#include <cstring>
#include <variant>

namespace mcast {
namespace error {

struct Errno {
    int num{0};
};

inline constexpr Errno OK() noexcept { return Errno{0}; }

inline bool ok(const Errno& e) noexcept {
    return (e.num == 0);
}

inline void clear() noexcept {
    errno = 0;
}

inline Errno&& current() noexcept {
    return std::move(Errno{errno});
}

inline Errno&& from(int rval) noexcept {
    return std::move(Errno{(rval == 0) ? 0 : errno});
}

inline const char* to_string(const Errno& e) {
    return std::strerror(e.num);
}

}  // namespace error

template<typename T>
using ErrorOr = std::variant<mcast::error::Errno, T>;

template<typename T>
error::Errno get_error(const ErrorOr<T>& e) noexcept {
    if (std::holds_alternative<mcast::error::Errno>(e)) {
        return *(std::get_if<mcast::error::Errno>(&e));
    }
    return error::Errno{};
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
