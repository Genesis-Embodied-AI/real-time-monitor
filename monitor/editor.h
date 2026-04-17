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
        // True when the editor's in-memory metadata differs from the copy on
        // disk (i.e. the user edited a field but has not pressed Save yet).
        bool dirty = false;
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
        void draw_error_modal();
        void draw_status_bar();
        void save_all();
        // Attempts to open `curve.path` read-write and persist metadata.
        // Returns true on success; on failure, appends the filename + reason
        // to save_errors_ so the user is notified via the error modal.
        bool save_curve(CurveInfo& curve);
        // Prepends `prefix` to every curve's display_name. If a curve has no
        // display_name override yet, the prefix is applied on top of its
        // original_name so the resulting label matches what was shown.
        // Idempotent: curves whose effective name already starts with the
        // prefix are skipped.
        void apply_prefix_to_all(std::string const& prefix);

        Plot& diff_plot_;
        Plot& up_plot_;

        std::vector<CurveInfo> curves_;
        int selected_idx_ = -1;
        // Set when a keyboard shortcut changes selected_idx_ so the list can
        // scroll the newly-selected row into view on the current frame.
        bool scroll_to_selected_ = false;

        // Curves that require sentinel repair before metadata can be written,
        // collected during save_all() and resolved by the repair modal.
        struct PendingRepair
        {
            int idx;
            int64_t eof_pos;
        };
        std::vector<PendingRepair> pending_repairs_;

        // Error messages accumulated during the most recent save pass, shown
        // to the user via draw_error_modal(). Cleared when the user dismisses.
        std::vector<std::string> save_errors_;

        // Input buffer for the "prefix all display names" bulk helper.
        char prefix_buf_[128] = {};
    };
}

#endif
