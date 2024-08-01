#include <quick_imgui/quick_imgui.hpp>
#include <webcam_info/webcam_info.hpp>
#include "imgui.h"
#include "glad/glad.h"

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
    std::unique_ptr<webcam::Capture> capture;
    GLuint                         texture_id;    // NOLINT(*init-variables)
    quick_imgui::loop("webcam_info tests", [&]() { // Open a window and run all the ImGui-related code
        ImGui::Begin("webcam_info tests");

        auto const webcam_infos = webcam::grab_all_infos();
        for (auto const& info : webcam_infos)
        {
            if (ImGui::CollapsingHeader(info.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
            {
                for (size_t i {0}; i < info.available_resolutions.size(); i++)
                {
                    auto const &resolution {info.available_resolutions[i]};
                    ImGui::Text("width : %d / height : %d", resolution.width(), resolution.height());
                    ImGui::PushID(i);
                    if (ImGui::Button("Open webcam"))
                    {
                        capture = std::make_unique<webcam::Capture>(info.unique_id, resolution);
                    }
                    ImGui::PopID();
                }
            }
        }
        if (capture != nullptr)
        {
            auto const image = capture->image();
            if (image.has_value()){
            glBindTexture(GL_TEXTURE_2D, texture_id);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, image->width(), image->height(), 0, GL_RGB, GL_UNSIGNED_BYTE, image->data());
            ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<void*>(static_cast<uint64_t>(texture_id))), ImVec2{850.f * static_cast<float>(image->width()) / static_cast<float>(image->height()), 850.f}); // NOLINT(performance-no-int-to-ptr, *reinterpret-cast)
            }
        }
        ImGui::End();
    });
}