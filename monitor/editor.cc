#include <algorithm>
#include <filesystem>
#include <string>

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

        // Callback used with ImGui::InputText to resize the backing
        // std::string whenever the user types past its current capacity.
        // Mirrors what imgui's misc/cpp/imgui_stdlib.h does, inlined here to
        // avoid pulling extra sources into the build.
        int input_text_resize(ImGuiInputTextCallbackData* data)
        {
            if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
            {
                auto* str = static_cast<std::string*>(data->UserData);
                str->resize(static_cast<std::size_t>(data->BufTextLen));
                data->Buf = str->data();
            }
            return 0;
        }

        // InputText backed by a std::string — no fixed-size buffer, no silent
        // truncation. Returns true when the user edited the field this frame.
        bool input_text_string(char const* label, std::string& value,
                               ImGuiInputTextFlags flags = 0)
        {
            flags |= ImGuiInputTextFlags_CallbackResize;
            return ImGui::InputText(label, value.data(), value.capacity() + 1,
                                    flags, input_text_resize, &value);
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

        // Keyboard navigation of the curve list that works even while a text
        // field (Display Name, bulk prefix) has focus. We route through
        // ImGui::Shortcut() rather than raw IsKeyPressed so the key is
        // "claimed" — otherwise ImGui's built-in nav system also consumes
        // the arrow event in parallel and moves its nav focus ring.
        if (not curves_.empty())
        {
            int new_idx = selected_idx_;
            int last_idx = static_cast<int>(curves_.size()) - 1;

            if (ImGui::Shortcut(ImGuiMod_Alt | ImGuiKey_DownArrow,
                                ImGuiInputFlags_Repeat))
            {
                if (selected_idx_ < 0)
                {
                    // No selection yet — Alt+Down enters the list at the top.
                    new_idx = 0;
                }
                else
                {
                    new_idx = std::min(selected_idx_ + 1, last_idx);
                }
            }
            else if (ImGui::Shortcut(ImGuiMod_Alt | ImGuiKey_UpArrow,
                                     ImGuiInputFlags_Repeat))
            {
                if (selected_idx_ < 0)
                {
                    // No selection yet — Alt+Up enters the list at the
                    // bottom, symmetric with Alt+Down entering at the top.
                    new_idx = last_idx;
                }
                else if (selected_idx_ > 0)
                {
                    new_idx = selected_idx_ - 1;
                }
                // else (selected_idx_ == 0) stay at 0
            }
            if (new_idx != selected_idx_)
            {
                selected_idx_ = new_idx;
                scroll_to_selected_ = true;
            }
        }

        bool save_pressed = ImGui::Button("Save", ImVec2(120, 0));
        bool ctrl_s = ImGui::GetIO().KeyCtrl
                      and ImGui::IsKeyPressed(ImGuiKey_S, false);
        if (save_pressed or ctrl_s)
        {
            save_all();
        }
        ImGui::SameLine();
        char const* plural = "s";
        if (curves_.size() == 1)
        {
            plural = "";
        }
        ImGui::TextDisabled("%zu curve%s  |  Ctrl+S save  |  Alt+Up/Down navigate",
                            curves_.size(), plural);

        // Bulk helper: prepend a prefix to every curve's display name in one
        // click. Useful for tagging a whole run (e.g. "run42_") at import time.
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Bulk:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(180.0f);
        ImGui::InputTextWithHint("##bulk_prefix", "prefix",
                                 prefix_buf_, sizeof(prefix_buf_));
        ImGui::SameLine();
        bool can_apply = not curves_.empty() and prefix_buf_[0] != '\0';
        ImGui::BeginDisabled(not can_apply);
        if (ImGui::Button("Prefix all display names"))
        {
            apply_prefix_to_all(prefix_buf_);
        }
        ImGui::EndDisabled();

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
            char const* dirty_marker = "  ";
            if (curve.dirty)
            {
                dirty_marker = "* ";
            }
            std::string label = dirty_marker + filename;
            if (ImGui::Selectable(label.c_str(), is_selected))
            {
                selected_idx_ = static_cast<int>(i);
            }
            // When the selection was moved by keyboard shortcut, keep the
            // highlighted row visible by scrolling the child window.
            if (is_selected and scroll_to_selected_)
            {
                ImGui::SetScrollHereY(0.5f);
            }
        }
        scroll_to_selected_ = false;
    }

    void Editor::draw_detail_panel()
    {
        if (selected_idx_ < 0 or selected_idx_ >= static_cast<int>(curves_.size()))
        {
            ImGui::TextDisabled("Select a curve to edit its metadata.");
            return;
        }

        auto& curve = curves_[static_cast<std::size_t>(selected_idx_)];

        // Render File and Original name as selectable-but-flat text: a
        // read-only InputText with transparent frame background and zero
        // padding looks like plain ImGui::Text but lets the user click-drag
        // to select the value and copy it (Ctrl+C).
        float name_width = ImGui::GetContentRegionAvail().x * 0.6f;
        ImVec4 transparent(0.0f, 0.0f, 0.0f, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg,        transparent);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, transparent);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  transparent);

        ImGui::TextUnformatted("File:");
        ImGui::SameLine();
        std::string filename = std::filesystem::path(curve.path).filename().string();
        ImGui::SetNextItemWidth(name_width);
        ImGui::InputText("##file", filename.data(), filename.size() + 1,
                         ImGuiInputTextFlags_ReadOnly);

        ImGui::TextUnformatted("Original name:");
        ImGui::SameLine();
        std::string original = curve.diff->original_name();
        ImGui::SetNextItemWidth(name_width);
        ImGui::InputText("##original", original.data(), original.size() + 1,
                         ImGuiInputTextFlags_ReadOnly);

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
        ImGui::Separator();
        ImGui::Spacing();

        // Use a dynamic std::string here rather than a fixed char[256]: the
        // bulk prefix helper can push the resulting display_name past any
        // fixed cap, and a truncating buffer would silently overwrite the
        // full name with the truncated form on the first edit.
        std::string display = curve.diff->display_name();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
        if (input_text_string("Display Name", display))
        {
            curve.diff->set_display_name(display);
            curve.up->set_display_name(display);
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

    void Editor::apply_prefix_to_all(std::string const& prefix)
    {
        if (prefix.empty())
        {
            return;
        }
        for (auto& curve : curves_)
        {
            // Fall back to original_name when no display override exists, so
            // the visible label after prefixing matches what the user saw
            // before the click (rather than stranding an empty display name).
            std::string current = curve.diff->display_name();
            if (current.empty())
            {
                current = curve.diff->original_name();
            }
            if (current.rfind(prefix, 0) == 0)
            {
                // Already prefixed — skip so repeated clicks are idempotent.
                continue;
            }
            std::string updated = prefix + current;
            curve.diff->set_display_name(updated);
            curve.up->set_display_name(updated);
            curve.dirty = true;
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
            char const* plural = "s";
            if (dirty_count == 1)
            {
                plural = "";
            }
            ImGui::TextColored(orange,
                               "%zu curve%s with unsaved changes",
                               dirty_count, plural);
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
