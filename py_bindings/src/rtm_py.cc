#include <nanobind/nanobind.h>

namespace nb = nanobind;

namespace rtm
{
    void create_python_bindings(nb::module_ &m);

    NB_MODULE(real_time_monitor, m)
    {
        m.doc() = "Real time monitor bindings";

        create_python_bindings(m);
    }
}
