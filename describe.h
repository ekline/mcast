/* LICENSE_BEGIN

    Apache 2.0 License

    SPDX:Apache-2.0

    https://spdx.org/licenses/Apache-2.0

    See LICENSE file in the top level directory.

LICENSE_END */

#ifndef MCAST_DESCRIBE_H
#define MCAST_DESCRIBE_H


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

    return str.str();
}

}  // namespace mcast

#endif  // MCAST_DESCRIBE_H
