#include <exception>
#include <iostream>
#include <mutex>
#include <quick_imgui/quick_imgui.hpp>
#include "glad/glad.h"
#include "imgui.h"
#include "wcam/wcam.hpp"

class TexturePool { // TODO not needed, Image can create their own texture (deferred, on the main thread)
public:
    TexturePool()
    {
        _ids.resize(20);
        for (auto& id : _ids)
        {
            glGenTextures(1, &id);
            glBindTexture(GL_TEXTURE_2D, id);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
    }

    auto take() -> GLuint
    {
        std::scoped_lock lock{_mutex};

        if (_ids.empty())
        {
            assert(false);
            return 0;
        }
        auto const res = _ids.back();
        _ids.pop_back();
        return res;
    }

    void give_back(GLuint id)
    {
        std::scoped_lock lock{_mutex};
        _ids.push_back(id);
    }

    // ~TexturePool{
    // TODO
    // glDeleteTextures(1, &_texture_id);
    // }

private:
    std::vector<GLuint> _ids{};
    std::mutex          _mutex{};
};

auto texture_pool() -> TexturePool&
{
    static auto instance = TexturePool{};
    return instance;
}

class Image : public wcam::Image {
public:
    Image()
        : _texture_id{texture_pool().take()}
    {
    }

    ~Image() override
    {
        texture_pool().give_back(_texture_id);
    }

    auto imgui_texture_id() const -> ImTextureID
    {
        if (_gen_texture.has_value())
        {
            (*_gen_texture)();
            _gen_texture.reset();
        }
        return static_cast<ImTextureID>(reinterpret_cast<void*>(static_cast<uint64_t>(_texture_id))); // NOLINT(performance-no-int-to-ptr, *reinterpret-cast)
    }

    void set_data(wcam::ImageDataView<wcam::RGB24> rgb_data) override
    {
        _gen_texture = [owned_rgb_data = rgb_data.copy(), this]() {
            glBindTexture(GL_TEXTURE_2D, _texture_id);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, static_cast<GLsizei>(owned_rgb_data.resolution().width()), static_cast<GLsizei>(owned_rgb_data.resolution().height()), 0, GL_RGB, GL_UNSIGNED_BYTE, owned_rgb_data.data());
        };
    }

    void set_data(wcam::ImageDataView<wcam::BGR24> bgr_data) override
    {
        _gen_texture = [owned_bgr_data = bgr_data.copy(), this]() {
            glBindTexture(GL_TEXTURE_2D, _texture_id);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, static_cast<GLsizei>(owned_bgr_data.resolution().width()), static_cast<GLsizei>(owned_bgr_data.resolution().height()), 0, GL_BGR, GL_UNSIGNED_BYTE, owned_bgr_data.data());
        };
    }

private:
    GLuint                                       _texture_id{};
    mutable std::optional<std::function<void()>> _gen_texture{}; // Since OpenGL calls must happen on the main thread, when set_data is called (from another thread) we just store the thing to do in this function, and call it later, on the main thread
};

class WebcamWindow {
public:
    void update()
    {
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
            auto const                                maybe_image = capture->image();
            static std::shared_ptr<wcam::Image const> image{};
            static std::string                        error_msg{};
            std::visit(
                wcam::overloaded{
                    [&](std::shared_ptr<wcam::Image const> const& imag) {
                        image = imag;
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
                        image = nullptr;
                    }
                },
                maybe_image
            );
            if (error_msg.empty())
            {
                if (image != nullptr)
                {
                    auto const& im     = *static_cast<Image const*>(image.get());
                    bool const  flip_y = im.row_order() == wcam::FirstRowIs::Bottom;

                    auto const w = ImGui::GetContentRegionAvail().x;
                    ImGui::Image(im.imgui_texture_id(), ImVec2{w, w / static_cast<float>(im.width()) * static_cast<float>(im.height())}, flip_y ? ImVec2(0., 1.) : ImVec2(0., 0.), flip_y ? ImVec2(1., 0.) : ImVec2(1., 1.));
                    ImGui::Text("%d x %d", im.width(), im.height());
                }
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
    wcam::set_image_type<Image>();
    auto windows = std::vector<WebcamWindow>(3);
    bool is_first_frame{true};
    quick_imgui::loop("webcam_info tests", [&]() {
        if (is_first_frame)
        {
            texture_pool(); // Init the texture pool, on the main thread to make sure all the textures can be successfully created
            is_first_frame = false;
        }

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