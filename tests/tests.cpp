#include <quick_imgui/quick_imgui.hpp>
#include <webcam_info/webcam_info.hpp>
#include "imgui.h"

auto main() -> int
{
    std::optional<webcam::Capture> capture;
    quick_imgui::loop("webcam_info tests", []() { // Open a window and run all the ImGui-related code
        ImGui::Begin("webcam_info tests");

        auto const webcam_infos = webcam::list_all_infos();
        for (auto const& info : webcam_infos)
        {
            if (ImGui::CollapsingHeader(info.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
            {
                for (auto const& resolution : info.available_resolutions)
                {
                    ImGui::Text("width : %d / height : %d", resolution.width, resolution.height);
                    if (ImGui::Button("Open webcam"))
                    {
                        capture = webcam::start_capture(info.unique_id, resolution);
                    }
                }
            }
        }
        if (capture.has_value())
        {
            auto const image = capture.get_image();
            // TODO display image with ImGui
        }
        ImGui::End();
    });
}