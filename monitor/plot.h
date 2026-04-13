#ifndef RTM_MONITOR_PLOT_H
#define RTM_MONITOR_PLOT_H

#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <implot.h>

#include "serie.h"

namespace rtm
{
    class Plot
    {
    public:
        Plot(std::string const& name, std::string const& legend);
        void add_serie(std::shared_ptr<Serie> serie, milliseconds_f min_y, milliseconds_f max_y, nanoseconds begin, nanoseconds end, bool visible = true);
        void sort_series();

        void draw();

    private:
        void draw_stats_panel();
        void draw_status_bar();

        void compute_stats_on_view_update();

        // Aggregate x/y bounds over currently-visible series, padded on Y by
        // Y_AXIS_MARGIN. Returns false if no series is visible (limits untouched).
        bool compute_visible_limits(double& x_min, double& x_max,
                                    double& y_min, double& y_max) const;

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

        std::vector<std::shared_ptr<Serie>> series_;
        std::vector<SerieBounds> serie_bounds_;
        std::vector<Statistics> stats_;
        std::future<std::vector<Statistics>> stats_future_;
        ImPlotRect old_limits_;
        bool is_downsampled_{false};

        bool show_stats_{false};
        float stats_bar_width{250.0f};
        bool was_dragging_stats_bar{false};
    };
}

#endif
