#include <quick_imgui/quick_imgui.hpp>
#include <webcam_info/webcam_info.hpp>
#include "imgui.h"

auto main() -> int
{
    quick_imgui::loop("webcam_info tests", []() { // Open a window and run all the ImGui-related code
        ImGui::Begin("webcam_info tests");

        auto const webcam_infos = webcam_info::grab_all_webcams_infos();
        for (auto const& info : webcam_infos)
        {
            if (ImGui::CollapsingHeader(info.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
            {
                for (auto const& resolution : info.available_resolutions)
                    ImGui::Text("width : %d / height : %d", resolution.width, resolution.height);
            }
        }
        ImGui::End();
    });
}