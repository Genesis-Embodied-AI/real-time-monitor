#ifndef RTM_LIB_TIME_WRAPPER_H
#define RTM_LIB_TIME_WRAPPER_H

#include <chrono>

namespace rtm
{
    using namespace std::chrono;
    using milliseconds_f = std::chrono::duration<float, std::milli>;
    using seconds_f = std::chrono::duration<float>;

    // return the time in ns since epoch
    nanoseconds since_epoch();

    // return processus start time (since epoch)
    nanoseconds start_time();
}

#endif
