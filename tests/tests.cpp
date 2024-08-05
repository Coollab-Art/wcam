#include <exception>
#include <iostream>
#include <quick_imgui/quick_imgui.hpp>
#include "glad/glad.h"
#include "imgui.h"
#include "wcam/wcam.hpp"

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
    std::unique_ptr<wcam::Capture> capture;
    GLuint                         texture_id{0};  // NOLINT(*init-variables)
    quick_imgui::loop("webcam_info tests", [&]() { // Open a window and run all the ImGui-related code
        if (texture_id == 0)
            texture_id = make_texture();
        ImGui::Begin("webcam_info tests");

        auto const webcam_infos = wcam::grab_all_infos();
        for (auto const& info : webcam_infos)
        {
            if (ImGui::CollapsingHeader(info.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
            {
                for (size_t i{0}; i < info.available_resolutions.size(); i++)
                {
                    auto const& resolution{info.available_resolutions[i]};
                    ImGui::Text("%d x %d", resolution.width(), resolution.height());
                    ImGui::PushID(i);
                    if (ImGui::Button("Open webcam"))
                    {
                        try
                        {
                            capture = std::make_unique<wcam::Capture>(info.unique_id, resolution);
                        }
                        catch (std::exception const& e)
                        {
                            std::cerr << "Exception occurred: " << e.what() << '\n';
                            capture = nullptr;
                            throw;
                        }
                    }
                    ImGui::PopID();
                }
            }
        }
        if (capture != nullptr)
        {
            auto const image = capture->image();
            static int width;
            static int height;
            if (image.has_value())
            {
                width  = image->width();
                height = image->height();
                glBindTexture(GL_TEXTURE_2D, texture_id);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, image->width(), image->height(), 0, GL_BGR, GL_UNSIGNED_BYTE, image->data());
            }
            ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<void*>(static_cast<uint64_t>(texture_id))), ImVec2{400.f * static_cast<float>(width) / static_cast<float>(height), 400.f}, ImVec2(0., 1.), ImVec2(1., 0.)); // NOLINT(performance-no-int-to-ptr, *reinterpret-cast)
            if (ImGui::Button("Close Webcam"))
                capture = nullptr;
        }
        ImGui::End();
    });
}