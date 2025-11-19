#ifndef RTM_MONITOR_PLOT_H
#define RTM_MONITOR_PLOT_H

#include <filesystem>
#include <functional>

#include "serie.h"

namespace rtm
{
    class Plot
    {
    public:
        void load_dataset(std::filesystem::path const& folder);

        void draw();

    private:
        void draw_graphs(char const* tab_name, char const* legend, float max_y, bool request_fit, std::vector<Serie> const& series);
        void draw_status_bar(bool is_downsampled);

        std::vector<Serie> diffs_;
        std::vector<Serie> ups_;

        nanoseconds end_{-1ns};           // end (X) of the plots
        milliseconds_f diff_max_{-1ns};   // max (Y) of the diff (jitter) plot
        milliseconds_f up_max_{-1ns};     // max (Y) of the up (time up) plot
    };
}

#endif
