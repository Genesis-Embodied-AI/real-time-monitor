#ifndef RTM_MONITOR_MAIN_WINDOW_H
#define RTM_MONITOR_MAIN_WINDOW_H

#include <string>
#include <vector>
#include "editor.h"
#include "plot.h"

namespace rtm
{
    class MainWindow
    {
    public:
        void load_dataset(std::vector<std::filesystem::path> const& inputs);

        void draw();

    private:
        int load_file(std::string const& path);
        void draw_migration_modal();

        Plot diff_{"Times Diff", "jitter (ms)"};
        Plot up_  {"Times Up",   "up time (ms)"};
        Editor editor_{diff_, up_};
        std::vector<std::string> pending_migration_;
    };
}

#endif
