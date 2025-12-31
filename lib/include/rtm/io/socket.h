#ifndef RTM_LIB_IO_SOCKET_H
#define RTM_LIB_IO_SOCKET_H

#include <memory>

#include "rtm/io/io.h"
#include "rtm/os/types.h"

namespace rtm
{
    class AbstractListener;

    class AbstractSocket : public AbstractIO
    {
    public:
        AbstractSocket() = default;
        virtual ~AbstractSocket() = default;

        int64_t read(void* data, int64_t data_size) override;
        int64_t write(void const* data, int64_t data_size) override;

    protected:
        std::error_code do_close() override;
        os_socket fd_{};
    };


    class AbstractListener
    {
    public:
        AbstractListener() = default;
        virtual ~AbstractListener() = default;

        virtual std::error_code listen(int backlog) = 0;
        virtual std::unique_ptr<AbstractSocket> accept(access::Mode mode) = 0;

    protected:
        os_socket fd_{};
    };
}

#endif
