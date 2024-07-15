#include <quick_imgui/quick_imgui.hpp>
#include <webcam_info/webcam_info.hpp>
#include "imgui.h"

auto make_texture() -> GLuint
{
    GLuint textureID; // NOLINT(*init-variables)
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    return textureID;
}

auto main() -> int
{
    std::optional<webcam::Capture> capture;
    GLuint                         texture_id;    // NOLINT(*init-variables)
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
            glBindTexture(GL_TEXTURE_2D, texture_id);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width(), image.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, image.data());
            ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<void*>(static_cast<uint64_t>(texture_id))), ImVec2{850.f * static_cast<float>(image.width()) / static_cast<float>(image.height()), 850.f}); // NOLINT(performance-no-int-to-ptr, *reinterpret-cast)
        }
        ImGui::End();
    });
}