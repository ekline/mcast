/* LICENSE_BEGIN

    Apache 2.0 License

    SPDX:Apache-2.0

    https://spdx.org/licenses/Apache-2.0

    See LICENSE file in the top level directory.

LICENSE_END */

#define __APPLE_USE_RFC_3542

#include <stdio.h>
#include <unistd.h>

#include <iostream>
#include <sstream>
#include <string>

#include "describe.h"
#include "error.h"
#include "socket.h"

using namespace mcast;

void usage(const char *argv0) {
    const std::string space{"    "};

    std::cerr
        << "Usage: " << argv0 << "\n"
        << space << "[-g multicast_group]\n"
        << space << "[-p port]\n"
        << space << "[-m ip_mtu]  # including headers; client mode only\n"
        << space << "[-l|-c]      # mode: listen (default)|client\n"
        << "\n"
        << "Examples:\n"
        << space << "-g 224.0.0.251 -p 5353       # IPv4 mDNS\n"
        << space << "-g ff02::fb -p 5353          # IPv6 mDNS\n"
        << space << "-g 239.255.255.251 -p 10101  # google cast debug\n"
        << "\n";
}

enum class Mode {
    LISTEN,
    CLIENT
};

int adjust_mtu(int mtu, int addr_family) {
    // Basic bounds checking.
    if (mtu < 0) mtu = 0;
    mtu = std::min(sizeof(socket::Msg::pckt), static_cast<size_t>(mtu));
    mtu = std::min(1500, mtu);

    switch (addr_family) {
        case AF_INET:
            mtu = std::max(576, mtu);
            mtu -= 20;  // IPv4
            break;
        default:
            mtu = std::max(1280, mtu);
            mtu -= 40;  // IPv6
            break;
    }

    mtu -= 8;  // UDP
    return mtu;
}

error::Error prepareListenSocket(socket::Socket& s,
                                 const struct sockaddr_storage& mc_dest) {
    auto e = socket::enable(s, SOL_SOCKET, SO_REUSEADDR);
    if (not error::ok(e)) return e;
    e = socket::enable(s, SOL_SOCKET, SO_REUSEPORT);
    if (not error::ok(e)) return e;

    switch (mc_dest.ss_family) {
        case AF_INET: {
            struct ip_mreqn mc_group{
                socket::sockaddr_in_ptr(mc_dest)->sin_addr,
                { INADDR_ANY },
                0,
            };

            struct sockaddr_in listen4{};
            listen4.sin_family = AF_INET;
            listen4.sin_addr = { INADDR_ANY };
            listen4.sin_port = socket::sockaddr_in_ptr(mc_dest)->sin_port;

            for (const auto& e :
                    {
                        socket::enable(s, IPPROTO_IP, IP_RECVTOS),
                        socket::enable(s, IPPROTO_IP, IP_RECVTTL),
                        socket::enable(s, IPPROTO_IP, IP_PKTINFO),
                        socket::enable(s, IPPROTO_IP, IP_MULTICAST_LOOP),
#ifdef IP_MULTICAST_ALL  // not available on macOS
                        socket::disable(s, IPPROTO_IP, IP_MULTICAST_ALL),
#endif
                        socket::set(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, mc_group),
                        socket::bind(s, listen4)
                    }) {
                if (not error::ok(e)) {
                    return e;
                }
            }

            s.at_exit.push_back([&s, mc_group]() mutable {
                socket::set(s, IPPROTO_IP, IP_DROP_MEMBERSHIP, mc_group);
            });
            return error::OK();
        }

        case AF_INET6: {
            struct ipv6_mreq mc_group{
                socket::sockaddr_in6_ptr(mc_dest)->sin6_addr,
                0,
            };

            struct sockaddr_in6 listen6{};
            listen6.sin6_family = AF_INET6;
            listen6.sin6_addr = in6addr_any;
            listen6.sin6_port = socket::sockaddr_in6_ptr(mc_dest)->sin6_port;

            for (const auto& e :
                    {
                        socket::enable(s, IPPROTO_IPV6, IPV6_RECVTCLASS),
                        socket::enable(s, IPPROTO_IPV6, IPV6_RECVHOPLIMIT),
                        socket::enable(s, IPPROTO_IPV6, IPV6_RECVPKTINFO),
                        socket::enable(s, IPPROTO_IPV6, IPV6_MULTICAST_LOOP),
#ifdef IPV6_MULTICAST_ALL  // not available on macOS
                        socket::disable(s, IPPROTO_IPV6, IPV6_MULTICAST_ALL),
#endif
                        socket::set(s, IPPROTO_IPV6, IPV6_JOIN_GROUP, mc_group),
                        socket::bind(s, listen6)
                    }) {
                if (not error::ok(e)) {
                    return e;
                }
            }

            s.at_exit.push_back([&s, mc_group]() mutable {
                socket::set(s, IPPROTO_IPV6, IPV6_LEAVE_GROUP, mc_group);
            });
            return error::OK();
        }

        default:
            return error::Error{EAFNOSUPPORT};
    }
}

