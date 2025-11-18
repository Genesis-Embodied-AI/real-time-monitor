#include <nanobind/nanobind.h>
#include <nanobind/trampoline.h>

#include "rtm/io.h"
#include "rtm/parser.h"
#include "rtm/probe.h"
#include "rtm/time_wrapper.h"

namespace nb = nanobind;
using namespace nb::literals;

namespace rtm
{
    void create_python_bindings(nb::module_ &m)
    {
        nb::class_<Probe>(m, "Probe")
            .def(nb::init<>())
            .def("init", [](Probe& self, char const* output_file, char const* process, char const* task,
                            uint32_t period_ms, uint32_t priority, int64_t start_time_ns)
                {

                    nanoseconds start;
                    if (start_time_ns >= 0)
                    {
                        start = nanoseconds{start_time_ns};
                    }
                    else
                    {
                        start = start_time();
                    }

                    auto io = std::make_unique<rtm::FileWrite>(output_file);
                    self.init(process, task,
                        start, milliseconds{period_ms}, priority,
                        std::move(io));
                }, "output_file"_a,
                   "process"_a, "task"_a,
                   "period_ms"_a, "priority"_a,
                   "start_time_ns"_a = -1)
            .def("log", [](Probe& self)
                {
                    self.log();
                });
    }
}
