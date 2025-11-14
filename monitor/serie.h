#ifndef RTM_MONITOR_SERIE_H
#define RTM_MONITOR_SERIE_H

#include <imgui.h>

#include "parser.h"

namespace rtm
{
    ImVec4 generate_random_color();
    std::string format_iso_timestamp(nanoseconds timestamp);

    class Serie
    {
    public:
        Serie(Parser const& parser);
        ~Serie() = default;

        bool plot_diffs() const;
        bool plot_ups() const;

    private:
        ImVec4 color_;

        std::string name_;
        std::vector<Parser::Point> diffs_full_;
        std::vector<Parser::Point> ups_full_;

        std::vector<Parser::Point> diffs_downsampled_;
        std::vector<Parser::Point> ups_downsampled_;
    };
}

#endif
