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
#include <net/if.h>
#include <netinet/in.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <functional>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <variant>
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

    return error::Error{rval, error::Category::ADDRINFO};
}

inline error::Error set_port(struct sockaddr_storage& ss, in_port_t port) {
    switch (ss.ss_family) {
        case AF_INET:
            sockaddr_in_ptr(ss)->sin_port = htons(port);
            return error::success();
        case AF_INET6:
            sockaddr_in6_ptr(ss)->sin6_port = htons(port);
            return error::success();
        default:
            return error::Error{EAFNOSUPPORT};
    }
}


inline std::string if_index2name(unsigned ifindex) {
    char buf[IFNAMSIZ+1]{};
    return if_indextoname(ifindex, buf);
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

inline error::Error enable(Socket& s, int optlvl, int optname) {
    error::clear();

    const int on{1};
    return error::from(::setsockopt(s.fd, optlvl, optname, &on, sizeof(on)));
}

inline error::Error disable(Socket& s, int optlvl, int optname) {
    error::clear();

    const int off{0};
    return error::from(::setsockopt(s.fd, optlvl, optname, &off, sizeof(off)));
}

template<typename T>
inline error::Error set(Socket& s, int optlvl, int optname, const T& t) {
    error::clear();

    return error::from(::setsockopt(s.fd, optlvl, optname, &t, sizeof(T)));
}

inline error::Error bind(Socket& s, const struct sockaddr_in& sin) {
    return error::from(::bind(s.fd,
                              reinterpret_cast<const struct sockaddr*>(&sin),
                              sizeof(struct sockaddr_in)));
}

inline error::Error bind(Socket& s, const struct sockaddr_in6& sin6) {
    return error::from(::bind(s.fd,
                              reinterpret_cast<const struct sockaddr*>(&sin6),
                              sizeof(struct sockaddr_in6)));
}

inline error::Error connect(Socket& s, const struct sockaddr_in& sin) {
    return error::from(
                ::connect(s.fd,
                          reinterpret_cast<const struct sockaddr*>(&sin),
                          sizeof(struct sockaddr_in)));
}

inline error::Error connect(Socket& s, const struct sockaddr_in6& sin6) {
    return error::from(
                ::connect(s.fd,
                          reinterpret_cast<const struct sockaddr*>(&sin6),
                          sizeof(struct sockaddr_in6)));
}

inline error::Error connect(Socket& s, const struct sockaddr_storage& ss) {
    return error::from(::connect(s.fd, sockaddr_ptr(ss), socklen(ss)));
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
        default: return error::Error{EAFNOSUPPORT};
    }
}


struct Msg {
    struct sockaddr_storage ss{};
    uint8_t cmsg[128]{};
    uint8_t pckt[2048 - sizeof(ss) - sizeof(cmsg)]{};
};

static_assert(sizeof(struct Msg) == 2048);

inline void clear(Msg& m) noexcept {
    memset(&(m.ss), 0, sizeof(m.ss));
    m.ss.ss_family = AF_UNSPEC;
    memset(m.cmsg, 0, sizeof(m.cmsg));
    memset(m.pckt, 0, sizeof(m.pckt));
}


struct MsgIO {
    struct msghdr mhdr{};
    struct iovec iov[1];

    static MsgIO from(Msg& m) {
        MsgIO mio{};

        mio.iov[0].iov_base     = m.pckt;
        mio.iov[0].iov_len      = sizeof(m.pckt);

        mio.mhdr.msg_name       = &(m.ss);
        mio.mhdr.msg_namelen    = sizeof(m.ss);
        mio.mhdr.msg_iov        = mio.iov;
        mio.mhdr.msg_iovlen     = 1;
        mio.mhdr.msg_control    = m.cmsg;
        mio.mhdr.msg_controllen = sizeof(m.cmsg);
        mio.mhdr.msg_flags      = 0;

        return mio;
    }

    static MsgIO from(const Msg& m) {
        MsgIO mio{};

        mio.iov[0].iov_base     = (void*)(m.pckt);
        mio.iov[0].iov_len      = sizeof(m.pckt);

        mio.mhdr.msg_name       = (void*)&(m.ss);
        mio.mhdr.msg_namelen    = sizeof(m.ss);
        mio.mhdr.msg_iov        = mio.iov;
        mio.mhdr.msg_iovlen     = 1;
        mio.mhdr.msg_control    = (void*)m.cmsg;
        mio.mhdr.msg_controllen = sizeof(m.cmsg);
        mio.mhdr.msg_flags      = 0;

        return mio;
    }
};


inline ErrorOr<ssize_t> recvmsg(Socket& s, Msg& m) {
    clear(m);
    auto mio{MsgIO::from(m)};

    error::clear();
    const ssize_t rval = ::recvmsg(s.fd, &(mio.mhdr), 0);
    if (rval < 0) {
        return error::current();
    }
    return rval;
}

inline ErrorOr<ssize_t> sendmsg(Socket& s, Msg& m, size_t len) {
    auto mio{MsgIO::from(m)};
    if (m.ss.ss_family == AF_UNSPEC) {
        // No destination address; hopefully the socket is connect()d.
        mio.mhdr.msg_name = nullptr;
        mio.mhdr.msg_namelen = 0;
    }
    mio.iov[0].iov_len = std::min(len, mio.iov[0].iov_len);

    auto* cmsg{reinterpret_cast<struct cmsghdr*>(mio.mhdr.msg_control)};
    if (cmsg->cmsg_len == 0) {
        // Apparently no options in the cmsg area.
        mio.mhdr.msg_control    = nullptr;
        mio.mhdr.msg_controllen = 0;
    }

    error::clear();
    const ssize_t rval = ::sendmsg(s.fd, &(mio.mhdr), 0);
    if (rval < 0) {
        return error::current();
    }
    return rval;
}


