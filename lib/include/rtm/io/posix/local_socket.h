#ifndef RTM_LIB_IO_POSIX_LOCAL_SOCKET_H
#define RTM_LIB_IO_POSIX_LOCAL_SOCKET_H

#include <string_view>

#include "rtm/io/socket.h"

namespace rtm
{
    class LocalListener : public AbstractListener
    {
    public:
        LocalListener(std::string_view local_path);
        virtual ~LocalListener();

        std::error_code listen(int backlog) override;
        std::unique_ptr<AbstractSocket> accept(access::Mode mode) override;

    private:
        std::string local_path_;
    };


    class LocalSocket : public AbstractSocket
    {
    public:
        LocalSocket(std::string_view address = "");
        LocalSocket(os_socket fd, access::Mode modes);
        virtual ~LocalSocket();

        std::error_code do_open(access::Mode mode) override;

    private:
        std::string local_path_;
    };
}

#endif
