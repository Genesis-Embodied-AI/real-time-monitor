#include <algorithm>
#include <cerrno>
#include <filesystem>

#include "recorder.h"
#include "commands.h"
#include "parser.h"
#include "serializer.h"
#include "io/file.h"
#include "io/null.h"
#include "os/time.h"

namespace rtm
{
    Recorder::Client::~Client()
    {
        flush();
    }

    Recorder::Recorder(std::string_view recording_path,
                       nanoseconds pre_duration,
                       nanoseconds post_duration)
        : recording_path_{recording_path}
        , pre_duration_{std::max(pre_duration, nanoseconds(2s))}
        , post_duration_{std::max(post_duration, nanoseconds(2s))}
    {
        std::filesystem::create_directories(recording_path_);
    }

    void Recorder::Client::flush()
    {
        if (sink != nullptr)
        {
            sink->write(buffer.data(), static_cast<int64_t>(buffer.size()));
            sink->sync();
        }
        buffer.clear();
    }

    void Recorder::add_client(std::unique_ptr<AbstractIO>&& io)
    {
        Client client;
        client.io = std::move(io);
        client.sink = nullptr;
        client.buffer.reserve(4096);
        clients_.emplace_back(std::move(client));

        printf("[Recorder] New client\n");
    }

    void Recorder::evict_ring(Client& client)
    {
        nanoseconds cutoff = client.prev_start_absolute - pre_duration_;

        while (not client.ring.empty() and client.ring.front().first_sample_time < cutoff)
        {
            uint32_t front_count = client.ring.front().sample_count;
            uint32_t remaining = client.ring_sample_count - front_count;

            if (remaining % 2 != 0)
            {
                if (client.ring.size() < 2)
                {
                    break;
                }

                uint32_t next_count = client.ring[1].sample_count;
                client.ring_sample_count -= (front_count + next_count);
                client.ring.pop_front();
                client.ring.pop_front();
            }
            else
            {
                client.ring_sample_count -= front_count;
                client.ring.pop_front();
            }
        }
    }


    void Recorder::trigger_recording(Client& client, nanoseconds trigger_absolute)
    {
        long trigger_s = std::chrono::duration_cast<std::chrono::seconds>(trigger_absolute).count();

        std::string path = recording_path_ + '/';
        path += format_iso_timestamp(client.start_time);
        path += '_';
        path += client.process_name;
        path += '_';
        path += client.source_name;
        path += '_';
        path += std::to_string(trigger_s) + 's';
        path += ".tick";
        printf("[Recorder] Blackbox trigger %ldns @ %lds! Writing %s\n",
               static_cast<long>(client.detected_jitter.count()), trigger_s, path.c_str());

        client.sink = std::make_unique<File>(path);
        client.sink->open(access::Mode::WRITE_ONLY | access::Mode::TRUNCATE);

        // Rebuild header with a unique task name so the GUI can distinguish files
        std::string unique_task = client.source_name + "@" + std::to_string(trigger_s) + "s";
        {
            std::array<uint8_t, 16> uuid;
            std::memcpy(uuid.data(), client.header_bytes.data() + 16, uuid.size());

            std::vector<uint8_t> header = build_tick_header(
                uuid, client.start_time, client.process_name, unique_task);

            client.sink->write(header.data(), static_cast<int64_t>(header.size()));
        }
        write_command(*client.sink, Command::UPDATE_PERIOD, client.current_period);
        write_command(*client.sink, Command::UPDATE_PRIORITY, client.current_priority);

        // Skip ring chunks until we find one starting with UPDATE_REFERENCE.
        // Chunks split at reference boundaries are self-contained; earlier chunks
        // without a leading reference would produce an artificial spike in times_diff.
        std::size_t start_idx = client.ring.size();
        for (std::size_t i = 0; i < client.ring.size(); ++i)
        {
            if (client.ring[i].data.size() >= sizeof(uint32_t))
            {
                uint32_t first_word;
                std::memcpy(&first_word, client.ring[i].data.data(), sizeof(first_word));
                if (first_word == (ESCAPE | Command::UPDATE_REFERENCE))
                {
                    start_idx = i;
                    break;
                }
            }
        }

        if (start_idx == client.ring.size() and not client.ring.empty())
        {
            write_command(*client.sink, Command::UPDATE_REFERENCE, client.ring.front().entry_reference);
            uint32_t zero_delta = 0;
            client.sink->write(&zero_delta, sizeof(zero_delta));
            start_idx = 0;
        }

        for (std::size_t i = start_idx; i < client.ring.size(); ++i)
        {
            if (not client.ring[i].data.empty())
            {
                client.sink->write(client.ring[i].data.data(), static_cast<int64_t>(client.ring[i].data.size()));
            }
        }

        client.ring.clear();
        client.ring_sample_count = 0;

        client.mode = Mode::RECORDING;
        client.recording_deadline = trigger_absolute + post_duration_;
    }

