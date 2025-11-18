#include <rtm/probe.h>
#include <rtm/parser.h>

using namespace std::chrono;

int main()
{
    auto io = std::make_unique<rtm::NullWrite>();
    rtm::Probe probe;
    probe.init("test", "task", 0s, 1ms, 42, std::move(io));

    probe.log(1ms);
    probe.log(2ms);

    probe.flush();

    return 0;
}
