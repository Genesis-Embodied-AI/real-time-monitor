#include <nanobind/nanobind.h>
#include <nanobind/stl/bind_vector.h>
#include <nanobind/stl/chrono.h>
#include <nanobind/stl/string.h>
#include <nanobind/trampoline.h>
#include <nanobind/stl/unique_ptr.h>

#include "rtm/io.h"
#include "rtm/parser.h"
#include "rtm/probe.h"
#include "rtm/time_wrapper.h"

namespace nb = nanobind;
using namespace nb::literals;

namespace rtm
{
    std::pair<std::vector<float>, std::vector<float>> split_point_vector(std::vector<Point> const& data)
    {
        std::vector<float> x, y;
        x.reserve(data.size());
        y.reserve(data.size());
        for (auto d : data)
        {
            x.push_back(d.x);
            y.push_back(d.y);
        }
        return std::make_pair(x, y);
    }

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
                })
            .def("log_start", [](Probe& self)
                {
                    self.log();
                })
            .def("log_end", [](Probe& self)
                {
                    self.log();
                });

        nb::bind_vector<std::vector<float>>(m, "FVector");
        nb::bind_vector<std::vector<nanoseconds>>(m, "nsVector");
        nb::class_<Parser>(m, "Parser")
            .def(nb::new_([](std::string const& file_path)
                {
                    auto io = std::make_unique<FileRead>(file_path.c_str());
                    auto p = std::make_unique<Parser>(std::move(io));
                    p->load_header();
                    if (not p->load_samples())
                    {
                        throw std::runtime_error("Failed");
                    }
                    return p;
                }))
            .def_prop_ro("name", [](Parser &p) {return p.header().name;})
            .def_prop_ro("process", [](Parser &p) {return p.header().process;})
            .def_prop_ro("start_time", [](Parser &p) {return std::chrono::time_point<std::chrono::system_clock>(p.header().start_time);})
            .def("samples", &Parser::samples)
            .def("generate_times_diff", [](Parser &p) {
                return split_point_vector(p.generate_times_diff());
                })
            .def("generate_times_up", [](Parser &p) {
                return split_point_vector(p.generate_times_up());
                })
                ;

    }
}
