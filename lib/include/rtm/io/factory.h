// The one place that names a concrete io backend, so its header and the platform it is
// written for stay out of everything that only needs an AbstractIO.

#ifndef RTM_LIB_IO_FACTORY_H
#define RTM_LIB_IO_FACTORY_H

#include <cstdint>
#include <memory>
#include <string_view>

#include "rtm/io/io.h"

namespace rtm
{
    // Each returns an unopened io, so the caller picks its own access::Mode, and never null,
    // so a failure to reach the destination surfaces from open() alone.

    /// Discard sink: absorbs every write and reads nothing. A caller with no destination
    /// still holds a usable io.
    std::unique_ptr<AbstractIO> make_null_io();

    std::unique_ptr<AbstractIO> make_file_io(std::string_view path);

    /// An empty `path` dials the recorder's default listening path.
    std::unique_ptr<AbstractIO> make_local_socket_io(std::string_view path = {});

    std::unique_ptr<AbstractIO> make_tcp_io(std::string_view host, uint16_t port);

    /// An empty `host` binds `bind_port` to receive, rather than connecting out to a peer.
    std::unique_ptr<AbstractIO> make_udp_io(std::string_view host, uint16_t port,
                                            uint16_t bind_port = 0);
}

#endif
