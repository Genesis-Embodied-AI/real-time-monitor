#ifndef RTM_LIB_COMMANDS_H
#define RTM_LIB_COMMANDS_H

#include <cstdint>

namespace rtm
{
    constexpr uint32_t ESCAPE = (1u << 31);
    enum Command
    {
        UPDATE_PERIOD    = (1 << 0),
        UPDATE_PRIORITY  = (1 << 1),
        UPDATE_REFERENCE = (1 << 2),
    };
}

#endif
