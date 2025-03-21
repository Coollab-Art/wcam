#include <optional>
#include <quick_imgui/quick_imgui.hpp>
#include "glad/glad.h"
#include "imgui.h"
#include "wcam/wcam.hpp"

struct Texture {
    GLuint           id{};
    wcam::Resolution resolution{};
};

class TexturePool { // NOLINT(*special-member-functions)
public:
    ~TexturePool()
    {
        for (auto& texture : _textures)
            glDeleteTextures(1, &texture.id);
    }

    auto take(wcam::Resolution resolution) -> Texture // We request a texture with a given resolution, to make sure we don't resize textures all the time. This can save a little bit of performance.
    {
        auto const it = std::find_if(_textures.begin(), _textures.end(), [&](Texture const& texture) {
            return texture.resolution == resolution;
        });
        if (it != _textures.end())
        {
            auto const res = *it;
            _textures.erase(it);
            return res;
        }

        auto texture       = Texture{};
        texture.resolution = resolution;
        glGenTextures(1, &texture.id);
        glBindTexture(GL_TEXTURE_2D, texture.id);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        return texture;
    }

    void give_back(Texture texture)
    {
        _textures.push_back(texture);
    }

private:
    std::vector<Texture> _textures{};
};

static auto texture_pool() -> TexturePool&
{
    static auto instance = TexturePool{};
    return instance;
}

class Image : public wcam::Image { // NOLINT(*special-member-functions)
public:
    ~Image() override
    {
        if (_texture.id != 0)
            texture_pool().give_back(_texture);
    }

    auto imgui_texture_id() const -> ImTextureID
    {
        if (_gen_texture.has_value())
        {
            (*_gen_texture)();
            _gen_texture.reset();
        }
        return static_cast<ImTextureID>(reinterpret_cast<void*>(static_cast<uint64_t>(_texture.id))); // NOLINT(performance-no-int-to-ptr, *reinterpret-cast)
    }

    auto width() const -> wcam::Resolution::DataType { return _resolution.width(); }
    auto height() const -> wcam::Resolution::DataType { return _resolution.height(); }
    auto row_order() const -> wcam::FirstRowIs { return _row_order; }

    void set_data(wcam::ImageDataView<wcam::RGB24> const& rgb_data) override
    {
        _resolution  = rgb_data.resolution();
        _row_order   = rgb_data.row_order();
        _gen_texture = [owned_rgb_data = rgb_data.to_owning(), this]() { // rgb_data will not live past this function, so we need to take a copy (which will just be a move in some cases)
            assert(_texture.id == 0);
            _texture = texture_pool().take(owned_rgb_data.resolution());
            glBindTexture(GL_TEXTURE_2D, _texture.id);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, static_cast<GLsizei>(owned_rgb_data.resolution().width()), static_cast<GLsizei>(owned_rgb_data.resolution().height()), 0, GL_RGB, GL_UNSIGNED_BYTE, owned_rgb_data.data());
        };
    }

    void set_data(wcam::ImageDataView<wcam::BGR24> const& bgr_data) override
    {
        _resolution  = bgr_data.resolution();
        _row_order   = bgr_data.row_order();
        _gen_texture = [owned_bgr_data = bgr_data.to_owning(), this]() { // bgr_data will not live past this function, so we need to take a copy (which will just be a move in some cases)
            assert(_texture.id == 0);
            _texture = texture_pool().take(owned_bgr_data.resolution());
            glBindTexture(GL_TEXTURE_2D, _texture.id);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, static_cast<GLsizei>(owned_bgr_data.resolution().width()), static_cast<GLsizei>(owned_bgr_data.resolution().height()), 0, GL_BGR, GL_UNSIGNED_BYTE, owned_bgr_data.data());
        };
    }

private:
    mutable Texture                              _texture{};
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
};

auto main() -> int
{
    wcam::set_image_type<Image>(); // Must be called before using anything from the library

    auto windows = std::vector<WebcamWindow>(3);
    bool library_enabled{true};

    quick_imgui::loop("wcam tests", [&]() {
        wcam::update(); // Must be called once every frame
        ImGui::Begin("wcam test");
        {
            if (ImGui::Button("Add"))
                windows.emplace_back();
            ImGui::SameLine();
            if (ImGui::Button("Remove") && !windows.empty())
                windows.pop_back();

            ImGui::Checkbox("Library enabled", &library_enabled);
        }
        ImGui::End();

        if (library_enabled)
        {
            for (size_t i = 0; i < windows.size(); ++i)
            {
                ImGui::Begin(std::to_string(i + 1).c_str());
                windows[i].update();
                ImGui::End();
            }
        }
    });
}