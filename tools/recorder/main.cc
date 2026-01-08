
#include <csignal>
#include <atomic>

#include "rtm/recorder.h"
#include "rtm/os/time.h"
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

    std::string recording_path = ".";
    std::string listening_path = DEFAULT_LISTENING_PATH;
    if (argc >= 2)
    {
        recording_path = argv[1];
    }
    if (argc >= 3)
    {
        listening_path = argv[2];
    }

    Recorder recorder{recording_path};

    LocalListener server(listening_path);
    auto rc = server.listen(1);
    if (rc)
    {
        printf("listen() error: %s\n", rc.message().c_str());
        return 1;
    }

    while (keep_running)
    {
        auto io = server.accept(access::Mode::NON_BLOCKING);
        if (io != nullptr)
        {
            recorder.add_client(std::move(io));
        }

        recorder.process();
        sleep(1ms);
    }

    return 0;
}
