#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "we/core/error.hpp"
#include "we/display/display.hpp"
#include "we/gfx/context.hpp"
#include "we/gfx/renderer.hpp"
#include "we/power/battery.hpp"
#include "we/power/pacer.hpp"
#include "we/scene/asset_container.hpp"
#include "we/scene/project.hpp"
#include "we/scene/scene.hpp"
#include "we/scene/scene_renderer.hpp"
#include "we/scene/video.hpp"

namespace we::app {

struct EngineOptions {
    display::DriverOptions display;
    int fps_ac = 60;
    int fps_battery = 15;
    bool pause_on_fullscreen = true;
    bool pause_on_battery = false;
    bool audio = false;
    float volume = 0.6F;
    bool verbose = false;

    // 0 means run until interrupted.
    std::uint64_t max_frames = 0;
    std::filesystem::path screenshot;
};

class Engine {
public:
    Engine () = default;
    ~Engine ();

    Engine (const Engine&) = delete;
    Engine& operator= (const Engine&) = delete;

    [[nodiscard]] static std::expected<std::unique_ptr<Engine>, core::Error>
    open (const std::filesystem::path& wallpaper, const EngineOptions& options);

    [[nodiscard]] int run ();

    [[nodiscard]] const scene::Project& project () const noexcept { return this->project_; }
    [[nodiscard]] std::size_t output_count () const noexcept { return this->outputs_.size (); }

private:
    void update_power ();
    void render_surface (std::size_t index);
    void draw_video (std::size_t index);

    EngineOptions options_;
    scene::AssetContainer assets_;
    scene::Project project_;
    std::unique_ptr<display::Driver> driver_;
    std::unique_ptr<gfx::Context> context_;
    std::vector<display::Output> outputs_;
    std::vector<std::unique_ptr<display::Surface>> surfaces_;
    std::unique_ptr<gfx::Renderer> renderer_;

    std::unique_ptr<scene::SceneRenderer> scene_renderer_;
    scene::Scene scene_;
    std::unique_ptr<scene::VideoPlayer> video_;

    power::BatteryMonitor battery_;
    power::PowerStatus power_status_;
    power::FramePacer pacer_ { 60.0 };
    std::uint64_t frames_rendered_ = 0;
    bool should_stop_ = false;
};

} // namespace we::app
