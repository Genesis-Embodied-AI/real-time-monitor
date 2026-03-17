#ifndef RTM_LIB_RECORDER_H
#define RTM_LIB_RECORDER_H

#include <deque>
#include <memory>
#include <vector>

#include "rtm/io/io.h"
#include "rtm/os/time.h"

namespace rtm
{
    class Recorder
    {
    public:
        Recorder(std::string_view recording_path,
                 nanoseconds pre_duration = 120s,
                 nanoseconds post_duration = 120s);
        ~Recorder() = default;

        Recorder(Recorder&& other) = default;
        Recorder& operator=(Recorder&& other) = default;

        void add_client(std::unique_ptr<AbstractIO>&& io);
        void process();

    private:
        struct Chunk
        {
            nanoseconds first_sample_time{0};
            nanoseconds entry_reference{0};
            uint32_t sample_count{0};
            std::vector<uint8_t> data;
        };

        enum class Mode
        {
            PENDING,    // waiting for first data/command to decide recording strategy
            NORMAL,     // no threshold set, recording continuously to a single file
            BUFFERING,  // threshold set, filling ring buffer while waiting for a spike
            RECORDING,  // spike detected, writing to file until recording_deadline
        };

        struct Client
        {
            Client() = default;
            Client(Client&&) = default;
            Client& operator=(Client&&) = default;

            ~Client();
            void flush();

            std::unique_ptr<AbstractIO> io{};
            std::unique_ptr<AbstractIO> sink{};
            std::vector<uint8_t> buffer{};
            std::string name{};

            // Blackbox state
            nanoseconds threshold{0};
            nanoseconds current_reference{0};
            nanoseconds current_period{0};
            int32_t     current_priority{0};
            nanoseconds start_time{0};

            // Pair-aware sample tracking
            uint32_t    sample_parity{0};
            nanoseconds prev_start_absolute{0};
            bool        has_prev_start{false};
            bool        pending_trigger{false};

            // Ring buffer (pre-event data) -- pair-aligned
            std::deque<Chunk> ring;
            uint32_t ring_sample_count{0};

            // Header bytes (stored for re-use across files)
            std::vector<uint8_t> header_bytes;
            std::string process_name;
            std::string source_name;

            // Recording state
            Mode mode{Mode::PENDING};
            nanoseconds recording_deadline{0};
        };

        void parse_blackbox_data(Client& client);
        void trigger_recording(Client& client, nanoseconds trigger_absolute);
        void stop_recording(Client& client);
        void evict_ring(Client& client);

        std::vector<Client> clients_{};
        std::string recording_path_;
        nanoseconds pre_duration_;
        nanoseconds post_duration_;
    };
}

#endif
