#include "probe.h"
#include "parser.h"

using namespace std::chrono;

int main()
{
    constexpr nanoseconds START = 8000000s;

    {
        auto io = std::make_unique<rtm::FileWrite>("test.tick");
        rtm::Probe probe;
        probe.init("generator", "one_task",
                START, 1ms, 42,
                std::move(io));

        uint64_t period = 1;
        for (uint64_t i = 1; i < 10000; i += period)
        {
            if (i > 3000)
            {
                period = 7;
            }
            if (i > 6000)
            {
                period = 3;
            }
            probe.log(START + 20ms + i * 1ms);
            probe.log(START + 20ms + i * 1ms + (rand() % 500 + 1) * 1us + 100us);
        }
        probe.flush();
    }

    {
        auto io = std::make_unique<rtm::FileRead>("test.tick");
        rtm::Parser p{std::move(io)};
        p.load_header();
        p.load_samples();
    }

    return 0;
}
