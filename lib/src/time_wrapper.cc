#include "rtm/time_wrapper.h"

namespace rtm
{
    nanoseconds since_epoch()
    {
        auto now = time_point_cast<nanoseconds>(system_clock::now());
        return now.time_since_epoch();
    }

    static nanoseconds const START_TIME = since_epoch();
    nanoseconds start_time()
    {
        return START_TIME;
    }
}
