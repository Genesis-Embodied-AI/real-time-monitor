#ifndef RTM_MONITOR_EDITOR_H
#define RTM_MONITOR_EDITOR_H

#include <memory>
#include <string>
#include <vector>

#include "rtm/parser.h"
#include "plot.h"

namespace rtm
{
    struct CurveInfo
    {
        std::string path;
        TickHeader header;
        std::shared_ptr<Serie> diff;
        std::shared_ptr<Serie> up;
        bool default_visible = true;
    };

    class Editor
    {
    public:
        Editor(Plot& diff, Plot& up);

        void add_curve(std::string const& path, TickHeader const& header,
                       std::shared_ptr<Serie> diff, std::shared_ptr<Serie> up,
                       bool default_visible = true);
        void draw();

    private:
        void draw_curve_list();
        void draw_detail_panel();
        void draw_repair_modal();
        void save_selected();

        Plot& diff_plot_;
        Plot& up_plot_;

        std::vector<CurveInfo> curves_;
        int selected_idx_ = -1;
        int pending_repair_idx_ = -1;
        int64_t pending_repair_eof_ = 0;
    };
}

#endif
