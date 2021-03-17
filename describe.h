/* LICENSE_BEGIN

    Apache 2.0 License

    SPDX:Apache-2.0

    https://spdx.org/licenses/Apache-2.0

    See LICENSE file in the top level directory.

LICENSE_END */

#ifndef MCAST_DESCRIBE_H
#define MCAST_DESCRIBE_H


#include <cctype>
#include <cstdio>
#include <sstream>
#include <string>

#include "error.h"
#include "socket.h"

namespace mcast {

std::string describe(const socket::Msg msg, ssize_t rcvd) {
    if (rcvd < 0) {
        return "error (see POSIX errno message)";
    }

    std::stringstream str{};

    str << "received " << rcvd << " bytes from " << socket::to_string(msg.ss);

    const int bytes_per_line{16};
    const std::string indent{"    "};
    char buf[3]{};
    for (int i = 0; i < rcvd; i++) {
        if (i == 0) str << "\ndata:";
        str << "\n";

        str << indent;
        for (int j = 0; j < bytes_per_line; j++) {
            if (j % 4 == 0) str << " ";
            if (i + j < rcvd) {
                std::snprintf(buf, 3, "%02x", msg.pckt[i + j]);
            } else {
                std::snprintf(buf, 3, "  ");
            }
            buf[2] = '\0';
            str << buf;
        }

        str << indent;
        for (int j = 0; j < bytes_per_line; j++) {
            if (j % 4 == 0) str << " ";
            if (i + j < rcvd) {
                if (std::isgraph(msg.pckt[i + j]) != 0) {
                    std::snprintf(buf, 2, "%c", msg.pckt[i + j]);
                } else {
                    std::snprintf(buf, 2, ".");
                }
            } else {
                std::snprintf(buf, 2, " ");
            }
            buf[1] = '\0';
            str << buf;
        }

        i += bytes_per_line;
    }
    str << "\n";

    return str.str();
}

}  // namespace mcast

#endif  // MCAST_DESCRIBE_H