    void Recorder::stop_recording(Client& client)
    {
        printf("[Recorder] Blackbox recording complete (%s_%s)\n",
               client.process_name.c_str(), client.source_name.c_str());

        if (client.sink != nullptr)
        {
            uint32_t sentinel = ESCAPE | Command::DATA_STREAM_END;
            client.sink->write(&sentinel, sizeof(sentinel));
            client.sink->sync();
            client.sink.reset();
        }

        client.mode = Mode::BUFFERING;
    }

    bool Recorder::parse_blackbox_data(Client& client)
    {
        bool end_of_stream = false;
        uint8_t const* pos = client.buffer.data();
        uint8_t const* const buf_end = pos + client.buffer.size();

        Chunk current_chunk;
        current_chunk.entry_reference = client.current_reference;
        current_chunk.sample_count = 0;

        auto route = [&](uint8_t const* from, std::size_t len)
        {
            if (client.mode == Mode::BUFFERING)
            {
                current_chunk.data.insert(current_chunk.data.end(), from, from + len);
            }
            else if (client.mode == Mode::RECORDING and client.sink != nullptr)
            {
                client.sink->write(from, static_cast<int64_t>(len));
            }
        };

        auto fire_trigger = [&](nanoseconds absolute, uint8_t const* elem_start, uint8_t const* elem_end)
        {
            current_chunk.data.insert(current_chunk.data.end(), elem_start, elem_end);
            current_chunk.sample_count++;

            client.sample_parity = 0;
            client.pending_trigger = false;

            client.ring_sample_count += current_chunk.sample_count;
            client.ring.push_back(std::move(current_chunk));

            trigger_recording(client, absolute);

            current_chunk = Chunk{};
            current_chunk.entry_reference = client.current_reference;
            current_chunk.sample_count = 0;
        };

        auto process_sample = [&](nanoseconds absolute, uint8_t const* elem_start, uint8_t const* elem_end) -> bool
        {
            if (client.sample_parity == 0)
            {
                if (client.has_prev_start and client.threshold.count() > 0)
                {
                    nanoseconds jitter = absolute - client.prev_start_absolute;
                    if (jitter > client.threshold)
                    {
                        client.pending_trigger = true;
                        client.detected_jitter = jitter;
                    }
                }
                client.prev_start_absolute = absolute;
                client.has_prev_start = true;
            }
            else if (client.pending_trigger)
            {
                if (client.mode == Mode::BUFFERING)
                {
                    fire_trigger(absolute, elem_start, elem_end);
                    return true;
                }
                else if (client.mode == Mode::RECORDING)
                {
                    client.recording_deadline = absolute + post_duration_;
                    client.pending_trigger = false;
                }
            }

            if (current_chunk.sample_count == 0)
            {
                current_chunk.first_sample_time = absolute;
            }

            client.sample_parity = (client.sample_parity + 1) % 2;
            current_chunk.sample_count++;
            route(elem_start, static_cast<std::size_t>(elem_end - elem_start));

            if (client.mode == Mode::RECORDING and client.sample_parity == 0 and absolute >= client.recording_deadline)
            {
                if (client.sink != nullptr)
                {
                    client.sink->sync();
                }
                stop_recording(client);
                current_chunk = Chunk{};
                current_chunk.entry_reference = client.current_reference;
            }

            return false;
        };

        while (pos + sizeof(uint32_t) <= buf_end)
        {
            uint8_t const* elem_start = pos;
            uint32_t raw = extract_data<uint32_t>(pos);

            if (raw & ESCAPE)
            {
                if (raw == (ESCAPE | Command::DATA_STREAM_END))
                {
                    // Sentinel = clean disconnection. Any bytes remaining in
                    // client.buffer after this are post-disconnect garbage and
                    // are intentionally dropped by the caller (io is reset).
                    end_of_stream = true;
                    break;
                }

                if (raw & Command::SET_THRESHOLD)
                {
                    if (pos + sizeof(uint64_t) > buf_end)
                    {
                        pos = elem_start;
                        break;
                    }
                    client.threshold = nanoseconds(extract_data<uint64_t>(pos));
                    continue;
                }

                if (raw & Command::UPDATE_REFERENCE)
                {
                    if (pos + sizeof(uint64_t) > buf_end)
                    {
                        pos = elem_start;
                        break;
                    }
                    client.current_reference = nanoseconds(extract_data<uint64_t>(pos));

                    // Split chunk at reference boundaries so each chunk is self-contained
                    if (client.mode == Mode::BUFFERING and current_chunk.sample_count > 0)
                    {
                        client.ring_sample_count += current_chunk.sample_count;
                        client.ring.push_back(std::move(current_chunk));
                        evict_ring(client);
                        current_chunk = Chunk{};
                        current_chunk.sample_count = 0;
                    }
                    current_chunk.entry_reference = client.current_reference;

                    nanoseconds absolute = client.current_reference - client.start_time;
                    process_sample(absolute, elem_start, pos);
                    continue;
                }

                if (raw & Command::UPDATE_PERIOD)
                {
                    if (pos + sizeof(uint64_t) > buf_end)
                    {
                        pos = elem_start;
                        break;
                    }
                    client.current_period = nanoseconds(extract_data<uint64_t>(pos));
                    route(elem_start, static_cast<std::size_t>(pos - elem_start));
                    continue;
                }

                if (raw & Command::UPDATE_PRIORITY)
                {
                    if (pos + sizeof(int32_t) > buf_end)
                    {
                        pos = elem_start;
                        break;
                    }
                    client.current_priority = extract_data<int32_t>(pos);
                    route(elem_start, static_cast<std::size_t>(pos - elem_start));
                    continue;
                }

                route(elem_start, static_cast<std::size_t>(pos - elem_start));
                continue;
            }

            nanoseconds absolute = nanoseconds(raw) + (client.current_reference - client.start_time);
            process_sample(absolute, elem_start, pos);
        }

        std::size_t consumed = static_cast<std::size_t>(pos - client.buffer.data());
        client.buffer.erase(client.buffer.begin(), client.buffer.begin() + static_cast<ptrdiff_t>(consumed));

        if (client.mode == Mode::BUFFERING and not current_chunk.data.empty())
        {
            client.ring_sample_count += current_chunk.sample_count;
            client.ring.push_back(std::move(current_chunk));
            evict_ring(client);
        }

        if (client.mode == Mode::RECORDING and client.sink != nullptr)
        {
            client.sink->sync();
        }

        return end_of_stream;
    }

