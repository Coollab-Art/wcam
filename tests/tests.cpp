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

class WebcamWindow {
public:
    void update()
    {
        if (texture_id == 0)
            texture_id = make_texture();

        timer.imgui_plot();
        timer.start();

        try
        {
            auto const webcam_infos = wcam::all_webcams_info();
        }
        catch (std::exception const& e)
        {
            std::cerr << "Exception occurred: " << e.what() << '\n';
            // capture = nullptr;
            throw;
        }
        auto const webcam_infos = wcam::all_webcams_info();
        int        imgui_id{0};
        for (auto const& info : webcam_infos)
        {
            ImGui::NewLine();
            ImGui::PushID(imgui_id++);

            ImGui::SeparatorText((info.name + " (ID: " + info.id.as_string() + ")").c_str());
            auto const selected_resolution = wcam::get_selected_resolution(info.id);
            if (ImGui::BeginCombo("Resolution", wcam::to_string(selected_resolution).c_str()))
            {
                for (auto const& resolution : info.resolutions)
                {
                    bool const is_selected = resolution == selected_resolution;
                    if (ImGui::Selectable(wcam::to_string(resolution).c_str(), is_selected))
                        wcam::set_selected_resolution(info.id, resolution);

                    // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            if (ImGui::Button("Open webcam"))
            {
                try
                {
                    capture = wcam::open_webcam(info.id);
                }
                catch (std::exception const& e)
                {
                    std::cerr << "Exception occurred: " << e.what() << '\n';
                    capture = std::nullopt;
                    throw;
                }
            }
            ImGui::PopID();
        }
        if (capture.has_value())
        {
            auto const                 maybe_image = capture->image();
            static img::Size::DataType width{};
            static img::Size::DataType height{};
            static bool                flip_y{};
            static std::string         error_msg{};
            std::visit(
                wcam::overloaded{
                    [&](std::shared_ptr<img::Image const> const& image) {
                        width     = image->width();
                        height    = image->height();
                        flip_y    = image->row_order() == img::FirstRowIs::Bottom;
                        error_msg = "";
                        glBindTexture(GL_TEXTURE_2D, texture_id);
                        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, static_cast<GLsizei>(image->width()), static_cast<GLsizei>(image->height()), 0, image->pixel_format() == img::PixelFormat::RGB ? GL_RGB : GL_BGR, GL_UNSIGNED_BYTE, image->data());
                    },
                    [](wcam::CaptureError const& error) {
                        error_msg = wcam::to_string(error);
                    },
                    [](wcam::NoNewImageAvailableYet) {
                        error_msg = "";
                    },
                    [](wcam::ImageNotInitYet) { // TODO display a "LOADING"
                        error_msg = "";
                        // Reset the image, otherwise it will show briefly when opening the next webcam (while the new capture hasn't returned any image yet) / when a capture needs to restart because the camera was unplugged and then plugged back
                        width  = 0;
                        height = 0;
                        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
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
                capture = std::nullopt;
            }
        }
        timer.stop();
    }

private:
    GLuint                            texture_id{0};
    quick_imgui::AverageTime          timer{};
    std::optional<wcam::SharedWebcam> capture;
    // TODO update the explanation about how we use KeepLibraryAlive
    wcam::KeepLibraryAlive _keep_wcam_alive{}; // We choose to keep the library running for the whole duration of the program.
                                               // But in a real application you would probably want to only have the library active if you are actively using or looking to use a camera.
                                               // For example in OBS you would store one wcam::KeepLibraryAlive{} in each of the webcam Sources, so that the library is only active while a webcam source is in the scene, or when the window to select which webcam to use is open
                                               // While the library is alive, it has a thread running in the background constantly refreshing its list of infos on which webcam are plugged in, and if webcams are currently beeing used this thread also checks to restart them if they failed (eg if the webcam was already used by another application when we tried to open it)
};

auto main() -> int
{
    auto windows = std::vector<WebcamWindow>(3);
    quick_imgui::loop("webcam_info tests", [&]() {
        ImGui::Begin("main");
        if (ImGui::Button("Add"))
            windows.emplace_back();
        ImGui::SameLine();
        if (ImGui::Button("Remove") && !windows.empty())
            windows.pop_back();
        ImGui::End();
        for (size_t i = 0; i < windows.size(); ++i)
        {
            ImGui::Begin(std::to_string(i + 1).c_str());
            windows[i].update();
            ImGui::End();
        }
    });
}