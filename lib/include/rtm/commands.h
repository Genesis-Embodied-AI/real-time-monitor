#ifndef RTM_LIB_COMMANDS_H
#define RTM_LIB_COMMANDS_H

#include <chrono>
#include <cstdint>

#include "rtm/io/io.h"

namespace rtm
{
    using std::chrono::nanoseconds;

    constexpr uint16_t PROTOCOL_MAJOR = 2;
    constexpr uint16_t PROTOCOL_MINOR = 0;

    constexpr uint32_t ESCAPE = (1u << 31);
    enum Command
    {
        UPDATE_PERIOD    = (1 << 0),
        UPDATE_PRIORITY  = (1 << 1),
        UPDATE_REFERENCE = (1 << 2),
        SET_THRESHOLD    = (1 << 3),
        DATA_STREAM_END  = (1 << 4),
    };

    template<typename T>
    void write_command(AbstractIO& io, uint32_t command, T value)
    {
        uint32_t oob = ESCAPE | command;
        io.write(&oob, sizeof(oob));
        io.write(&value, sizeof(value));
    }

    inline void write_command(AbstractIO& io, uint32_t command, nanoseconds value)
    {
        uint64_t raw = static_cast<uint64_t>(value.count());
        write_command(io, command, raw);
    }
}

#endif
