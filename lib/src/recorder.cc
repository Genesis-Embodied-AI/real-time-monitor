#include <algorithm>
#include <filesystem>

#include "recorder.h"
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

    Recorder::Recorder(std::string_view recording_path)
        : recording_path_{recording_path}
    {
        std::filesystem::create_directory(recording_path_);
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

                std::string name = recording_path_ + '/';;
                name += format_iso_timestamp(start_time);
                name += '_';
                name += process_name;
                name += '_';
                name += source_name;
                name += ".tick";

                auto it = std::find_if(clients_.begin(), clients_.end(),
                    [&name](Client const& c)
                    {
                        return c.name == name;
                    });
                if (it != clients_.end())
                {
                    printf("[Recorder] !!! WARNING !!! Another client have the same name (%s)! Switching the sink to null IO\n", client.name.c_str());
                    client.sink = std::make_unique<NullIO>();
                }
                else
                {
                    client.name = std::move(name);
                    client.sink = std::make_unique<File>(client.name);
                }

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
