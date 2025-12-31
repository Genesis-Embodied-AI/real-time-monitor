#ifndef RTM_LIB_IO_NULL_H
#define RTM_LIB_IO_NULL_H

#include "rtm/io/io.h"

namespace rtm
{
    // Do nothing - stub
    class NullIO final : public AbstractIO
    {
    public:
        NullIO() = default;
        virtual ~NullIO() = default;

        int64_t read(void*, int64_t) override { return 0; };
        int64_t write(void const*, int64_t) override { return 0; }

        std::error_code seek(int64_t) override { return {}; }

    protected:
        std::error_code do_open(access::Mode) override { return {}; }
        std::error_code do_close() override { return {}; }
    };
}

#endif