error::Error prepareClientSocket(socket::Socket& s,
                                 const struct sockaddr_storage& mc_dest) {
    switch (mc_dest.ss_family) {
        case AF_INET: {
            struct sockaddr_in client4{};
            client4.sin_family = AF_INET;
            client4.sin_addr = { INADDR_ANY };
            client4.sin_port = 0;

            for (const auto& e :
                    {
                        socket::set(s, IPPROTO_IP, IP_MULTICAST_TTL, 4),
                        socket::bind(s, client4),
                        socket::connect(s, mc_dest),
                    }) {
                if (not error::ok(e)) {
                    return e;
                }
            }

            return error::OK();
        }

        case AF_INET6: {
            struct sockaddr_in6 client6{};
            client6.sin6_family = AF_INET6;
            client6.sin6_addr = in6addr_any;
            client6.sin6_port = 0;

            for (const auto& e :
                    {
                        socket::set(s, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, 4),
                        socket::bind(s, client6),
                        socket::connect(s, mc_dest),
                    }) {
                if (not error::ok(e)) {
                    return e;
                }
            }

            return error::OK();
        }

        default:
            return error::Error{EAFNOSUPPORT};
    }
}

int main(int argc, char * argv[]) {
    auto mc_dest_or{socket::from_string("239.255.255.251")};
    in_port_t port = 10101;
    int mtu = 1500;
    Mode mode{Mode::LISTEN};

    int ch{-1};
    while ((ch = getopt(argc, argv, "cg:lm:p:")) != -1) {
        switch (ch) {
            case 'c':
                mode = Mode::CLIENT;
                break;
            case 'g':
                mc_dest_or = socket::from_string(optarg);
                break;
            case 'l':
                mode = Mode::LISTEN;
                break;
            case 'm': {
                const int specified_mtu{atoi(optarg)};
                if (specified_mtu > 0 && specified_mtu <= 1500) {
                    mtu = specified_mtu;
                } else {
                    std::cerr << "specified MTU invalid or out of range\n";
                    exit(-1);
                }
                break;
            }
            case 'p': {
                const int specified_port{atoi(optarg)};
                if (specified_port > 0 && specified_port <= 0xffff) {
                    port = specified_port;
                } else {
                    std::cerr << "specified port invalid or out of range\n";
                    exit(-1);
                }
                break;
            }
            default:
                usage(argv[0]);
                exit(-1);
        }
    }
    argc -= optind;
    argv += optind;

    if (not ok(mc_dest_or)) {
        std::cerr << to_string(mc_dest_or) << "\n";
        exit(-1);
    }
    auto mc_dest{get_valueref_unsafe(mc_dest_or)};
    socket::set_port(mc_dest, port);

    mtu = adjust_mtu(mtu, mc_dest.ss_family);
    std::cerr << "application-layer MTU: " << mtu << "\n";

    auto socket_or{socket::makeForFamily(mc_dest.ss_family)};
    if (not ok(socket_or)) {
        std::cerr << to_string(socket_or);
        exit(-1);
    }
    auto& s{get_valueref_unsafe(socket_or)};

    switch (mode) {
        case Mode::LISTEN: {
            auto e = prepareListenSocket(s, mc_dest);
            if (not error::ok(e)) {
                std::cerr << error::to_string(e);
                exit(-1);
            }
            std::cerr << "listening...\n";

            socket::Msg msg{};
            while (true) {
                const auto rval = socket::recvmsg(s, msg);
                if (not ok(rval)) {
                    std::cerr << to_string(rval);
                }

                std::cout << describe(msg, get_valueref_unsafe(rval)) << "\n";
            }
            break;
        }

        case Mode::CLIENT: {
            auto e = prepareClientSocket(s, mc_dest);
            if (not error::ok(e)) {
                std::cerr << error::to_string(e);
                exit(-1);
            }
            std::cerr << "copying from stdin to multicast sendmsg\n";

            socket::Msg msg{};
            while (true) {
                const auto consumed{fread(msg.pckt, 1, mtu, stdin)};
                if (consumed == 0) {
                    break;
                }
                const auto rval = socket::sendmsg(s, msg, consumed);
                if (not ok(rval)) {
                    std::cerr << to_string(rval);
                }

                std::cerr << "sent " << consumed << " bytes\n";
            }
            break;
        }
    }

    return 0;
}
