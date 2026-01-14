#ifndef RTM_LIB_OS_TIME_H
#define RTM_LIB_OS_TIME_H

#include <chrono>
#include <string>

namespace rtm
{
    using namespace std::chrono;
    using milliseconds_f = std::chrono::duration<double, std::milli>;
    using seconds_f = std::chrono::duration<double>;

    // return the time in ns since epoch
    nanoseconds since_epoch();

    // return processus start time (since epoch)
    nanoseconds start_time();

    std::string format_iso_timestamp(nanoseconds timestamp);

    void sleep(nanoseconds delay);
}

#endif
