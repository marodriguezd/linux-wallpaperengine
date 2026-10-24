#include "we/app/engine.hpp"

#include "we/core/log.hpp"
#include "we/steam/steam_locator.hpp"
#include "we/steam/vfs.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <thread>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace we::app {
namespace {

std::atomic<bool> signal_stop { false };

extern "C" void on_signal (int) {
    signal_stop.store (true, std::memory_order_relaxed);
}

// llvmpipe/softpipe cannot sample the dma-buf images that VA-API/drmprime
// hardware decoding hands to the GL renderer; mpv then outputs a black frame
// instead of falling back. Software decoding is both correct and faster there.
bool is_software_renderer (std::string_view renderer) {
    for (const std::string_view name : { "llvmpipe", "softpipe", "swrast", "SwiftShader" }) {
        if (renderer.find (name) != std::string_view::npos) {
            return true;
        }
    }

    return false;
}

// Scenes reference shared resources (models/util, materials, effects) that ship
// with Wallpaper Engine itself, not inside the wallpaper, so its assets directory
// is searched after the wallpaper's own files.
void add_wallpaper_engine_assets (scene::AssetContainer& assets) {
    const auto discovery = steam::SteamLocator ().discover ();

    for (const auto& installation : discovery.wallpaper_engine_installations) {
        assets.add_root (installation / "assets");
        return;
    }
}

std::filesystem::path video_path (const scene::AssetContainer& assets, const std::string& file) {
    const std::string normalized = steam::CaseInsensitiveVfs::normalize (file);
    const auto direct = assets.root () / normalized;

    std::error_code error;

    if (std::filesystem::is_regular_file (direct, error)) {
        return direct;
    }

    return {};
}

} // namespace

Engine::~Engine () = default;

