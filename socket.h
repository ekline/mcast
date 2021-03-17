/* LICENSE_BEGIN

    Apache 2.0 License

    SPDX:Apache-2.0

    https://spdx.org/licenses/Apache-2.0

    See LICENSE file in the top level directory.

LICENSE_END */

#ifndef MCAST_SOCKET_H
#define MCAST_SOCKET_H

#define __APPLE_USE_RFC_3542

#include <netdb.h>
#include <netinet/in.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include <functional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "error.h"

namespace mcast {
namespace socket {

inline struct sockaddr* sockaddr_ptr(struct sockaddr_storage& ss) noexcept {
    return reinterpret_cast<struct sockaddr*>(&ss);
}

inline const struct sockaddr*
sockaddr_ptr(const struct sockaddr_storage& ss) noexcept {
    return reinterpret_cast<const struct sockaddr*>(&ss);
}

inline struct sockaddr_in*
sockaddr_in_ptr(struct sockaddr_storage& ss) noexcept {
    return (ss.ss_family == AF_INET)
        ? reinterpret_cast<struct sockaddr_in*>(&ss)
        : nullptr;
}

inline const struct sockaddr_in*
sockaddr_in_ptr(const struct sockaddr_storage& ss) noexcept {
    return (ss.ss_family == AF_INET)
        ? reinterpret_cast<const struct sockaddr_in*>(&ss)
        : nullptr;
}

inline struct sockaddr_in6*
sockaddr_in6_ptr(struct sockaddr_storage& ss) noexcept {
    return (ss.ss_family == AF_INET6)
        ? reinterpret_cast<struct sockaddr_in6*>(&ss)
        : nullptr;
}

inline const struct sockaddr_in6*
sockaddr_in6_ptr(const struct sockaddr_storage& ss) noexcept {
    return (ss.ss_family == AF_INET6)
        ? reinterpret_cast<const struct sockaddr_in6*>(&ss)
        : nullptr;
}

inline socklen_t socklen(const struct sockaddr_storage& ss) noexcept {
    switch (ss.ss_family) {
        case AF_INET:  return sizeof(sockaddr_in);
        case AF_INET6: return sizeof(sockaddr_in6);
        default:       return sizeof(ss);
    }
}

inline std::string to_string(const struct sockaddr_storage& ss) noexcept {
    std::stringstream str{};

    char hbuf[NI_MAXHOST]{};
    char sbuf[NI_MAXSERV]{};
    switch (ss.ss_family) {
        case AF_INET:
        case AF_INET6: {
            ::getnameinfo(sockaddr_ptr(ss), socklen(ss),
                          hbuf, sizeof(hbuf),
                          sbuf, sizeof(sbuf),
                          (NI_NUMERICHOST | NI_NUMERICSERV));
            break;
        }

        default:
            str << "unknown address family: " << ss.ss_family;
            return str.str();
    }

    if (ss.ss_family == AF_INET6) { str << "["; }
    str << hbuf;
    if (ss.ss_family == AF_INET6) { str << "]"; }
    str << ":" << sbuf;

    return str.str();
}

inline ErrorOr<struct sockaddr_storage> from_string(const char* ip_literal) {
    struct sockaddr_storage ss{};
    ss.ss_family = AF_UNSPEC;

    struct addrinfo *res{nullptr};
    const struct addrinfo hints{
        .ai_flags = AI_NUMERICHOST,
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_DGRAM,
    };
    const int rval = ::getaddrinfo(ip_literal, nullptr, &hints, &res);
    if ((rval == 0) && (res != nullptr)) {
        if (res->ai_addr != nullptr) {
            memcpy(&ss, res->ai_addr,
                   std::min(static_cast<size_t>(res->ai_addrlen), sizeof(ss)));
        }
        ::freeaddrinfo(res);
        return ss;
    }

    return error::Errno{rval};
}

inline error::Errno set_port(struct sockaddr_storage& ss, in_port_t port) {
    switch (ss.ss_family) {
        case AF_INET:
            sockaddr_in_ptr(ss)->sin_port = htons(port);
            return error::OK();
        case AF_INET6:
            sockaddr_in6_ptr(ss)->sin6_port = htons(port);
            return error::OK();
        default:
            return error::Errno{EAFNOSUPPORT};
    }
}


struct Socket {
    Socket() = default;
    Socket(int sockfd) : fd((sockfd > -1) ? sockfd : -1) {}
    Socket(const Socket&) = delete;
    Socket(Socket&& other) {
        fd = std::exchange(other.fd, -1);
        std::swap(at_exit, other.at_exit);
    }

