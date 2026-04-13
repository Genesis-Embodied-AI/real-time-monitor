#ifndef RTM_LIB_PARSER_H
#define RTM_LIB_PARSER_H

#include <array>
#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "rtm/commands.h"
#include "rtm/data.h"
#include "rtm/error.h"
#include "rtm/io/io.h"
#include "rtm/metadata.h"
#include "rtm/os/time.h"

namespace rtm
{
    struct TickHeader
    {
        uint16_t major;
        uint16_t minor;
        std::array<uint8_t, 16> uuid;
        nanoseconds start_time;
        std::string process;    // source of the data (i.e. a processus name)
        std::string name;       // name of the serie (i.e. a thread name)
        int64_t metadata_footer_offset{0};
        int64_t sentinel_pos{0};
        uint64_t data_section_offset;
        uint16_t data_version;
        std::string original_name;

        int64_t needs_sentinel_repair() const
        {
            if (sentinel_pos < 0)
            {
                return -sentinel_pos;
            }
            return 0;
        }
    };

    // Builds a v2 tick file header in a byte buffer:
    //   major/minor + padding + data_offset + uuid + start_time +
    //   process_str + task_str + metadata_footer_offset +
    //   alignment padding + data_version + data_padding
    // data_offset is resolved in-place once the padding has been applied, so the
    // returned buffer is ready to write verbatim at file offset 0.
    std::vector<uint8_t> build_tick_header(
        std::array<uint8_t, 16> const& uuid,
        nanoseconds start_time,
        std::string_view process,
        std::string_view task);

    class Parser
    {
    public:
        Parser(std::unique_ptr<AbstractIO> io);
        ~Parser() = default;

        void load_header();
        void print_header();

        // TODO: support begin/end
        // if end = 0, takes alls samples
        // if begin/end is negative, reference is end, otherwise it is begin (relative)
        bool load_samples();

        void load_metadata();

        TickHeader const& header() const                   { return header_;    }
        TickMetadata const& metadata() const               { return metadata_;  }
        std::vector<nanoseconds> const& samples() const    { return samples_;   }

        std::vector<Point> generate_times_diff();
        std::vector<Point> generate_times_up();

        nanoseconds begin() const;          // only available after a call to load_samples()
        nanoseconds end() const;            // only available after a call to load_samples()

        milliseconds_f diff_min() const;    // only available after a call to generate_times_diff
        milliseconds_f diff_max() const;    // only available after a call to generate_times_diff
        milliseconds_f up_min() const;      // only available after a call to generate_times_up
        milliseconds_f up_max() const;      // only available after a call to generate_times_up

    private:
        std::unique_ptr<AbstractIO> io_;

        TickHeader header_;
        TickMetadata metadata_;
        nanoseconds begin_{-1ns};
        nanoseconds end_{-1ns};
        milliseconds_f diff_min_{nanoseconds::max()};
        milliseconds_f diff_max_{-1ns};
        milliseconds_f up_min_{nanoseconds::max()};
        milliseconds_f up_max_{-1ns};
        std::vector<nanoseconds> samples_;
    };
}

#endif
