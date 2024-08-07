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

template<class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

auto main() -> int
{
    std::unique_ptr<wcam::Capture> capture;
    GLuint                         texture_id{0};  // NOLINT(*init-variables)
    quick_imgui::loop("webcam_info tests", [&]() { // Open a window and run all the ImGui-related code
        if (texture_id == 0)
            texture_id = make_texture();
        ImGui::Begin("webcam_info tests");

        try
        {
            auto const webcam_infos = wcam::grab_all_infos();
        }
        catch (std::exception const& e)
        {
            std::cerr << "Exception occurred: " << e.what() << '\n';
            // capture = nullptr;
            throw;
        }
        auto const webcam_infos = wcam::grab_all_infos();
        int        imgui_id{0};
        for (auto const& info : webcam_infos)
        {
            if (ImGui::CollapsingHeader(std::format("{} (ID: {})", info.name, info.unique_id.as_string()).c_str(), ImGuiTreeNodeFlags_DefaultOpen))
            {
                for (auto const& resolution : info.available_resolutions)
                {
                    ImGui::Text("%d x %d", resolution.width(), resolution.height());
                    ImGui::PushID(imgui_id++);
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
            auto const                 maybe_image = capture->image();
            static img::Size::DataType width{};
            static img::Size::DataType height{};
            static bool                flip_y{};
            static std::string         error_msg{};
            std::visit(
                overloaded{
                    [&](img::Image const& image) {
                        width     = image.width();
                        height    = image.height();
                        flip_y    = image.row_order() == img::FirstRowIs::Bottom;
                        error_msg = "";
                        glBindTexture(GL_TEXTURE_2D, texture_id);
                        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, static_cast<GLsizei>(image.width()), static_cast<GLsizei>(image.height()), 0, image.pixel_format() == img::PixelFormat::RGB ? GL_RGB : GL_BGR, GL_UNSIGNED_BYTE, image.data());
                    },
                    [](wcam::CaptureError error) {
                        error_msg = wcam::to_string(error);
                    },
                    [](wcam::NoNewImageAvailableYet) {
                        error_msg = "";
                    }
                },
                maybe_image
            );
            if (error_msg.empty())
            {
                ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<void*>(static_cast<uint64_t>(texture_id))), ImVec2{400.f * static_cast<float>(width) / static_cast<float>(height), 400.f}, flip_y ? ImVec2(0., 1.) : ImVec2(0., 0.), flip_y ? ImVec2(1., 0.) : ImVec2(1., 1.)); // NOLINT(performance-no-int-to-ptr, *reinterpret-cast)
                ImGui::Text("%d x %d", width, height);
            }
            else
            {
                ImGui::Text("ERROR: %s", error_msg.c_str());
            }
            if (ImGui::Button("Close Webcam"))
            {
                capture = nullptr;
                // Reset the image, otherwise it will show briefly when opening the next webcam (while the new capture hasn't returned any image yet)
                width  = 0;
                height = 0;
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
            }
        }
        ImGui::End();
    });
}