    void Recorder::process()
    {
        for (auto& client : clients_)
        {
            uint8_t read_buf[2048];
            int64_t bytes_read = client.io->read(read_buf, sizeof(read_buf));

            if (bytes_read < 0)
            {
                if (errno != EAGAIN)
                {
                    printf("[Recorder] Read error on client (%s_%s)\n",
                           client.process_name.c_str(), client.source_name.c_str());
                    client.io.reset();
                    continue;
                }
            }
            else if (bytes_read == 0)
            {
                printf("[Recorder] Client disconnected (%s_%s)\n",
                       client.process_name.c_str(), client.source_name.c_str());

                if (client.mode == Mode::RECORDING)
                {
                    parse_blackbox_data(client);
                    stop_recording(client);
                }
                else if (client.mode == Mode::NORMAL)
                {
                    client.flush();
                    if (client.sink != nullptr)
                    {
                        uint32_t sentinel = ESCAPE | Command::DATA_STREAM_END;
                        client.sink->write(&sentinel, sizeof(sentinel));
                        client.sink->sync();
                    }
                }

                client.io.reset();
                continue;
            }
            else
            {
                client.buffer.insert(client.buffer.end(), read_buf, read_buf + bytes_read);
            }

            // --- Header parsing ---
            if (client.header_bytes.empty() and client.buffer.size() >= 16)
            {
                uint64_t data_offset;
                std::memcpy(&data_offset, client.buffer.data() + 8, sizeof(data_offset));

                std::size_t header_total = static_cast<std::size_t>(data_offset) + 8;
                if (client.buffer.size() < header_total)
                {
                    continue;
                }

                uint8_t const* hdr_pos = client.buffer.data() + 32;
                client.start_time = nanoseconds(extract_data<uint64_t>(hdr_pos));

                auto extract_string = [&hdr_pos]()
                {
                    uint16_t str_size = extract_data<uint16_t>(hdr_pos);
                    std::string str(str_size, '\0');
                    std::memcpy(str.data(), hdr_pos, str_size);
                    hdr_pos += str_size;
                    return str;
                };
                client.process_name = extract_string();
                client.source_name = extract_string();

                auto header_end = client.buffer.begin() + static_cast<ptrdiff_t>(header_total);
                client.header_bytes.assign(client.buffer.begin(), header_end);
                client.buffer.erase(client.buffer.begin(), header_end);

                printf("[Recorder] Header parsed: %s_%s\n", client.process_name.c_str(), client.source_name.c_str());
            }

            if (client.header_bytes.empty())
            {
                continue;
            }

            // --- Mode decision (PENDING -> NORMAL or BUFFERING) ---
            if (client.mode == Mode::PENDING)
            {
                uint8_t const* pos = client.buffer.data();
                uint8_t const* const buf_end = pos + client.buffer.size();
                bool decided = false;

                while (pos + sizeof(uint32_t) <= buf_end)
                {
                    uint32_t raw;
                    std::memcpy(&raw, pos, sizeof(raw));

                    if (not (raw & ESCAPE))
                    {
                        decided = true;
                        break;
                    }

                    if (raw & Command::UPDATE_REFERENCE)
                    {
                        // Only peek at the data, do not consume it so it can be written later
                        decided = true;
                        break;
                    }

                    if (raw & Command::SET_THRESHOLD)
                    {
                        if (pos + 4 + sizeof(uint64_t) > buf_end)
                        {
                            break;
                        }
                        pos += 4;
                        client.threshold = nanoseconds(extract_data<uint64_t>(pos));
                        continue;
                    }

                    if (raw & Command::UPDATE_PERIOD)
                    {
                        if (pos + 4 + sizeof(uint64_t) > buf_end)
                        {
                            break;
                        }
                        pos += 4;
                        client.current_period = nanoseconds(extract_data<uint64_t>(pos));
                        continue;
                    }

                    if (raw & Command::UPDATE_PRIORITY)
                    {
                        if (pos + 4 + sizeof(int32_t) > buf_end)
                        {
                            break;
                        }
                        pos += 4;
                        client.current_priority = extract_data<int32_t>(pos);
                        continue;
                    }

                    decided = true;
                    break;
                }

                std::size_t consumed = static_cast<std::size_t>(pos - client.buffer.data());
                client.buffer.erase(client.buffer.begin(), client.buffer.begin() + static_cast<ptrdiff_t>(consumed));

                if (not decided)
                {
                    continue;
                }

                if (client.threshold.count() > 0)
                {
                    client.mode = Mode::BUFFERING;
                    printf("[Recorder] Blackbox mode: %s_%s (threshold: %ld ns)\n",
                           client.process_name.c_str(), client.source_name.c_str(),
                           static_cast<long>(client.threshold.count()));
                }
                else
                {
                    client.mode = Mode::NORMAL;

                    std::string file_name = recording_path_ + '/';
                    file_name += format_iso_timestamp(client.start_time);
                    file_name += '_';
                    file_name += client.process_name;
                    file_name += '_';
                    file_name += client.source_name;
                    file_name += ".tick";

                    auto it = std::find_if(clients_.begin(), clients_.end(),
                        [&file_name](Client const& c)
                        {
                            return c.name == file_name;
                        });
                    if (it != clients_.end())
                    {
                        printf("[Recorder] !!! WARNING !!! Another client have the same name (%s)!"
                               " Switching the sink to null IO\n", file_name.c_str());
                        client.sink = std::make_unique<NullIO>();
                    }
                    else
                    {
                        client.name = file_name;
                        client.sink = std::make_unique<File>(client.name);
                    }

                    client.sink->open(access::Mode::WRITE_ONLY | access::Mode::TRUNCATE);

                    client.sink->write(client.header_bytes.data(), static_cast<int64_t>(client.header_bytes.size()));

                    write_command(*client.sink, Command::UPDATE_PERIOD, client.current_period);
                    write_command(*client.sink, Command::UPDATE_PRIORITY, client.current_priority);

                    client.flush();
                }
            }

            // --- Per-mode data handling ---
            if (client.mode == Mode::NORMAL and client.sink != nullptr)
            {
                if (client.buffer.size() > 2048)
                {
                    client.flush();
                }
            }
            else if (client.mode == Mode::BUFFERING or client.mode == Mode::RECORDING)
            {
                if (parse_blackbox_data(client))
                {
                    if (client.mode == Mode::RECORDING)
                    {
                        stop_recording(client);
                    }
                    client.io.reset();
                }
            }
        }

        clients_.erase(std::remove_if(clients_.begin(), clients_.end(),
            [](Client const& client) { return client.io == nullptr; }), clients_.end());
    }
}
