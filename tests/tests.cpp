#include <quick_imgui/quick_imgui.hpp>
#include <webcam_info/webcam_info.hpp>
#include "imgui.h"

// Learn how to use Dear ImGui: https://coollibs.github.io/contribute/Programming/dear-imgui

auto main(int argc, char* argv[]) -> int
{
    const bool should_run_imgui_tests = argc < 2 || strcmp(argv[1], "-nogpu") != 0;
    if (
        should_run_imgui_tests
    )
    {
        quick_imgui::loop("webcam_info tests", []() { // Open a window and run all the ImGui-related code
            ImGui::Begin("webcam_info tests");
            auto list_webcam_info = webcam_info::get_all_webcams();
            for (auto& info : list_webcam_info)
            {
                ImGui::Text("%s \n", info.name.c_str());
                ImGui::Text("    width : %d / height : %d \n\n", info.width, info.height);
            }

            ImGui::End();
            ImGui::ShowDemoWindow();
        });
    }
}