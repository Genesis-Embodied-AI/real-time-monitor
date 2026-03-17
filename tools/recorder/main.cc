#include <csignal>
#include <atomic>
#include <argparse/argparse.hpp>

#include "rtm/recorder.h"
#include "rtm/os/time.h"
#include "rtm/io/posix/local_socket.h"
#include "rtm/io/posix/tcp_socket.h"

using namespace rtm;

std::atomic<bool> keep_running{true};

void signal_handler(int signal)
{
    if (signal == SIGINT)
    {
        keep_running = false;
    }
}

static std::pair<std::string, uint16_t> parse_host_port(std::string const& str)
{
    auto pos = str.rfind(':');
    if (pos == std::string::npos)
    {
        return {"", static_cast<uint16_t>(std::stoi(str))};
    }
    return {str.substr(0, pos), static_cast<uint16_t>(std::stoi(str.substr(pos + 1)))};
}

int main(int argc, char* argv[])
{
    std::signal(SIGINT, signal_handler);

    std::string recording_path;

    argparse::ArgumentParser parser("rtm_recorder", "Record real-time probe data to tick files.");
    parser.add_argument("output")
        .help("directory where recording files are written")
        .default_value(std::string{"."})
        .nargs(argparse::nargs_pattern::optional)
        .store_into(recording_path);
    parser.add_argument("-l", "--local")
        .help("listen on a local (Unix) socket at the given path (repeatable)")
        .default_value(std::vector<std::string>{})
        .append();
    parser.add_argument("-t", "--tcp")
        .help("listen on a TCP socket at [host:]port (repeatable)")
        .default_value(std::vector<std::string>{})
        .append();
    parser.add_argument("--pre-duration")
        .help("blackbox pre-event capture duration in seconds (default: 120)")
        .default_value(120u)
        .scan<'u', unsigned>();
    parser.add_argument("--post-duration")
        .help("blackbox post-event capture duration in seconds (default: 120)")
        .default_value(120u)
        .scan<'u', unsigned>();

    try
    {
        parser.parse_args(argc, argv);
    }
    catch (const std::exception& e)
    {
        printf("%s\n", e.what());
        printf("%s\n", parser.help().str().c_str());
        return 1;
    }

    auto local_args = parser.get<std::vector<std::string>>("--local");
    auto tcp_args   = parser.get<std::vector<std::string>>("--tcp");

    // Default to a local socket if nothing is specified
    if (local_args.empty() and tcp_args.empty())
    {
        local_args.push_back(DEFAULT_LISTENING_PATH);
    }

    auto pre_seconds  = parser.get<unsigned>("--pre-duration");
    auto post_seconds = parser.get<unsigned>("--post-duration");
    nanoseconds pre_duration  = std::chrono::seconds{pre_seconds};
    nanoseconds post_duration = std::chrono::seconds{post_seconds};

    printf("[Recorder] Starting\n");
    printf("[Recorder] Recording to %s\n", recording_path.c_str());
    printf("[Recorder] Blackbox window: %us pre / %us post\n", pre_seconds, post_seconds);

    Recorder recorder{recording_path, pre_duration, post_duration};

    // --- Set up local (Unix) listeners ---
    std::vector<std::unique_ptr<LocalListener>> local_listeners;
    for (auto const& path : local_args)
    {
        auto listener = std::make_unique<LocalListener>(path);
        auto rc = listener->listen(1);
        if (rc)
        {
            printf("[Recorder] listen() error on local '%s': %s\n", path.c_str(), rc.message().c_str());
            return 1;
        }
        printf("[Recorder] Listening on local socket %s\n", path.c_str());
        local_listeners.push_back(std::move(listener));
    }

    // --- Set up TCP listeners ---
    std::vector<std::unique_ptr<TcpListener>> tcp_listeners;
    for (auto const& arg : tcp_args)
    {
        auto [host, port] = parse_host_port(arg);
        auto listener = std::make_unique<TcpListener>(host, port);
        auto rc = listener->listen(4);
        if (rc)
        {
            printf("[Recorder] listen() error on TCP '%s': %s\n", arg.c_str(), rc.message().c_str());
            return 1;
        }
        char const* tcp_display = "*";
        if (not host.empty())
        {
            tcp_display = host.c_str();
        }
        printf("[Recorder] Listening on TCP %s:%u\n", tcp_display, port);
        tcp_listeners.push_back(std::move(listener));
    }

    while (keep_running)
    {
        for (auto& listener : local_listeners)
        {
            auto io = listener->accept(access::Mode::NON_BLOCKING);
            if (io != nullptr)
            {
                recorder.add_client(std::move(io));
            }
        }

        for (auto& listener : tcp_listeners)
        {
            auto io = listener->accept(access::Mode::NON_BLOCKING);
            if (io != nullptr)
            {
                recorder.add_client(std::move(io));
            }
        }

        recorder.process();
        sleep(1ms);
    }

    return 0;
}
