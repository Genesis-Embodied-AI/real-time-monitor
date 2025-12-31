#ifndef RTM_LIB_IO_NULL_H
#define RTM_LIB_IO_NULL_H

#include "rtm/io.h"

namespace rtm
{
    // Do nothing - stub
    class NullIO final : public AbstractIO
    {
    public:
        NullIO() = default;
        virtual ~NullIO() = default;

        std::error_code open(access::Mode) override { return {}; }
        std::error_code close() override { return {}; }

        int64_t read(void*, std::size_t) override { return 0; };
        void write(void const*, std::size_t) override {}

        std::error_code seek(std::size_t) override { return {}; }
    };
}

#endif
