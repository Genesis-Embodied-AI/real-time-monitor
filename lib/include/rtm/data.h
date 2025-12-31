#ifndef RTM_LIB_DATA_H
#define RTM_LIB_DATA_H

#include <cstdint>
#include <vector>

#include "rtm/error.h"

namespace rtm
{
    struct Point
    {
        float x;
        float y;
    };

    // Helper to downsample big series
    // First pass with min/max to decimate a bit
    // Second pass with LTTB
    std::vector<Point> minmax_lttb(std::vector<Point> const& series, uint32_t threshold);

    // LTTB downsampler
    std::vector<Point> lttb(std::vector<Point> const& s, uint32_t threshold);

    // Min/Max downsampler
    std::vector<Point> minmax_downsampler(std::vector<Point> const& series, uint32_t threshold);
}

#endif
