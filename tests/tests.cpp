#include <optional>
#include <quick_imgui/quick_imgui.hpp>
#include "glad/glad.h"
#include "imgui.h"
#include "wcam/wcam.hpp"

class TexturePool { // NOLINT(*special-member-functions)
public:
    ~TexturePool()
    {
        glDeleteTextures(static_cast<GLsizei>(_ids.size()), _ids.data());
    }

    auto take() -> GLuint
    {
        if (_ids.empty())
        {
            GLuint id{};
            glGenTextures(1, &id);
            glBindTexture(GL_TEXTURE_2D, id);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            return id;
        }
        else // NOLINT(*else-after-return)
        {
            auto const res = _ids.back();
            _ids.pop_back();
            return res;
        }
    }

    void give_back(GLuint id)
    {
        _ids.push_back(id);
    }

private:
    std::vector<GLuint> _ids{};
};

auto texture_pool() -> TexturePool&
{
    static auto instance = TexturePool{};
    return instance;
}

class Image : public wcam::Image { // NOLINT(*special-member-functions)
public:
    ~Image() override
    {
        if (_texture_id != 0)
            texture_pool().give_back(_texture_id);
    }

    auto imgui_texture_id() const -> ImTextureID
    {
        if (_gen_texture.has_value())
        {
            if (_texture_id == 0)
                _texture_id = texture_pool().take();
            (*_gen_texture)();
            _gen_texture.reset();
        }
        return static_cast<ImTextureID>(reinterpret_cast<void*>(static_cast<uint64_t>(_texture_id))); // NOLINT(performance-no-int-to-ptr, *reinterpret-cast)
    }

    auto width() const -> wcam::Resolution::DataType { return _resolution.width(); }
    auto height() const -> wcam::Resolution::DataType { return _resolution.height(); }
    auto row_order() const -> wcam::FirstRowIs { return _row_order; }

    void set_data(wcam::ImageDataView<wcam::RGB24> const& rgb_data) override
    {
        _resolution  = rgb_data.resolution();
        _row_order   = rgb_data.row_order();
        _gen_texture = [owned_rgb_data = rgb_data.to_owning(), this]() { // rgb_data will not live past this function, so we need to take a copy (which will just be a move in some cases)
            glBindTexture(GL_TEXTURE_2D, _texture_id);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, static_cast<GLsizei>(owned_rgb_data.resolution().width()), static_cast<GLsizei>(owned_rgb_data.resolution().height()), 0, GL_RGB, GL_UNSIGNED_BYTE, owned_rgb_data.data());
        };
    }

    void set_data(wcam::ImageDataView<wcam::BGR24> const& bgr_data) override
    {
        _resolution  = bgr_data.resolution();
        _row_order   = bgr_data.row_order();
        _gen_texture = [owned_bgr_data = bgr_data.to_owning(), this]() { // bgr_data will not live past this function, so we need to take a copy (which will just be a move in some cases)
            glBindTexture(GL_TEXTURE_2D, _texture_id);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, static_cast<GLsizei>(owned_bgr_data.resolution().width()), static_cast<GLsizei>(owned_bgr_data.resolution().height()), 0, GL_BGR, GL_UNSIGNED_BYTE, owned_bgr_data.data());
        };
    }

private:
    mutable GLuint                               _texture_id{0};
    mutable std::optional<std::function<void()>> _gen_texture{}; // Since OpenGL calls must happen on the main thread, when set_data is called (from another thread) we just store the thing to do in this function, and call it later, on the main thread
    wcam::Resolution                             _resolution{};
    wcam::FirstRowIs                             _row_order{};
};

class WebcamWindow {
public:
    void update()
    {
        _timer.imgui_plot();
        _timer.start();
        imgui_select_webcam();
        imgui_show_open_webcam();
        _timer.stop();
    }

    void imgui_select_webcam()
    {
        auto const webcam_infos = wcam::all_webcams_info();
        for (auto const& info : webcam_infos)
        {
            ImGui::PushID(info.id.as_string().c_str());
            ImGui::NewLine();

            ImGui::SeparatorText((info.name + " (ID: " + info.id.as_string() + ")").c_str());
            auto const selected_resolution = wcam::get_selected_resolution(info.id);
            if (ImGui::BeginCombo("Resolution", wcam::to_string(selected_resolution).c_str()))
            {
                for (auto const& resolution : info.resolutions)
                {
                    bool const is_selected = resolution == selected_resolution;
                    if (ImGui::Selectable(wcam::to_string(resolution).c_str(), is_selected))
                        wcam::set_selected_resolution(info.id, resolution);

                    if (is_selected) // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            if (ImGui::Button("Open webcam"))
                _webcam = wcam::open_webcam(info.id);

            ImGui::PopID();
        }
    }

    void imgui_show_open_webcam()
    {
        if (!_webcam.has_value())
            return;

        _maybe_image = _webcam->image(); // We need to keep the image alive till the end of the frame, so we take a copy of the shared_ptr. The image stored in the _webcam can be destroyed at any time if a new image is created by the background thread
        std::visit(
            wcam::overloaded{
                [&](wcam::ImageNotInitYet) {
                    ImGui::TextUnformatted("LOADING");
                },
                [&](std::shared_ptr<wcam::Image const> const& image) {
                    auto const& img    = *static_cast<Image const*>(image.get()); // NOLINT(*static-cast-downcast)
                    bool const  flip_y = img.row_order() == wcam::FirstRowIs::Bottom;

                    auto const w = ImGui::GetContentRegionAvail().x;
                    ImGui::Image(img.imgui_texture_id(), ImVec2{w, w / static_cast<float>(img.width()) * static_cast<float>(img.height())}, flip_y ? ImVec2(0., 1.) : ImVec2(0., 0.), flip_y ? ImVec2(1., 0.) : ImVec2(1., 1.));
                    ImGui::Text("%d x %d", img.width(), img.height());
                },
                [&](wcam::CaptureError const& error) {
                    ImGui::Text("ERROR: %s", wcam::to_string(error).c_str());
                }
            },
            _maybe_image
        );
        if (ImGui::Button("Close Webcam"))
        {
            _webcam      = std::nullopt;
            _maybe_image = wcam::ImageNotInitYet{}; // Make sure we don't keep the shared_ptr alive for no reason
        }
    }

private:
    quick_imgui::AverageTime          _timer{};
    std::optional<wcam::SharedWebcam> _webcam{};
    wcam::MaybeImage                  _maybe_image{};
    wcam::KeepLibraryAlive            _keep_wcam_alive{}; // We choose to keep the library running for the whole duration of the program.
                                                          // But in a real application you would probably want to only have the library active if you are actively using or looking to use a camera.
                                                          // For example in OBS you would store one wcam::KeepLibraryAlive{} in each of the webcam Sources, so that the library is only active while a webcam source is in the scene, or when the window to select which webcam to use is open
                                                          // While the library is alive, it has a thread running in the background constantly refreshing its list of infos on which webcam are plugged in, and if webcams are currently beeing used this thread also checks to restart them if they failed (eg if the webcam was already used by another application when we tried to open it)
};

auto main() -> int
{
    wcam::set_image_type<Image>(); // Must be called before using anything from the library
    auto windows = std::vector<WebcamWindow>(3);
    quick_imgui::loop("wcam tests", [&]() {
        ImGui::Begin("wcam test");
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