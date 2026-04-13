#include <algorithm>

#include "rtm/parser.h"
#include "rtm/metadata.h"
#include "rtm/io/file.h"

#include "main_window.h"
#include "editor.h"

namespace rtm
{
    int MainWindow::load_file(std::string const& path)
    {
        auto io = std::make_unique<rtm::File>(path.c_str());
        io->open(access::Mode::READ_ONLY);
        Parser p{std::move(io)};
        p.load_header();

        if (p.header().major < PROTOCOL_MAJOR)
        {
            return PROTOCOL_MAJOR - p.header().major;
        }

        p.print_header();
        if (not p.load_samples())
        {
            printf("Empty file, skipping.\n");
            return 0;
        }
        p.load_metadata();
        auto color = generate_random_color();
        auto const& header = p.header();
        auto const& meta = p.metadata();
        bool visible = meta.default_visibility;

        auto diff = std::make_shared<Serie>(header.original_name, p.generate_times_diff(), color);
        diff->set_display_name(meta.display_name);
        diff->set_display_weight(meta.display_weight);
        diff_.add_serie(diff, p.diff_min(), p.diff_max(), p.begin(), p.end(), visible);

        auto up = std::make_shared<Serie>(header.original_name, p.generate_times_up(), color);
        up->set_display_name(meta.display_name);
        up->set_display_weight(meta.display_weight);
        up_.add_serie(up, p.up_min(), p.up_max(), p.begin(), p.end(), visible);

        editor_.add_curve(path, header, std::move(diff), std::move(up), visible);
        return 0;
    }

    void MainWindow::load_dataset(std::vector<std::filesystem::path> const& inputs)
    {
        std::vector<std::string> detected_files;
        for (auto const& input : inputs)
        {
            if (std::filesystem::is_directory(input))
            {
                for (auto const& entry : std::filesystem::directory_iterator(input))
                {
                    if (entry.is_regular_file() && entry.path().extension() == ".tick")
                        detected_files.push_back(entry.path().string());
                }
            }
            else if (std::filesystem::is_regular_file(input) && input.extension() == ".tick")
            {
                detected_files.push_back(input.string());
            }
        }
        std::sort(detected_files.begin(), detected_files.end());
        detected_files.erase(std::unique(detected_files.begin(), detected_files.end()), detected_files.end());

        for (auto const& entry : detected_files)
        {
            if (load_file(entry) > 0)
            {
                pending_migration_.push_back(entry);
            }
        }

        diff_.sort_series();
        up_.sort_series();
    }

    void MainWindow::draw_migration_modal()
    {
        if (not pending_migration_.empty())
        {
            ImGui::OpenPopup("File Migration Required");
        }

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(520, 0), ImGuiCond_Appearing);

        if (ImGui::BeginPopupModal("File Migration Required", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextWrapped(
                "The following files use an older format (v1) and need to be "
                "upgraded to v2.0 to be loaded. This operation modifies the "
                "files in place and is not reversible.");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            float list_height = std::min(200.0f, static_cast<float>(pending_migration_.size()) * 20.0f);
            ImGui::BeginChild("FileList", ImVec2(0, list_height), true);
            for (auto const& path : pending_migration_)
            {
                ImGui::TextUnformatted(std::filesystem::path(path).filename().c_str());
            }
            ImGui::EndChild();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Button("Migrate", ImVec2(120, 0)))
            {
                for (auto const& path : pending_migration_)
                {
                    if (migrate_v1_to_v2(path))
                    {
                        load_file(path);
                    }
                }
                pending_migration_.clear();
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            if (ImGui::Button("Skip", ImVec2(120, 0)))
            {
                pending_migration_.clear();
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    void MainWindow::draw()
    {
        draw_migration_modal();

        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("Settings"))
            {
                ImGui::MenuItem("Software anti-aliasing", nullptr,
                    &ImGui::GetStyle().AntiAliasedLines);
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        if (ImGui::BeginTabBar("GraphsTabBar"))
        {
            diff_.draw();
            up_.draw();
            editor_.draw();
            ImGui::EndTabBar();
        }
    }

}
