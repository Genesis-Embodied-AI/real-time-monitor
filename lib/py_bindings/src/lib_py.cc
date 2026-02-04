#include <nanobind/nanobind.h>
#include <nanobind/stl/bind_vector.h>
#include <nanobind/stl/chrono.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>
#include <nanobind/trampoline.h>
#include <nanobind/stl/unique_ptr.h>

#include "rtm/parser.h"
#include "rtm/probe.h"
#include "rtm/recorder.h"
#include "rtm/io/file.h"
#include "rtm/io/posix/local_socket.h"
#include "rtm/os/time.h"

namespace nb = nanobind;
using namespace nb::literals;

namespace rtm
{
    std::pair<std::vector<double>, std::vector<double>> split_point_vector(std::vector<Point> const& data)
    {
        std::vector<double> x, y;
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
            .def("init", [](Probe& self, char const* process, char const* task,
                            uint32_t period_ms, int32_t priority, nanoseconds start)
                {
                    auto io = std::make_unique<rtm::LocalSocket>();
                    auto rc = io->open(rtm::access::Mode::READ_WRITE);
                    if (rc)
                    {
                        throw std::runtime_error("Cannot connect ot the recorder");
                    }

                    self.init(process, task,
                        start, milliseconds{period_ms}, priority,
                        std::move(io));
                }, "process"_a, "task"_a,
                   "period_ms"_a, "priority"_a,
                   "start"_a = start_time())
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
                    auto io = std::make_unique<File>(file_path);
                    auto rc = io->open(access::Mode::READ_ONLY);
                    if (rc)
                    {
                        throw std::runtime_error("Cannot open file");
                    }

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
            .def_prop_ro("start_time", [](Parser &p) {return p.header().start_time;})
            .def("samples", &Parser::samples)
            .def("generate_times_diff", [](Parser &p) {
                return split_point_vector(p.generate_times_diff());
                })
            .def("generate_times_up", [](Parser &p) {
                return split_point_vector(p.generate_times_up());
                });

        nb::class_<LocalListener>(m, "LocalListener")
            .def(nb::init<std::string_view>(), nb::arg("listening_path") = DEFAULT_LISTENING_PATH)
            .def("listen", [](LocalListener& self, int backlog)
            {
                auto rc = self.listen(backlog);
                if (rc)
                {
                    throw std::runtime_error(rc.message().c_str());
                }
            }, nb::arg("backlog") = 1);

        nb::class_<Recorder>(m, "Recorder")
            .def(nb::init<std::string_view>(), nb::arg("recording_path"))
            .def("accept", [](Recorder& self, LocalListener& server)
            {
                auto io = server.accept(access::Mode::NON_BLOCKING);
                if (io != nullptr)
                {
                    self.add_client(std::move(io));
                }
            })
            .def("process", &Recorder::process);
    }
}
