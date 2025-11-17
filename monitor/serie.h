#ifndef RTM_MONITOR_SERIE_H
#define RTM_MONITOR_SERIE_H

#include <imgui.h>
#include <unordered_map>

#include "rtm/parser.h"

namespace rtm
{
    ImVec4 generate_random_color();
    std::string format_iso_timestamp(nanoseconds timestamp);

    class Serie
    {
    public:
        Serie(TickHeader const& header, std::vector<Parser::Point>&& raw_serie, ImVec4 color);
        ~Serie() = default;

        bool plot() const;

    private:
        static constexpr seconds_f SECTION_SIZE = 2min;
        struct Section
        {
            seconds_f min;
            seconds_f max;
            std::vector<Parser::Point> points;
        };
        void split_serie(std::vector<Section>& sections, std::vector<Parser::Point> const& flat);

        ImVec4 color_;

        std::string name_;

        std::vector<Section> sections_;
        std::vector<Parser::Point> serie_;
        bool is_downsampled_;
    };
}

#endif