std::expected<std::unique_ptr<Engine>, core::Error>
Engine::open (const std::filesystem::path& wallpaper, const EngineOptions& options) {
    core::set_verbose_logging (options.verbose);

    auto engine = std::make_unique<Engine> ();
    engine->options_ = options;

    auto assets = scene::AssetContainer::open (wallpaper);

    if (!assets) {
        return std::unexpected (std::move (assets.error ()));
    }

    engine->assets_ = std::move (*assets);
    add_wallpaper_engine_assets (engine->assets_);

    auto project = scene::load_project (engine->assets_);

    if (!project) {
        return std::unexpected (std::move (project.error ()));
    }

    engine->project_ = std::move (*project);
    core::log_info ("loaded '", engine->project_.title, "' (", scene::to_string (engine->project_.type), ")");

    if (engine->project_.type == scene::WallpaperType::web) {
        return core::fail (
            core::ErrorCode::unsupported,
            "web wallpapers require the CEF runtime, which is not part of this build"
        );
    }

    if (engine->project_.type == scene::WallpaperType::scene) {
        auto parsed = scene::load_scene (engine->assets_, engine->project_.file);

        if (!parsed) {
            return std::unexpected (std::move (parsed.error ()));
        }

        engine->scene_ = std::move (*parsed);
    }

    auto driver = display::create_driver (engine->options_.display);

    if (!driver) {
        return std::unexpected (std::move (driver.error ()));
    }

    engine->driver_ = std::move (*driver);
    const auto outputs = engine->driver_->outputs ();
    engine->outputs_.assign (outputs.begin (), outputs.end ());

    if (engine->outputs_.empty ()) {
        return core::fail (core::ErrorCode::io, "the display backend reported no outputs");
    }

    auto context = engine->driver_->create_context ();

    if (!context) {
        return std::unexpected (std::move (context.error ()));
    }

    engine->context_ = std::move (*context);
    const std::size_t surface_count
        = engine->driver_->backend () == display::Backend::x11_root ? 1 : engine->outputs_.size ();

    for (std::size_t index = 0; index < surface_count; ++index) {
        auto surface = engine->driver_->create_surface (index, *engine->context_);

        if (!surface) {
            return std::unexpected (std::move (surface.error ()));
        }

        (*surface)->set_vsync (engine->options_.display.vsync);
        engine->surfaces_.push_back (std::move (*surface));
    }

    engine->surfaces_.front ()->make_current ();

    auto renderer = gfx::Renderer::create ();

    if (!renderer) {
        return std::unexpected (std::move (renderer.error ()));
    }

    engine->renderer_ = std::make_unique<gfx::Renderer> (std::move (*renderer));

    if (engine->project_.type == scene::WallpaperType::scene) {
        auto renderer_scene = scene::SceneRenderer::build (engine->scene_, engine->assets_);

        if (!renderer_scene) {
            return std::unexpected (std::move (renderer_scene.error ()));
        }

        engine->scene_renderer_ = std::make_unique<scene::SceneRenderer> (std::move (*renderer_scene));
    } else if (engine->project_.type == scene::WallpaperType::video) {
        scene::VideoOptions video_options;
        video_options.audio = engine->options_.audio;
        video_options.volume = engine->options_.volume;
        video_options.hardware_decode = !is_software_renderer (engine->context_->renderer ());

        auto video = scene::VideoPlayer::create (video_options);

        if (!video) {
            return std::unexpected (std::move (video.error ()));
        }

        engine->video_ = std::move (*video);

        const auto physical = video_path (engine->assets_, engine->project_.file);
        std::expected<void, core::Error> loaded
            = core::fail (core::ErrorCode::not_found, "video file is missing");

        if (!physical.empty ()) {
            loaded = engine->video_->load (physical);
        }

        if (!loaded) {
            auto bytes = engine->assets_.read (engine->project_.file);

            if (!bytes) {
                return core::fail (
                    core::ErrorCode::not_found,
                    "video file '" + engine->project_.file + "' is not available: " + loaded.error ().message
                );
            }

            loaded = engine->video_->load (*bytes, engine->project_.file);
        }

        if (!loaded) {
            return std::unexpected (std::move (loaded.error ()));
        }

        engine->video_->set_loop (true);
        engine->video_->play ();
    }

    if (!engine->options_.screenshot.empty () && engine->options_.max_frames == 0) {
        engine->options_.max_frames = 1;
    }

    engine->power_status_ = engine->battery_.poll ();
    engine->pacer_.set_rate (static_cast<double> (
        engine->power_status_.source == power::PowerSource::battery ? engine->options_.fps_battery
                                                                    : engine->options_.fps_ac
    ));

    return engine;
}

void Engine::update_power () {
    const auto status = this->battery_.poll ();

    if (status.source != this->power_status_.source || status.battery_percent != this->power_status_.battery_percent) {
        core::log_info ("power: ", status.source == power::PowerSource::battery ? "battery" : "ac",
                        status.battery_percent >= 0 ? " " + std::to_string (status.battery_percent) + "%" : "");
    }

    this->power_status_ = status;
}

void Engine::draw_video (std::size_t index) {
    if (this->video_ == nullptr || !this->video_->ready ()) {
        return;
    }

    const auto& surface = *this->surfaces_[index];
    const auto extent = surface.size ();

    if (extent.width <= 0 || extent.height <= 0 || this->video_->width () <= 0 || this->video_->height () <= 0) {
        return;
    }

    const float screen_width = static_cast<float> (extent.width);
    const float screen_height = static_cast<float> (extent.height);
    const float video_width = static_cast<float> (this->video_->width ());
    const float video_height = static_cast<float> (this->video_->height ());

    // Cover: fill the output and crop the overflow, preserving aspect ratio.
    const float scale = std::max (screen_width / video_width, screen_height / video_height);
    const float visible_width = std::min (1.0F, screen_width / (video_width * scale));
    const float visible_height = std::min (1.0F, screen_height / (video_height * scale));

    const core::Rect uv { (1.0F - visible_width) * 0.5F, (1.0F - visible_height) * 0.5F, visible_width,
                          visible_height };

    this->renderer_->draw (this->video_->texture (),
                           core::Rect { 0.0F, 0.0F, screen_width, screen_height }, uv);
}

