#ifndef RTM_MONITOR_MAIN_WINDOW_H
#define RTM_MONITOR_MAIN_WINDOW_H

#include <vector>
#include "plot.h"

namespace rtm
{
    class MainWindow
    {
    public:
        void load_dataset(std::vector<std::filesystem::path> const& inputs);

        void draw();

    private:
        Plot diff_{"Times Diff", "jitter (ms)"};
        Plot up_  {"Times Up",   "up time (ms)"};
    };
}

#endif
