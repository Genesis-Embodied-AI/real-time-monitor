#include <cstring>
#include <filesystem>

#include "rtm/metadata.h"
#include "rtm/io/file.h"

#include "editor.h"

namespace rtm
{
    namespace
    {
        TickMetadata build_metadata(CurveInfo const& curve)
        {
            TickMetadata meta;
            meta.display_name = curve.diff->display_name();
            meta.default_visibility = curve.default_visible;
            meta.display_weight = curve.diff->display_weight();
            return meta;
        }
    }

    Editor::Editor(Plot& diff, Plot& up)
        : diff_plot_(diff)
        , up_plot_(up)
    {
    }

    void Editor::add_curve(std::string const& path, TickHeader const& header,
                           std::shared_ptr<Serie> diff, std::shared_ptr<Serie> up,
                           bool default_visible)
    {
        curves_.push_back({path, header, std::move(diff), std::move(up), default_visible});
    }

    void Editor::draw()
    {
        if (not ImGui::BeginTabItem("Editor"))
        {
            return;
        }

        draw_repair_modal();
        draw_error_modal();

        bool save_pressed = ImGui::Button("Save", ImVec2(120, 0));
        bool ctrl_s = ImGui::GetIO().KeyCtrl
                      and ImGui::IsKeyPressed(ImGuiKey_S, false);
        if (save_pressed or ctrl_s)
        {
            save_all();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%zu curve%s (Ctrl+S)",
                            curves_.size(), curves_.size() == 1 ? "" : "s");

        ImGui::Separator();

        // Reserve space at the bottom for the status bar (separator + 1 line).
        float status_bar_height = ImGui::GetFrameHeightWithSpacing()
                                  + ImGui::GetStyle().ItemSpacing.y;
        float available = ImGui::GetContentRegionAvail().x;
        float list_width = available * 0.35f;

        ImGui::BeginChild("CurveList", ImVec2(list_width, -status_bar_height), true);
        draw_curve_list();
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("DetailPanel", ImVec2(0, -status_bar_height), true);
        draw_detail_panel();
        ImGui::EndChild();

        draw_status_bar();

        ImGui::EndTabItem();
    }

    void Editor::draw_curve_list()
    {
        for (std::size_t i = 0; i < curves_.size(); ++i)
        {
            auto& curve = curves_[i];
            bool is_selected = (selected_idx_ == static_cast<int>(i));

            std::string filename = std::filesystem::path(curve.path).filename().string();
            // Prefix dirty entries with "*" so the user can spot unsaved files
            // in the list without selecting them one by one.
            std::string label = (curve.dirty ? "* " : "  ") + filename;
            if (ImGui::Selectable(label.c_str(), is_selected))
            {
                selected_idx_ = static_cast<int>(i);
            }
        }
    }

