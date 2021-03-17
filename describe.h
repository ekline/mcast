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
    const std::string indent_short{"  "};
    const std::string indent_long{"    "};

    if (rcvd < 0) {
        return "error (see POSIX errno message)";
    }

    std::stringstream str{};

    str << "received " << rcvd << " bytes from " << socket::to_string(msg.ss);

    const int bytes_per_line{16};
    char buf[3]{};
    for (int i = 0; i < rcvd; i += bytes_per_line) {
        if (i == 0) str << "\n" << indent_short << "data:";
        str << "\n";

        // Print bytes as lowercase hexadecimal.
        str << indent_long;
        for (int j = 0; j < bytes_per_line; j++) {
            if (j % 2 == 0) str << " ";
            if (j % 8 == 0) str << " ";
            if (i + j < rcvd) {
                std::snprintf(buf, 3, "%02x", msg.pckt[i + j]);
            } else {
                buf[0] = ' ';
                buf[1] = ' ';
            }
            buf[2] = '\0';
            str << buf;
        }

        // Print any bytes that look like printable characters.
        str << indent_long;
        for (int j = 0; j < bytes_per_line && (i + j < rcvd); j++) {
            if (j % 2 == 0) str << " ";
            if (j % 8 == 0) str << " ";
            if (std::isgraph(msg.pckt[i + j]) != 0) {
                std::snprintf(buf, 2, "%c", msg.pckt[i + j]);
            } else {
                buf[0] = '.';
            }
            buf[1] = '\0';
            str << buf;
        }
    }

    str << "\n";
    return str.str();
}

}  // namespace mcast

#endif  // MCAST_DESCRIBE_H
