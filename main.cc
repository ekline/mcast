/* LICENSE_BEGIN

    Apache 2.0 License

    SPDX:Apache-2.0

    https://spdx.org/licenses/Apache-2.0

    See LICENSE file in the top level directory.

LICENSE_END */

#include <unistd.h>

#include <iostream>
#include <sstream>
#include <string>

#include "describe.h"
#include "error.h"
#include "socket.h"

using namespace mcast;

void usage(const char *argv0) {
    std::cerr << "Usage: " << argv0 << "\n"
              << "    " << "[-g multicast_group]\n"
              << "    " << "[-p port]\n"
              << "    " << "[-l|-c]  # mode: listen|client\n"
              << "\n"
              << "Examples:\n"
              << "    " << "-g 224.0.0.251 -p 5353       # IPv4 mDNS\n"
              << "    " << "-g ff02::fb -p 5353          # IPv6 mDNS\n"
              << "    " << "-g 239.255.255.251 -p 10101  # google cast\n"
              << "\n";
}

enum class Mode {
    LISTEN,
    CLIENT
};

error::Errno prepareListeningSocket(socket::Socket& s,
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
                        socket::set(s, IPPROTO_IP, IP_MULTICAST_TTL, 4),
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
                        socket::set(s, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, 4),
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
            return error::Errno{EAFNOSUPPORT};
    }
}

int main(int argc, char * argv[]) {
    auto mc_dest_or{socket::from_string("239.255.255.251")};
    in_port_t port = 10101;
    // const int mtu = 1500;
    Mode mode{Mode::LISTEN};

    int ch{-1};
    while ((ch = getopt(argc, argv, "clg:p:")) != -1) {
        switch (ch) {
            case 'c':
                mode = Mode::CLIENT;
                break;
            case 'l':
                mode = Mode::LISTEN;
                break;
            case 'g':
                mc_dest_or = socket::from_string(optarg);
                break;
            case 'p': {
                const int specified_port{atoi(optarg)};
                if (specified_port > 0 && specified_port <= 0xffff) {
                    port = atoi(optarg);
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
        std::cerr << gai_strerror(get_error(mc_dest_or).num) << "\n";
        exit(-1);
    }
    auto mc_dest{get_valueref_unsafe(mc_dest_or)};
    socket::set_port(mc_dest, port);

    auto socket_or{socket::makeForFamily(mc_dest.ss_family)};
    if (not ok(socket_or)) {
        std::cerr << to_string(socket_or);
        exit(-1);
    }
    auto& s{get_valueref_unsafe(socket_or)};


    switch (mode) {
        case Mode::LISTEN: {
            auto e = prepareListeningSocket(s, mc_dest);
            if (not error::ok(e)) {
                std::cerr << error::to_string(e);
                exit(-1);
            }
            std::cerr << "Listening...\n";

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
            std::cerr << "Client mode not yet implemented\n";
            break;
        }
    }

    return 0;
}
