#include "probe.h"
#include "parser.h"

using namespace std::chrono;

int main(int argc, char* argv[])
{
    constexpr nanoseconds START = 8'000'000s;

    uint64_t samples = std::stoull(argv[1]);
    printf("Generate %ld samples\n", samples);

    auto io = std::make_unique<rtm::FileWrite>("test.tick");
    rtm::Probe probe;
    probe.init("generator", "one_task",
            START, 1ms, 42,
            std::move(io));

    uint64_t period = 1;
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

        nanoseconds now = START + 20ms + i * 1ms + (100 - rand() % 200) * 1us;
        probe.log(now);
        probe.log(now + (rand() % 500 + period * 50) * 1us);
    }
    probe.flush();

    return 0;
}
