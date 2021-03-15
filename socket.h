/* LICENSE_BEGIN

    Apache 2.0 License

    SPDX:Apache-2.0

    https://spdx.org/licenses/Apache-2.0

    See LICENSE file in the top level directory.

LICENSE_END */

#ifndef MCAST_SOCKET_H
#define MCAST_SOCKET_H

#include <netinet/in.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include <utility>

#include "error.h"

namespace mcast {
namespace socket {

struct Socket {
    Socket() = default;
    Socket(int sockfd) : fd((sockfd > -1) ? sockfd : -1) {}
    Socket(const Socket&) = delete;
    Socket(Socket&& other) {
        fd = std::exchange(other.fd, -1);
    }

    ~Socket() { if (fd > -1) ::close(fd); }

    Socket& operator=(const Socket&) = delete;
    Socket& operator=(Socket&& other) {
        fd = std::exchange(other.fd, -1);
        return *this;
    }

    int fd{-1};
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

struct Message {
    struct sockaddr_storage ss{};
    uint8_t cmsg[256]{};
    uint8_t pckt[1500]{};

    struct sockaddr* sockaddr_ptr() noexcept {
        return reinterpret_cast<struct sockaddr*>(&ss);
    }

    const struct sockaddr* sockaddr_ptr() const noexcept {
        return reinterpret_cast<const struct sockaddr*>(&ss);
    }

    socklen_t socklen() const noexcept {
        switch (ss.ss_family) {
            case AF_INET:  return sizeof(sockaddr_in);
            case AF_INET6: return sizeof(sockaddr_in6);
            default:       return sizeof(ss);
        }
    }
};

inline ErrorOr<ssize_t> recvmsg(Socket& s, Message& m) {
    memset(&m, 0, sizeof(m));
    m.ss.ss_family = AF_UNSPEC;

    struct iovec iov[1];
    iov[0].iov_base = m.pckt;
    iov[0].iov_len = sizeof(m.pckt);

    struct msghdr mhdr{};
    mhdr.msg_name = &(m.ss);
    mhdr.msg_namelen = sizeof(m.ss);
    mhdr.msg_iov = iov;
    mhdr.msg_iovlen = 1;
    mhdr.msg_control = m.cmsg;
    mhdr.msg_controllen = sizeof(m.cmsg);
    mhdr.msg_flags = 0;

    error::clear();

    const ssize_t rval = ::recvmsg(s.fd, &mhdr, 0);
    if (rval < 0) {
        return error::current();
    }
    return rval;
}

}  // namespace socket
}  // namespace mcast

#endif  // MCAST_SOCKET_H
