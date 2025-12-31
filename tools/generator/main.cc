#include "rtm/probe.h"
#include "rtm/io/file.h"

using namespace std::chrono;

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        printf("Usage: /generator [samples]\n");
        return 1;
    }

    constexpr nanoseconds START = 8'000'000s;

    uint64_t samples = std::stoull(argv[1]);
    printf("Generate %ld samples\n", samples);

    auto io = std::make_unique<rtm::File>("test.tick");
    io->open(rtm::access::Mode::WRITE_ONLY);
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
