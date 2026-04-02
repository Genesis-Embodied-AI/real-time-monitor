#ifndef RTM_MONITOR_PLOT_H
#define RTM_MONITOR_PLOT_H

#include <filesystem>
#include <functional>
#include <future>
#include <implot.h>

#include "serie.h"

namespace rtm
{
    class Plot
    {
    public:
        Plot(std::string const& name, std::string const& legend);
        void add_serie(Serie&& serie, milliseconds_f min_y, milliseconds_f max_y, nanoseconds begin, nanoseconds end);
        void draw();

    private:
        void draw_stats_panel();
        void draw_status_bar();

        void compute_stats_on_view_update();

        std::string name_;
        std::string legend_;

        struct SerieBounds
        {
            seconds_f begin;
            seconds_f end;
            milliseconds_f min_y;
            milliseconds_f max_y;
            bool visible{true};
        };

        std::vector<Serie> series_;
        std::vector<SerieBounds> serie_bounds_;
        std::vector<Statistics> stats_;
        std::future<std::vector<Statistics>> stats_future_;
        ImPlotRect old_limits_;
        milliseconds_f min_y_{nanoseconds::max()};
        milliseconds_f max_y_{-1ns};
        bool is_downsampled_{false};

        nanoseconds begin_{-1ns};
        nanoseconds end_{-1ns};           // end (X) of the plots

        bool show_stats_{false};
        float stats_bar_width{250.0f};
        bool was_dragging_stats_bar{false};
    };
}

#endif
