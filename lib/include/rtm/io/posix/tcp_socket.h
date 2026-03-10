#ifndef RTM_LIB_IO_POSIX_TCP_SOCKET_H
#define RTM_LIB_IO_POSIX_TCP_SOCKET_H

#include <cstdint>
#include <string>
#include <string_view>

#include "rtm/io/socket.h"

namespace rtm
{
    class TcpSocket final : public AbstractSocket
    {
    public:
        TcpSocket(std::string_view host, uint16_t port);
        ~TcpSocket();

    private:
        friend class TcpListener;
        TcpSocket(os_socket fd, access::Mode modes);

        std::error_code do_open(access::Mode mode) override;

        std::string host_;
        uint16_t port_;
    };


    class TcpListener final : public AbstractListener
    {
    public:
        TcpListener(std::string_view bind_address, uint16_t port);
        ~TcpListener();

        std::error_code listen(int backlog) override;
        std::unique_ptr<AbstractSocket> accept(access::Mode mode) override;

    private:
        std::string bind_address_;
        uint16_t port_;
    };
}

#endif