struct AuxiliaryData {
    std::optional<int> hoplimit{};
    std::optional<int> dscp{};
    std::variant<std::monostate,
                 struct in_pktinfo,
                 struct in6_pktinfo> pktinfo{};
};

inline bool has_hoplimit(const struct AuxiliaryData& aux) noexcept {
    return aux.hoplimit.has_value();
}

inline int get_hoplimit(const struct AuxiliaryData& aux) noexcept {
    return aux.hoplimit.value_or(-1);
}

inline void
set_hoplimit(struct AuxiliaryData& aux, const struct cmsghdr* cmsg) noexcept {
    if (cmsg == nullptr) return;

    int received_hops{0};
    memcpy(&received_hops, CMSG_DATA(cmsg),
           std::min(sizeof(received_hops),
                    static_cast<size_t>(cmsg->cmsg_len)));
    aux.hoplimit = received_hops & 0xff;
}

inline bool has_dscp(const struct AuxiliaryData& aux) noexcept {
    return aux.dscp.has_value();
}
inline int get_dscp(const struct AuxiliaryData& aux) noexcept {
    return aux.dscp.value_or(-1);
}

inline void
set_dscp(struct AuxiliaryData& aux, const struct cmsghdr* cmsg) noexcept {
    if (cmsg == nullptr) return;

    int received_dscp{0};
    memcpy(&received_dscp, CMSG_DATA(cmsg),
           std::min(sizeof(received_dscp),
                    static_cast<size_t>(cmsg->cmsg_len)));
    aux.dscp = received_dscp & 0xff;
}

inline bool has_pktinfo(const struct AuxiliaryData& aux) noexcept {
    return not std::holds_alternative<std::monostate>(aux.pktinfo);
}

inline unsigned get_pktinfo_interface(const struct AuxiliaryData& aux) {
    if (std::holds_alternative<struct in_pktinfo>(aux.pktinfo)) {
        return std::get_if<struct in_pktinfo>(&(aux.pktinfo))->ipi_ifindex;
    }
    if (std::holds_alternative<struct in6_pktinfo>(aux.pktinfo)) {
        return std::get_if<struct in6_pktinfo>(&(aux.pktinfo))->ipi6_ifindex;
    }

    return 0;
}

inline void
set_pktinfo4(struct AuxiliaryData& aux, const struct cmsghdr* cmsg) noexcept {
    if (cmsg == nullptr) return;

    struct in_pktinfo received_pktinfo{};
    memcpy(&received_pktinfo, CMSG_DATA(cmsg),
           std::min(sizeof(received_pktinfo),
                    static_cast<size_t>(cmsg->cmsg_len)));
    aux.pktinfo = received_pktinfo;
}

inline void
set_pktinfo6(struct AuxiliaryData& aux, const struct cmsghdr* cmsg) noexcept {
    if (cmsg == nullptr) return;

    struct in6_pktinfo received_pktinfo{};
    memcpy(&received_pktinfo, CMSG_DATA(cmsg),
           std::min(sizeof(received_pktinfo),
                    static_cast<size_t>(cmsg->cmsg_len)));
    aux.pktinfo = received_pktinfo;
}

struct AuxiliaryData parse_aux(const struct msghdr& mhdr) {
    struct AuxiliaryData aux{};

    // CMSG macros appear to require non-const msghdr.
    struct msghdr *msgp = const_cast<struct msghdr*>(&mhdr);

    for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(msgp);
         (cmsg != NULL) && (cmsg->cmsg_len > 0);
         cmsg = CMSG_NXTHDR(msgp, cmsg)) {
        switch (cmsg->cmsg_level) {
            case IPPROTO_IP:
                switch (cmsg->cmsg_type) {
                    case IP_PKTINFO:
                        set_pktinfo4(aux, cmsg);
                        break;

                    case IP_TOS:
                    case IP_RECVTOS:
                        set_dscp(aux, cmsg);
                        break;

                    case IP_TTL:
                    case IP_RECVTTL:
                        set_hoplimit(aux, cmsg);
                        break;

                    default:
                        // std::cerr << "unhandled cmsg_type: "
                        //           << cmsg->cmsg_type << "\n";
                        break;
                }
                break;

            case IPPROTO_IPV6:
                switch (cmsg->cmsg_type) {
                    case IPV6_HOPLIMIT:
                    case IPV6_RECVHOPLIMIT:
                        set_hoplimit(aux, cmsg);
                        break;

                    case IPV6_PKTINFO:
                        set_pktinfo6(aux, cmsg);
                        break;

                    case IPV6_TCLASS:
                    case IPV6_RECVTCLASS:
                        set_dscp(aux, cmsg);
                        break;

                    default:
                        // std::cerr << "unhandled cmsg_type: "
                        //           << cmsg->cmsg_type << "\n";
                        break;
                    }
                break;

            default:
                // std::cerr << "unhandled cmsg_level: "
                //           << cmsg->cmsg_level << "\n";
                break;
        }
    }

    return aux;
}

struct AuxiliaryData parse_aux(const Msg& m) {
    return parse_aux(MsgIO::from(m).mhdr);
}

}  // namespace socket
}  // namespace mcast

#endif  // MCAST_SOCKET_H
