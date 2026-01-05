#include "error.h"
#include "os/time.h"

namespace rtm
{    
    void sleep(nanoseconds ns)
    {
        // convert chrono to OS timespec
        auto secs = duration_cast<seconds>(ns);
        nanoseconds nsecs = (ns - secs);
        timespec remaining_time{secs.count(), nsecs.count()};

        while (true)
        {
            timespec required_time = remaining_time;
            
            // nanosleep returns -1 on error and sets errno
            if (nanosleep(&required_time, &remaining_time) == 0)
            {
                return;
            }
            
            if (errno == EINTR)
            {
                // call interrupted by a POSIX signal: must sleep again.
                continue;
            }
            
            // only possible if timespec is wrongly defined
            throw std::system_error(errno, std::system_category());
        }
    }
}
