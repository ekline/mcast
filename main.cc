/* LICENSE_BEGIN

    Apache 2.0 License

    SPDX:Apache-2.0

    https://spdx.org/licenses/Apache-2.0

    See LICENSE file in the top level directory.

LICENSE_END */

#include <arpa/inet.h>
#include <netdb.h>

#include <iostream>
#include <sstream>
#include <string>

#include "describe.h"
#include "error.h"
#include "socket.h"

using namespace mcast;

void usage(const char *argv0) {
    std::cerr << "Usage:\n"
              << "    " << argv0
              << "\n";
}

int main(int argc, char * argv[]) {
    if (argc > 1) {
        usage(argv[0]);
        exit(-1);
    }

    // TODO: things that should be command line flags
    const in_addr_t mc_dest = inet_addr("239.255.255.251");
    const in_port_t port = 10101;
    // const int mtu = 1500;

    auto socket_or{socket::makeIPv4()};
    if (not ok(socket_or)) {
        std::cerr << to_string(socket_or);
        exit(-1);
    }

    struct ip_mreqn mc_group{
        { mc_dest },
        { INADDR_ANY },
        0,
    };

    struct sockaddr_in listen4{};
    listen4.sin_family = AF_INET;
    listen4.sin_addr = { INADDR_ANY };
    listen4.sin_port = htons(port);

    auto& s{get_valueref_unsafe(socket_or)};
    for (const auto& e :
            {
                socket::enable(s, IPPROTO_IP, IP_RECVTOS),
                socket::enable(s, IPPROTO_IP, IP_RECVTTL),
                socket::enable(s, IPPROTO_IP, IP_PKTINFO),
#ifdef IP_MULTICAST_ALL  // not available on macOS
                socket::disable(s, IPPROTO_IP, IP_MULTICAST_ALL),
#endif
                socket::disable(s, IPPROTO_IP, IP_MULTICAST_LOOP),
                socket::set(s, IPPROTO_IP, IP_MULTICAST_TTL, 4),
                socket::set(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, mc_group),
                socket::bind(s, listen4)
            }) {
        if (not error::ok(e)) {
            std::cerr << error::to_string(e);
            exit(-1);
        }
    }

    std::cerr << "Listening...\n";

    socket::Message msg{};
    while (true) {
        const auto rval = socket::recvmsg(s, msg);
        if (not ok(rval)) {
            std::cerr << to_string(rval);
        }

        std::cout << describe(msg, get_valueref_unsafe(rval)) << "\n";
    }

    return 0;
}
