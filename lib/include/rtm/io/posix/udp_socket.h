#ifndef RTM_LIB_IO_POSIX_UDP_SOCKET_H
#define RTM_LIB_IO_POSIX_UDP_SOCKET_H

#include <cstdint>
#include <string>
#include <string_view>

#include "rtm/io/socket.h"

namespace rtm
{
    /// UDP socket that can operate in connected or unconnected mode.
    /// In connected mode (host + port provided), read/write use the connected peer.
    /// Use bind_port to receive on a specific port.
    class UdpSocket final : public AbstractSocket
    {
    public:
        /// Create a UDP socket bound to a local port (receiver side).
        UdpSocket(uint16_t bind_port);

        /// Create a UDP socket targeting a remote host:port (sender side).
        /// If bind_port is non-zero, the socket is also bound locally.
        UdpSocket(std::string_view remote_host, uint16_t remote_port, uint16_t bind_port = 0);

        ~UdpSocket();

    private:
        std::error_code do_open(access::Mode mode) override;

        std::string remote_host_;
        uint16_t remote_port_{0};
        uint16_t bind_port_{0};
    };
}

#endif
