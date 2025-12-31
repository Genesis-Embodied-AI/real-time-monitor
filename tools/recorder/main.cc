
#include <csignal>
#include <algorithm>
#include <atomic>
#include <vector>
#include <unistd.h>

#include "rtm/os/time.h"
#include "rtm/serializer.h"
#include "rtm/io/file.h"
#include "rtm/io/posix/local_socket.h"

using namespace rtm;


std::atomic<bool> keep_running{true};

void signal_handler(int signal)
{
    if (signal == SIGINT)
    {
        keep_running = false;
    }
}

int main(int argc, char* argv[])
{
    std::signal(SIGINT, signal_handler);

    std::string listening_path = DEFAULT_LISTENING_PATH;
    if (argc == 2)
    {
        listening_path = argv[1];
    }

    LocalListener server(listening_path);
    auto rc = server.listen(1);
    if (rc)
    {
        printf("listen() error: %s\n", rc.message().c_str());
        return 1;
    }


    struct Client
    {
        Client() = default;
        Client(Client&&) = default;
        Client& operator=(Client&&) = default;

        ~Client()
        {
            flush();
        }

        void flush()
        {
            if (sink != nullptr)
            {
                sink->write(buffer.data(), static_cast<int64_t>(buffer.size()));
            }
            buffer.clear();
        }

        std::unique_ptr<AbstractIO> io{};
        std::unique_ptr<AbstractIO> sink{};
        std::vector<uint8_t> buffer{};
    };
    std::vector<Client> clients;

    while (keep_running)
    {
        auto io = server.accept(access::Mode::NON_BLOCKING);
        if (io != nullptr)
        {
            Client client;
            client.io = std::move(io);
            client.sink = nullptr;
            client.buffer.reserve(4096);
            clients.emplace_back(std::move(client));

            printf("[Recorder] New client\n");
        }

        for (auto& client : clients)
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
                ::sync();
            }
        }

        clients.erase(std::remove_if(clients.begin(), clients.end(),
            [](Client const& client) { return client.io == nullptr; }),
        clients.end());

        sleep(1ms);
    }

    return 0;
}
