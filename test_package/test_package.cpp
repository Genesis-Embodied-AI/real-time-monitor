#include <rtm/probe.h>
#include <rtm/parser.h>
#include <rtm/io/null.h>

using namespace std::chrono;
using namespace rtm;

int main()
{
    auto io = std::make_unique<NullIO>();
    io->open(access::Mode::WRITE_ONLY);

    Probe probe;
    probe.init("test", "task", 0s, 1ms, 42, std::move(io));

    probe.log(1ms);
    probe.log(2ms);

    probe.flush();

    return 0;
}
