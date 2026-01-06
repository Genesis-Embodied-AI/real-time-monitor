#include <algorithm>

#include "recorder.h"
#include "serializer.h"
#include "io/file.h"
#include "os/time.h"

namespace rtm
{
    Recorder::Client::~Client()
    {
        flush();
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

    void Recorder::process()
    {
        for (auto& client : clients_)
        {
            uint8_t buffer[2048];
            int64_t read = client.io->read(buffer, sizeof(buffer));
            if (read == 0)
            {
                // client disconnection
                printf("[Recorder] Client disconnected\n");
                client.io.reset();
            }

            if (read > 0)
            {
                client.buffer.insert(client.buffer.end(), buffer, buffer + read);
            }

            if ((client.sink == nullptr) and (client.buffer.size() > 64))
            {
                // Enough data to determine sink path and start recording
                uint8_t const* pos = client.buffer.data() + 32;
                uint64_t start_time_raw = extract_data<uint64_t>(pos);
                nanoseconds start_time(start_time_raw);

                auto extract_string = [&pos]()
                {
                    uint16_t str_size = extract_data<uint16_t>(pos);

                    std::string str;
                    str.resize(str_size);
                    std::memcpy(str.data(), pos, str_size);

                    pos += str_size;
                    return str;
                };
                std::string process_name = extract_string();
                std::string source_name  = extract_string();

                std::string filename = format_iso_timestamp(start_time);
                filename += '_';
                filename += process_name;
                filename += '_';
                filename += source_name;
                filename += ".tick";
                client.sink = std::make_unique<File>(filename);
                client.sink->open(access::Mode::WRITE_ONLY | access::Mode::TRUNCATE);
                client.flush();
            }

            if (client.buffer.size() > 2048)
            {
                client.flush();
            }
        }

        clients_.erase(std::remove_if(clients_.begin(), clients_.end(),
            [](Client const& client) { return client.io == nullptr; }),
        clients_.end());
    }
}
