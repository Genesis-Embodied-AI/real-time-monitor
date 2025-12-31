#ifndef RTM_LIB_IO_POSIX_LOCAL_SOCKET_H
#define RTM_LIB_IO_POSIX_LOCAL_SOCKET_H

#include "rtm/io.h"

namespace rtm
{
    class LocalListener
    {
    public:
        LocalListener(std::string const& local_path);
        virtual ~LocalListener();

        void listen(int backLog);
        std::unique_ptr<AbstractSocket> accept();
    };


    class LocalSocket : public AbstractWriteIO
    {
    public:
        LocalSocket(std::string const& address="");
        virtual ~LocalSocket();


    private:
    };
}

#endif
