#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "we/core/error.hpp"
#include "we/gfx/framebuffer.hpp"
#include "we/gfx/texture.hpp"

struct mpv_handle;
struct mpv_render_context;

namespace we::scene {

struct VideoOptions {
    bool audio = false;
    float volume = 0.6F;
    bool hardware_decode = true;
};

// Plays a video wallpaper through libmpv's OpenGL render API. The decoded
// frame is rendered directly into an offscreen texture: the same texture can be
// drawn on every output, so multi-monitor cloning costs one decode.
class VideoPlayer {
public:
    VideoPlayer () = default;
    ~VideoPlayer ();

    VideoPlayer (const VideoPlayer&) = delete;
    VideoPlayer& operator= (const VideoPlayer&) = delete;
    VideoPlayer (VideoPlayer&&) = delete;
    VideoPlayer& operator= (VideoPlayer&&) = delete;

    // Requires a current OpenGL context.
    [[nodiscard]] static std::expected<std::unique_ptr<VideoPlayer>, core::Error>
    create (const VideoOptions& options = {});

    [[nodiscard]] std::expected<void, core::Error> load (const std::filesystem::path& file);
    [[nodiscard]] std::expected<void, core::Error> load (const std::vector<std::byte>& data,
                                                         const std::string& name_hint);

    // Decodes and uploads the next frame if mpv produced one. Returns true when
    // the texture changed.
    bool update ();

    void play ();
    void pause ();
    void set_paused (bool paused);
    void set_loop (bool loop);
    void seek (double seconds);

    [[nodiscard]] bool ready () const noexcept { return this->frame_texture_.valid (); }
    [[nodiscard]] const gfx::Texture& texture () const noexcept { return this->frame_texture_; }
    [[nodiscard]] int width () const noexcept { return this->frame_texture_.width (); }
    [[nodiscard]] int height () const noexcept { return this->frame_texture_.height (); }
    [[nodiscard]] double position () const;

    // Must be called after presenting the mpv texture so mpv can pace decoding.
    void report_swap ();

    [[nodiscard]] std::string_view last_error () const noexcept { return this->error_; }

private:
    void apply_options ();
    [[nodiscard]] bool ensure_framebuffer (int width, int height);

    mpv_handle* mpv_ = nullptr;
    mpv_render_context* render_ = nullptr;
    gfx::Framebuffer framebuffer_;
    gfx::Texture frame_texture_;
    std::string error_;
    VideoOptions options_;
    int video_width_ = 0;
    int video_height_ = 0;
    bool loaded_ = false;
};

} // namespace we::scene
