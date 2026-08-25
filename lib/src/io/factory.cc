#include "io/factory.h"
#include "io/file.h"
#include "io/null.h"
#include "io/posix/local_socket.h"
#include "io/posix/tcp_socket.h"
#include "io/posix/udp_socket.h"

namespace rtm
{
    std::unique_ptr<AbstractIO> make_null_io()
    {
        return std::make_unique<NullIO>();
    }

    std::unique_ptr<AbstractIO> make_file_io(std::string_view path)
    {
        return std::make_unique<File>(path);
    }

    std::unique_ptr<AbstractIO> make_local_socket_io(std::string_view path)
    {
        if (path.empty())
        {
            return std::make_unique<LocalSocket>(DEFAULT_LISTENING_PATH);
        }
        return std::make_unique<LocalSocket>(path);
    }

    std::unique_ptr<AbstractIO> make_tcp_io(std::string_view host, uint16_t port)
    {
        return std::make_unique<TcpSocket>(host, port);
    }

    std::unique_ptr<AbstractIO> make_udp_io(std::string_view host, uint16_t port,
                                            uint16_t bind_port)
    {
        if (host.empty())
        {
            return std::make_unique<UdpSocket>(bind_port);
        }
        return std::make_unique<UdpSocket>(host, port, bind_port);
    }
}
