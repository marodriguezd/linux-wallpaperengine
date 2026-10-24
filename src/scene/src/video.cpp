#include "we/scene/video.hpp"

#include "we/core/log.hpp"
#include "we/gfx/context.hpp"
#include "we/gfx/gl.hpp"

#include <mpv/client.h>
#include <mpv/render_gl.h>

#include <EGL/egl.h>

#include <cstring>
#include <fstream>
#include <vector>

namespace we::scene {
namespace {

const char* mpv_error (int code) { return mpv_error_string (code); }

void* get_proc_address (void*, const char* name) {
    return reinterpret_cast<void*> (eglGetProcAddress (name));
}

} // namespace

VideoPlayer::~VideoPlayer () {
    if (this->render_ != nullptr) {
        mpv_render_context_free (this->render_);
        this->render_ = nullptr;
    }

    if (this->mpv_ != nullptr) {
        mpv_terminate_destroy (this->mpv_);
        this->mpv_ = nullptr;
    }
}

std::expected<std::unique_ptr<VideoPlayer>, core::Error> VideoPlayer::create (const VideoOptions& options) {
    auto player = std::make_unique<VideoPlayer> ();
    player->options_ = options;
    player->mpv_ = mpv_create ();

    if (player->mpv_ == nullptr) {
        return core::fail (core::ErrorCode::io, "mpv_create failed");
    }

    player->apply_options ();

    int status = mpv_initialize (player->mpv_);

    if (status < 0) {
        return core::fail (core::ErrorCode::io, std::string ("mpv_initialize failed: ") + mpv_error (status));
    }

    mpv_opengl_init_params init {};
    init.get_proc_address = get_proc_address;
    init.get_proc_address_ctx = nullptr;

    int advanced = 0;

    mpv_render_param params[] = {
        { MPV_RENDER_PARAM_API_TYPE, const_cast<char*> (MPV_RENDER_API_TYPE_OPENGL) },
        { MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &init },
        { MPV_RENDER_PARAM_ADVANCED_CONTROL, &advanced },
        { MPV_RENDER_PARAM_INVALID, nullptr },
    };

    status = mpv_render_context_create (&player->render_, player->mpv_, params);

    if (status < 0) {
        return core::fail (
            core::ErrorCode::io, std::string ("mpv_render_context_create failed: ") + mpv_error (status)
        );
    }

    // No update callback: without MPV_RENDER_PARAM_ADVANCED_CONTROL mpv only
    // reports availability once, so the engine polls mpv_render_context_update()
    // from its own frame loop instead (see update()).
    return player;
}

void VideoPlayer::apply_options () {
    mpv_set_option_string (this->mpv_, "vo", "libmpv");
    mpv_set_option_string (this->mpv_, "idle", "yes");
    mpv_set_option_string (this->mpv_, "keep-open", "no");
    mpv_set_option_string (this->mpv_, "loop-file", "inf");
    mpv_set_option_string (this->mpv_, "hwdec", this->options_.hardware_decode ? "auto-safe" : "no");
    mpv_set_option_string (this->mpv_, "audio", this->options_.audio ? "auto" : "no");
    mpv_set_option_string (this->mpv_, "terminal", "no");
    mpv_set_option_string (this->mpv_, "input-default-bindings", "no");
    mpv_set_option_string (this->mpv_, "osc", "no");

    const std::string volume = std::to_string (static_cast<int> (this->options_.volume * 100.0F));
    mpv_set_option_string (this->mpv_, "volume", volume.c_str ());
}

std::expected<void, core::Error> VideoPlayer::load (const std::filesystem::path& file) {
    std::error_code error;

    if (!std::filesystem::is_regular_file (file, error)) {
        return core::fail (core::ErrorCode::not_found, "video file not found: " + file.string ());
    }

    const std::string path = std::filesystem::absolute (file).string ();
    const char* command[] = { "loadfile", path.c_str (), "replace", nullptr };
    const int status = mpv_command (this->mpv_, command);

    if (status < 0) {
        return core::fail (core::ErrorCode::io, std::string ("mpv loadfile failed: ") + mpv_error (status));
    }

    this->loaded_ = true;
    return {};
}

std::expected<void, core::Error> VideoPlayer::load (const std::vector<std::byte>& data,
                                                    const std::string& name_hint) {
    (void)name_hint;

    // mpv can only load from a URL or a file. Streamed data is spooled to a
    // temporary file; video wallpapers are large enough that keeping the whole
    // decoded stream in memory would be worse.
    // Each player gets its own file: scenes can hold several video layers.
    const auto temporary = std::filesystem::temp_directory_path ()
        / ("we-video-stream-" + std::to_string (reinterpret_cast<std::uintptr_t> (this)) + ".mp4");
    std::ofstream stream (temporary, std::ios::binary | std::ios::trunc);

    if (!stream) {
        return core::fail (core::ErrorCode::io, "cannot create a temporary video file");
    }

    stream.write (reinterpret_cast<const char*> (data.data ()), static_cast<std::streamsize> (data.size ()));
    stream.close ();
    return this->load (temporary);
}

void VideoPlayer::play () {
    if (this->mpv_ != nullptr) {
        mpv_set_property_string (this->mpv_, "pause", "no");
    }
}

void VideoPlayer::pause () {
    if (this->mpv_ != nullptr) {
        mpv_set_property_string (this->mpv_, "pause", "yes");
    }
}

void VideoPlayer::set_paused (bool paused) {
    if (this->mpv_ != nullptr) {
        mpv_set_property_string (this->mpv_, "pause", paused ? "yes" : "no");
    }
}

void VideoPlayer::set_loop (bool loop) {
    if (this->mpv_ != nullptr) {
        mpv_set_property_string (this->mpv_, "loop-file", loop ? "inf" : "no");
    }
}

void VideoPlayer::seek (double seconds) {
    if (this->mpv_ == nullptr) {
        return;
    }

    const std::string value = std::to_string (seconds);
    const char* command[] = { "seek", value.c_str (), "absolute", nullptr };
    mpv_command (this->mpv_, command);
}

double VideoPlayer::position () const {
    if (this->mpv_ == nullptr) {
        return 0.0;
    }

    double value = 0.0;
    mpv_get_property (this->mpv_, "time-pos", MPV_FORMAT_DOUBLE, &value);
    return value;
}

bool VideoPlayer::ensure_framebuffer (int width, int height) {
    if (this->framebuffer_.valid () && this->framebuffer_.width () == width
        && this->framebuffer_.height () == height) {
        this->frame_texture_ = this->framebuffer_.view ();
        return true;
    }

    auto framebuffer = gfx::Framebuffer::create (width, height, gfx::FramebufferFormat::rgba16f);

    if (!framebuffer) {
        this->error_ = framebuffer.error ().message;
        core::log_error (this->error_);
        return false;
    }

    this->framebuffer_ = std::move (*framebuffer);
    this->video_width_ = width;
    this->video_height_ = height;
    this->frame_texture_ = this->framebuffer_.view ();
    return true;
}

bool VideoPlayer::update () {
    if (this->render_ == nullptr || !this->loaded_) {
        return false;
    }

    // mpv is polled once per engine frame: without advanced control the update
    // callback is not invoked for every frame, and gating on it would deliver a
    // single (often empty) frame and then stall forever.
    const std::uint64_t flags = mpv_render_context_update (this->render_);

    if ((flags & MPV_RENDER_UPDATE_FRAME) == 0) {
        return false;
    }

    std::int64_t width = 0;
    std::int64_t height = 0;

    if (mpv_get_property (this->mpv_, "width", MPV_FORMAT_INT64, &width) < 0
        || mpv_get_property (this->mpv_, "height", MPV_FORMAT_INT64, &height) < 0 || width <= 0 || height <= 0) {
        return false;
    }

    if (!this->ensure_framebuffer (static_cast<int> (width), static_cast<int> (height))) {
        return false;
    }

    mpv_opengl_fbo fbo {};
    fbo.fbo = static_cast<int> (this->framebuffer_.id ());
    fbo.w = this->framebuffer_.width ();
    fbo.h = this->framebuffer_.height ();
    fbo.internal_format = GL_RGBA16F;

    // The sprite renderer samples with the top-left origin, which matches
    // mpv's unflipped output; flipping would render the video upside down.
    int flip = 0;

    // The engine owns frame pacing (FramePacer + swap interval), so mpv must not
    // wait for its own target time: with blocking enabled and a swap interval of
    // 1 the renderer can skip every frame while the previous one is still held
    // back, which leaves the texture black on drivers that discard the target.
    int block_for_target_time = 0;

    mpv_render_param params[] = {
        { MPV_RENDER_PARAM_OPENGL_FBO, &fbo },
        { MPV_RENDER_PARAM_FLIP_Y, &flip },
        { MPV_RENDER_PARAM_BLOCK_FOR_TARGET_TIME, &block_for_target_time },
        { MPV_RENDER_PARAM_INVALID, nullptr },
    };

    // Deliberately no glClear here: if mpv defers a frame, the previous one must
    // stay visible instead of flashing black.
    this->framebuffer_.bind ();
    glViewport (0, 0, fbo.w, fbo.h);
    glDisable (GL_SCISSOR_TEST);
    const int status = mpv_render_context_render (this->render_, params);
    gfx::Framebuffer::unbind ();

    if (status < 0) {
        this->error_ = std::string ("mpv render failed: ") + mpv_error (status);
        core::log_error (this->error_);
        return false;
    }

    return true;
}

void VideoPlayer::report_swap () {
    if (this->render_ != nullptr) {
        mpv_render_context_report_swap (this->render_);
    }
}

} // namespace we::scene
