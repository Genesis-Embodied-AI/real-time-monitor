#include <csignal>
#include <atomic>
#include <argparse/argparse.hpp>

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

    std::string recording_path;
    std::string listening_path;

    argparse::ArgumentParser parser("rtm_recorder", "Record real-time probe data to tick files.");
    parser.add_argument("output")
        .help("directory where recording files are written")
        .default_value(std::string{"."})
        .nargs(argparse::nargs_pattern::optional)
        .store_into(recording_path);
    parser.add_argument("-l", "--listen")
        .help("path of the Unix socket to listen for probe connections")
        .default_value(std::string{DEFAULT_LISTENING_PATH})
        .store_into(listening_path);

    try
    {
        parser.parse_args(argc, argv);
    }
    catch (const std::runtime_error& e)
    {
        printf("%s\n", e.what());
        printf("%s\n", parser.help().str().c_str());
        return 1;
    }

    printf("[Recorder] Starting\n");
    printf("[Recorder] Recording to %s\n", recording_path.c_str());
    printf("[Recorder] Listening to %s\n", listening_path.c_str());

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
