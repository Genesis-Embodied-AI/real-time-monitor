#ifndef RTM_PROBE_PARSER_H
#define RTM_PROBE_PARSER_H

#include <array>
#include <chrono>
#include <string>

#include "io.h"
#include "commands.h"

namespace rtm
{
    using namespace std::chrono;

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
        void load_samples(nanoseconds begin = 0ns, nanoseconds end = 0ns);

        TickHeader const& header() const                { return header_;    }
        nanoseconds begin() const                       { return begin_;     }
        nanoseconds end() const                         { return end_;       }
        std::vector<nanoseconds>& samples()             { return samples_;   }
        std::vector<nanoseconds> const& samples() const { return samples_;   }

    private:
        std::unique_ptr<AbstractReadIO> io_;

        TickHeader header_;
        nanoseconds begin_;
        nanoseconds end_;
        std::vector<nanoseconds> samples_;
    };
}

#endif