void Engine::render_surface (std::size_t index) {
    auto& surface = *this->surfaces_[index];
    surface.make_current ();

    const auto extent = surface.size ();
    const core::Color clear = this->scene_renderer_ ? this->scene_renderer_->clear_color ()
                                                    : core::Color { 0.0F, 0.0F, 0.0F, 1.0F };

    this->renderer_->begin (extent.width, extent.height, clear);

    if (this->scene_renderer_) {
        this->scene_renderer_->render (*this->renderer_);
    } else {
        this->draw_video (index);
    }

    this->renderer_->end ();

    const bool capture = !this->options_.screenshot.empty () && this->options_.max_frames != 0
        && this->frames_rendered_ + 1 >= this->options_.max_frames;

    if (capture) {
        std::vector<std::uint8_t> pixels (static_cast<std::size_t> (extent.width)
                                           * static_cast<std::size_t> (extent.height) * 4U);
        surface.readback (pixels);

        if (stbi_write_png (this->options_.screenshot.string ().c_str (), extent.width, extent.height, 4,
                            pixels.data (), extent.width * 4)
            == 0) {
            core::log_error ("cannot write screenshot to ", this->options_.screenshot.string ());
        } else {
            core::log_info ("wrote screenshot ", this->options_.screenshot.string ());
        }
    }

    surface.swap ();
}

int Engine::run () {
    signal_stop.store (false);
    std::signal (SIGINT, on_signal);
    std::signal (SIGTERM, on_signal);

    auto last_power_poll = std::chrono::steady_clock::now ();
    std::size_t rendered_this_cycle = 0;

    while (!this->should_stop_) {
        if (signal_stop.load (std::memory_order_relaxed)) {
            break;
        }

        this->driver_->pump ();

        if (this->driver_->should_close ()) {
            break;
        }

        const auto now = std::chrono::steady_clock::now ();

        if (now - last_power_poll > std::chrono::seconds (5)) {
            this->update_power ();
            last_power_poll = now;
        }

        const bool on_battery = this->power_status_.source == power::PowerSource::battery;

        if (on_battery && this->options_.pause_on_battery) {
            if (this->video_) {
                this->video_->pause ();
            }

            std::this_thread::sleep_for (std::chrono::milliseconds (200));
            continue;
        }

        const int target_fps = on_battery ? this->options_.fps_battery : this->options_.fps_ac;

        if (target_fps <= 0) {
            if (this->video_) {
                this->video_->pause ();
            }

            std::this_thread::sleep_for (std::chrono::milliseconds (250));
            continue;
        }

        this->pacer_.set_rate (static_cast<double> (target_fps));

        // mpv only expects report_swap() for a frame it actually rendered.
        const bool video_frame_rendered = this->video_ != nullptr && this->video_->update ();
        const bool scene_video_rendered = this->scene_renderer_ != nullptr && this->scene_renderer_->update_videos ();

        rendered_this_cycle = 0;

        for (std::size_t index = 0; index < this->surfaces_.size (); ++index) {
            if (this->options_.pause_on_fullscreen && index < this->outputs_.size ()
                && this->driver_->fullscreen_window (index).has_value ()) {
                continue;
            }

            this->render_surface (index);
            ++rendered_this_cycle;
            ++this->frames_rendered_;

            if (this->options_.max_frames != 0 && this->frames_rendered_ >= this->options_.max_frames) {
                this->should_stop_ = true;
                break;
            }
        }

        if (video_frame_rendered && rendered_this_cycle > 0) {
            this->video_->report_swap ();
        }

        if (scene_video_rendered && rendered_this_cycle > 0) {
            this->scene_renderer_->report_video_swap ();
        }

        if (this->should_stop_) {
            break;
        }

        this->pacer_.wait ();
    }

    core::log_info ("stopped after ", this->frames_rendered_, " frames");
    return 0;
}

} // namespace we::app
