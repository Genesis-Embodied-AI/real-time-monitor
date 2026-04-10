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

        float available = ImGui::GetContentRegionAvail().x;
        float list_width = available * 0.35f;

        ImGui::BeginChild("CurveList", ImVec2(list_width, 0), true);
        draw_curve_list();
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("DetailPanel", ImVec2(0, 0), true);
        draw_detail_panel();
        ImGui::EndChild();

        ImGui::EndTabItem();
    }

    void Editor::draw_curve_list()
    {
        for (std::size_t i = 0; i < curves_.size(); ++i)
        {
            auto& curve = curves_[i];
            bool is_selected = (selected_idx_ == static_cast<int>(i));

            std::string label = std::filesystem::path(curve.path).filename().string();
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
        }

        ImGui::Checkbox("Visible by default", &curve.default_visible);

        int32_t weight = curve.diff->display_weight();
        ImGui::SetNextItemWidth(100.0f);
        if (ImGui::InputInt("Display weight", &weight, 1, 10))
        {
            curve.diff->set_display_weight(weight);
            curve.up->set_display_weight(weight);
            diff_plot_.sort_series();
            up_plot_.sort_series();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Save", ImVec2(120, 0)))
        {
            save_selected();
        }
    }

    void Editor::save_selected()
    {
        auto& curve = curves_[static_cast<std::size_t>(selected_idx_)];
        int64_t eof_pos = curve.header.needs_sentinel_repair();
        if (eof_pos)
        {
            pending_repair_idx_ = selected_idx_;
            pending_repair_eof_ = eof_pos;
        }
        else
        {
            File file(curve.path);
            file.open(access::Mode::READ_WRITE);
            save_metadata(file, curve.header, build_metadata(curve));
        }
    }

    void Editor::draw_repair_modal()
    {
        if (pending_repair_idx_ >= 0)
        {
            ImGui::OpenPopup("File Repair Required");
        }

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(520, 0), ImGuiCond_Appearing);

        if (ImGui::BeginPopupModal("File Repair Required", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            auto idx = static_cast<std::size_t>(pending_repair_idx_);
            auto& curve = curves_[idx];

            ImGui::TextWrapped(
                "This file is missing the end-of-stream sentinel (possibly due "
                "to a power failure or crash). A sentinel must be appended "
                "before metadata can be saved. This modifies the file.");

            ImGui::Spacing();
            ImGui::TextUnformatted(std::filesystem::path(curve.path).filename().c_str());
            ImGui::Spacing();

            if (ImGui::Button("Repair & Save", ImVec2(140, 0)))
            {
                File file(curve.path);
                file.open(access::Mode::READ_WRITE);
                repair_sentinel(file, pending_repair_eof_);
                curve.header.sentinel_pos = pending_repair_eof_;

                save_metadata(file, curve.header, build_metadata(curve));

                pending_repair_idx_ = -1;
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel", ImVec2(120, 0)))
            {
                pending_repair_idx_ = -1;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }
}
