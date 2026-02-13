#include <argparse/argparse.hpp>
#include "rtm/probe.h"
#include "rtm/io/file.h"
#include "rtm/io/posix/local_socket.h"

using namespace std::chrono;

int main(int argc, char *argv[])
{
    uint64_t samples;
    std::string listening_path;
    std::string output_path;

    argparse::ArgumentParser parser("rtm_generator", "Generate dummy probe data.");
    parser.add_argument("-s", "--samples")
        .help("number of samples to generate")
        .required()
        .store_into(samples);

    parser.add_argument("-l", "--listen")
        .help("path of the Unix socket to connect to")
        .default_value(std::string{rtm::DEFAULT_LISTENING_PATH})
        .store_into(listening_path);

    parser.add_argument("-o", "--output")
        .help("output file path (used if --listen is not provided)")
        .default_value(std::string{"test.tick"})
        .store_into(output_path);

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

    constexpr nanoseconds START = 8'000'000s;

    printf("Generate %ld samples\n", samples);

    std::unique_ptr<rtm::AbstractIO> io;
    rtm::access::Mode mode;
    if (parser.is_used("--listen"))
    {
        io = std::make_unique<rtm::LocalSocket>(listening_path);
        mode = rtm::access::Mode::READ_WRITE;
    }
    else
    {
        io = std::make_unique<rtm::File>(output_path);
        mode = rtm::access::Mode::WRITE_ONLY | rtm::access::Mode::TRUNCATE;
    }

    auto rc = io->open(mode);
    if (rc)
    {
        printf("io open() error: %s\n", rc.message().c_str());
        return 1;
    }

    rtm::Probe probe;
    probe.init("generator", "one_task",
               START, 1ms, 42,
               std::move(io));

    uint64_t period = 1;
    uint64_t save_period = 0;
    for (uint64_t i = 1; i < samples; i += period)
    {
        if (i > (samples / 3))
        {
            period = 7;
        }
        if (i > (samples * 2 / 3))
        {
            period = 3;
        }

        // create a discontinuity of one record (like a real time loss)
        if (save_period != 0)
        {
            period = save_period;
            save_period = 0;
        }
        if (i == (samples / 5))
        {
            save_period = period;
            period = 30;
        }

        nanoseconds now = START + 20ms + i * 1ms + (100 - rand() % 200) * 1us;
        probe.log(now);
        probe.log(now + (rand() % 500 + period * 50) * 1us);
    }
    probe.flush();

    return 0;
}
