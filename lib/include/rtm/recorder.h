#ifndef RTM_LIB_RECORDER_H
#define RTM_LIB_RECORDER_H

#include <memory>
#include <vector>

#include "rtm/io/io.h"

namespace rtm
{
    class Recorder
    {
    public:
        // output folder is created if it does not exist
        Recorder(std::string_view recording_path);
        ~Recorder() = default;

        Recorder(Recorder&& other) = default;
        Recorder& operator=(Recorder&& other) = default;

        void add_client(std::unique_ptr<AbstractIO>&& io);
        void process();

    private:
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
        };
        std::vector<Client> clients_{};

        std::string recording_path_;
    };
}

#endif
