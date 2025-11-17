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
        void draw_graphs(char const* tab_name, bool request_fit,
                         std::function<bool(const Serie&)> plot_func);
        void draw_status_bar(bool is_downsampled);

        std::vector<Serie> series_;
    };
}

#endif