    ~Socket() {
        if (fd > -1) {
            for (auto& cleanup : at_exit) {
                cleanup();
            }
            ::close(fd);
        }
    }

    Socket& operator=(const Socket&) = delete;
    Socket& operator=(Socket&& other) {
        fd = std::exchange(other.fd, -1);
        std::swap(at_exit, other.at_exit);
        return *this;
    }

    int fd{-1};
    std::vector<std::function<void(void)>> at_exit{};
};

inline error::Errno enable(Socket& s, int optlvl, int optname) {
    error::clear();

    const int on{1};
    return error::from(::setsockopt(s.fd, optlvl, optname, &on, sizeof(on)));
}

inline error::Errno disable(Socket& s, int optlvl, int optname) {
    error::clear();

    const int off{0};
    return error::from(::setsockopt(s.fd, optlvl, optname, &off, sizeof(off)));
}

template<typename T>
inline error::Errno set(Socket& s, int optlvl, int optname, const T& t) {
    error::clear();

    return error::from(::setsockopt(s.fd, optlvl, optname, &t, sizeof(T)));
}

inline error::Errno bind(Socket& s, const struct sockaddr_in& sin) {
    return error::from(::bind(s.fd,
                              reinterpret_cast<const struct sockaddr*>(&sin),
                              sizeof(struct sockaddr_in)));
}

inline error::Errno bind(Socket& s, const struct sockaddr_in6& sin6) {
    return error::from(::bind(s.fd,
                              reinterpret_cast<const struct sockaddr*>(&sin6),
                              sizeof(struct sockaddr_in6)));
}

inline ErrorOr<mcast::socket::Socket> makeIPv4() {
    const int fd{::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)};
    if (fd < 0) {
        return error::current();
    }
    return Socket{fd};
}

inline ErrorOr<mcast::socket::Socket> makeIPv6() {
    const int fd{::socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP)};
    if (fd < 0) {
        return error::current();
    }

    Socket s{fd};
    const auto err{enable(s, IPPROTO_IPV6, IPV6_V6ONLY)};
    if (not error::ok(err)) {
        return err;
    }
    return s;
}

inline ErrorOr<mcast::socket::Socket> makeForFamily(int addr_family) {
    switch (addr_family) {
        case AF_INET: return makeIPv4();
        case AF_INET6: return makeIPv6();
        default: return error::Errno{EAFNOSUPPORT};
    }
}


struct Msg {
    struct sockaddr_storage ss{};
    uint8_t cmsg[256]{};
    uint8_t pckt[2048 - sizeof(ss) - sizeof(cmsg)]{};
};

static_assert(sizeof(struct Msg) == 2048);


struct MsgIO {
    struct msghdr mhdr{};
    struct iovec iov[1];

    static MsgIO from(Msg& m) {
        MsgIO mio{};

        mio.iov[0].iov_base = m.pckt;
        mio.iov[0].iov_len = sizeof(m.pckt);

        mio.mhdr.msg_name = &(m.ss);
        mio.mhdr.msg_namelen = sizeof(m.ss);
        mio.mhdr.msg_iov = mio.iov;
        mio.mhdr.msg_iovlen = 1;
        mio.mhdr.msg_control = m.cmsg;
        mio.mhdr.msg_controllen = sizeof(m.cmsg);
        mio.mhdr.msg_flags = 0;

        return mio;
    }
};


inline ErrorOr<ssize_t> recvmsg(Socket& s, Msg& m) {
    memset(&m, 0, sizeof(m));
    m.ss.ss_family = AF_UNSPEC;
    auto mio{MsgIO::from(m)};

    error::clear();
    const ssize_t rval = ::recvmsg(s.fd, &(mio.mhdr), 0);
    if (rval < 0) {
        return error::current();
    }
    return rval;
}


struct AuxiliaryData {
    std::variant<std::monostate, struct ip_mreqn, struct ipv6_mreq> mreq{};

    bool has_mreq() const noexcept {
        return not std::holds_alternative<std::monostate>(mreq);
    }
};

}  // namespace socket
}  // namespace mcast

#endif  // MCAST_SOCKET_H