    void Editor::draw_detail_panel()
    {
        if (selected_idx_ < 0 or selected_idx_ >= static_cast<int>(curves_.size()))
        {
            ImGui::TextDisabled("Select a curve to edit its metadata.");
            return;
        }

        auto& curve = curves_[static_cast<std::size_t>(selected_idx_)];

        ImGui::Text("File: %s", std::filesystem::path(curve.path).filename().c_str());
        ImGui::Text("Original name: %s", curve.diff->original_name().c_str());
        ImGui::Separator();
        ImGui::Spacing();

        char buf[256];
        std::strncpy(buf, curve.diff->display_name().c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
        if (ImGui::InputText("Display Name", buf, sizeof(buf)))
        {
            curve.diff->set_display_name(buf);
            curve.up->set_display_name(buf);
            curve.dirty = true;
        }

        if (ImGui::Checkbox("Visible by default", &curve.default_visible))
        {
            curve.dirty = true;
        }

        int32_t weight = curve.diff->display_weight();
        ImGui::SetNextItemWidth(100.0f);
        if (ImGui::InputInt("Display weight", &weight, 1, 10))
        {
            curve.diff->set_display_weight(weight);
            curve.up->set_display_weight(weight);
            diff_plot_.sort_series();
            up_plot_.sort_series();
            curve.dirty = true;
        }

    }

    bool Editor::save_curve(CurveInfo& curve)
    {
        File file(curve.path);
        std::error_code ec = file.open(access::Mode::READ_WRITE);
        if (ec)
        {
            save_errors_.push_back(
                std::filesystem::path(curve.path).filename().string()
                + ": " + ec.message());
            return false;
        }
        save_metadata(file, curve.header, build_metadata(curve));
        curve.dirty = false;
        return true;
    }

    void Editor::save_all()
    {
        pending_repairs_.clear();
        save_errors_.clear();
        for (std::size_t i = 0; i < curves_.size(); ++i)
        {
            auto& curve = curves_[i];
            int64_t eof_pos = curve.header.needs_sentinel_repair();
            if (eof_pos)
            {
                pending_repairs_.push_back({static_cast<int>(i), eof_pos});
            }
            else
            {
                save_curve(curve);
            }
        }
    }

    void Editor::draw_repair_modal()
    {
        if (not pending_repairs_.empty())
        {
            ImGui::OpenPopup("File Repair Required");
        }

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(520, 0), ImGuiCond_Appearing);

        if (ImGui::BeginPopupModal("File Repair Required", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextWrapped(
                "The following file(s) are missing the end-of-stream sentinel "
                "(possibly due to a power failure or crash). A sentinel must be "
                "appended before metadata can be saved. This modifies the files.");

            ImGui::Spacing();
            for (auto const& pending : pending_repairs_)
            {
                auto const& curve = curves_[static_cast<std::size_t>(pending.idx)];
                ImGui::BulletText("%s",
                                  std::filesystem::path(curve.path).filename().c_str());
            }
            ImGui::Spacing();

            if (ImGui::Button("Repair & Save", ImVec2(140, 0)))
            {
                for (auto const& pending : pending_repairs_)
                {
                    auto& curve = curves_[static_cast<std::size_t>(pending.idx)];
                    File file(curve.path);
                    std::error_code ec = file.open(access::Mode::READ_WRITE);
                    if (ec)
                    {
                        save_errors_.push_back(
                            std::filesystem::path(curve.path).filename().string()
                            + ": " + ec.message());
                        continue;
                    }
                    repair_sentinel(file, pending.eof_pos);
                    curve.header.sentinel_pos = pending.eof_pos;
                    save_metadata(file, curve.header, build_metadata(curve));
                    curve.dirty = false;
                }
                pending_repairs_.clear();
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel", ImVec2(120, 0)))
            {
                pending_repairs_.clear();
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    void Editor::draw_status_bar()
    {
        ImGui::Separator();

        std::size_t dirty_count = 0;
        for (auto const& curve : curves_)
        {
            if (curve.dirty)
            {
                ++dirty_count;
            }
        }

        ImGui::AlignTextToFramePadding();
        ImVec4 green  = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
        ImVec4 orange = ImVec4(1.0f, 0.6f, 0.0f, 1.0f);

        if (curves_.empty())
        {
            ImGui::TextDisabled("No curves loaded");
            return;
        }

        if (dirty_count == 0)
        {
            ImGui::ColorButton("##saved_col", green,
                               ImGuiColorEditFlags_NoTooltip |
                               ImGuiColorEditFlags_NoBorder);
            ImGui::SameLine();
            ImGui::TextColored(green, "All changes saved");
        }
        else
        {
            ImGui::ColorButton("##dirty_col", orange,
                               ImGuiColorEditFlags_NoTooltip |
                               ImGuiColorEditFlags_NoBorder);
            ImGui::SameLine();
            ImGui::TextColored(orange,
                               "%zu curve%s with unsaved changes",
                               dirty_count, dirty_count == 1 ? "" : "s");
        }
    }

    void Editor::draw_error_modal()
    {
        if (not save_errors_.empty())
        {
            ImGui::OpenPopup("Save Failed");
        }

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(520, 0), ImGuiCond_Appearing);

        if (ImGui::BeginPopupModal("Save Failed", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextWrapped(
                "The following file(s) could not be saved. Check permissions "
                "and that the files still exist on disk.");

            ImGui::Spacing();
            for (auto const& msg : save_errors_)
            {
                ImGui::BulletText("%s", msg.c_str());
            }
            ImGui::Spacing();

            if (ImGui::Button("OK", ImVec2(120, 0)))
            {
                save_errors_.clear();
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }
}
