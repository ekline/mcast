/* LICENSE_BEGIN

    Apache 2.0 License

    SPDX:Apache-2.0

    https://spdx.org/licenses/Apache-2.0

    See LICENSE file in the top level directory.

LICENSE_END */

#ifndef MCAST_DESCRIBE_H
#define MCAST_DESCRIBE_H

#include <netdb.h>

#include <sstream>
#include <string>

#include "error.h"
#include "socket.h"

namespace mcast {

std::string describe(const socket::Message msg, ssize_t rcvd) {
    if (rcvd < 0) {
        return "error (see POSIX errno message)";
    }

    std::stringstream str{};

    str << "received " << rcvd << " bytes from ";
    switch (msg.ss.ss_family) {
        case AF_INET:
        case AF_INET6: {
            char hbuf[NI_MAXHOST]{}, sbuf[NI_MAXSERV]{};

            ::getnameinfo(msg.sockaddr_ptr(), msg.socklen(),
                          hbuf, sizeof(hbuf),
                          sbuf, sizeof(sbuf),
                          (NI_NUMERICHOST | NI_NUMERICSERV));
            str << "{" << hbuf << ", " << sbuf << "}";
            break;
        }

        default:
            str << "unknown address family: " << msg.ss.ss_family;
            return str.str();
    }

    return str.str();
}

}  // namespace mcast

#endif  // MCAST_DESCRIBE_H
