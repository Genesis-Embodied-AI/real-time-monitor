#ifndef RTM_LIB_PARSER_H
#define RTM_LIB_PARSER_H

#include <array>
#include <chrono>
#include <string>

#include "rtm/time_wrapper.h"
#include "rtm/error.h"
#include "rtm/io.h"
#include "rtm/commands.h"

namespace rtm
{
    struct TickHeader
    {
        uint16_t version;
        std::array<uint8_t, 16> uuid;
        nanoseconds start_time;
        std::string process;    // source of the data (i.e. a processus name)
        std::string name;       // name of the serie (i.e. a thread name)
        uint64_t data_section_offset;
        uint16_t data_version;
    };

    class Parser
    {
    public:
        Parser(std::unique_ptr<AbstractReadIO> io);
        ~Parser() = default;

        void load_header();

        // TODO: support begin/end
        // if end = 0, takes alls samples
        // if begin/end is negative, reference is end, otherwise it is begin (relative)
        void load_samples();

        TickHeader const& header() const                { return header_;    }
        std::vector<nanoseconds> const& samples() const { return samples_;   }

        struct Point
        {
            float x;
            float y;
        };
        std::vector<Point> generate_times_diff();
        std::vector<Point> generate_times_up();

        nanoseconds begin() const;          // only available after a call to load_samples()
        nanoseconds end() const;            // only available after a call to load_samples()

        milliseconds_f diff_max() const ;   // only available after a call to generate_times_diff
        milliseconds_f up_max() const;      // only available after a call to generate_times_up

    private:
        std::unique_ptr<AbstractReadIO> io_;

        TickHeader header_;
        nanoseconds begin_{-1ns};
        nanoseconds end_{-1ns};
        milliseconds_f diff_max_{-1ns};
        milliseconds_f up_max_{-1ns};
        std::vector<nanoseconds> samples_;
    };

    // Helper to downsample big series
    // First pass with min/max to decimate a bit
    // Second pass with LTTB
    result<std::vector<Parser::Point>> minmax_lttb(std::vector<Parser::Point> const& series, uint32_t threshold);

    // LTTB downsampler
    result<std::vector<Parser::Point>> lttb(std::vector<Parser::Point> const& s, uint32_t threshold);

    // Min/Max downsampler
    result<std::vector<Parser::Point>> minmax_downsampler(std::vector<Parser::Point> const& series, uint32_t threshold);
}

#endif
