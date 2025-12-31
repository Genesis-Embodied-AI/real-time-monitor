#include "os/time.h"

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

    std::string format_iso_timestamp(nanoseconds timestamp)
    {
        constexpr uint32_t ISO_TIMESTAMP_LENGTH = 17;
        constexpr char ISO_TIMESTAMP_FORMAT[] = "%Y%m%dT%H%M%SZ";

        std::time_t const serie_start_time = duration_cast<seconds>(timestamp).count();
        char buffer[ISO_TIMESTAMP_LENGTH];

        std::tm utc_time;
        if (gmtime_r(&serie_start_time, &utc_time) == nullptr)
        {
            return "";
        }
        std::strftime(buffer, ISO_TIMESTAMP_LENGTH, ISO_TIMESTAMP_FORMAT, &utc_time);
        return std::string(buffer);
    }
}
