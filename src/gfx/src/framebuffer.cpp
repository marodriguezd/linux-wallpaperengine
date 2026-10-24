#include "we/gfx/framebuffer.hpp"

#include <algorithm>

namespace we::gfx {

Framebuffer::~Framebuffer () { this->destroy (); }

Framebuffer::Framebuffer (Framebuffer&& other) noexcept :
    framebuffer_ (other.framebuffer_), color_ (other.color_), width_ (other.width_), height_ (other.height_),
    format_ (other.format_) {
    other.framebuffer_ = 0;
    other.color_ = 0;
    other.width_ = 0;
    other.height_ = 0;
}

Framebuffer& Framebuffer::operator= (Framebuffer&& other) noexcept {
    if (this != &other) {
        this->destroy ();
        this->framebuffer_ = other.framebuffer_;
        this->color_ = other.color_;
        this->width_ = other.width_;
        this->height_ = other.height_;
        this->format_ = other.format_;
        other.framebuffer_ = 0;
        other.color_ = 0;
        other.width_ = 0;
        other.height_ = 0;
    }

    return *this;
}

void Framebuffer::destroy () noexcept {
    if (this->framebuffer_ != 0) {
        glDeleteFramebuffers (1, &this->framebuffer_);
        this->framebuffer_ = 0;
    }

    if (this->color_ != 0) {
        glDeleteTextures (1, &this->color_);
        this->color_ = 0;
    }
}

std::expected<Framebuffer, core::Error> Framebuffer::create (int width, int height, FramebufferFormat format) {
    Framebuffer result;

    if (!result.allocate (width, height, format)) {
        return core::fail (core::ErrorCode::io, "cannot create an offscreen framebuffer");
    }

    return result;
}

void Framebuffer::resize (int width, int height) {
    if (width == this->width_ && height == this->height_) {
        return;
    }

    const FramebufferFormat format = this->format_;
    this->destroy ();
    (void)this->allocate (width, height, format);
}

bool Framebuffer::allocate (int width, int height, FramebufferFormat format) {
    if (width <= 0 || height <= 0) {
        return false;
    }

    const GLenum internal_format = format == FramebufferFormat::rgba16f ? GL_RGBA16F : GL_RGBA8;
    const GLenum upload_type = format == FramebufferFormat::rgba16f ? GL_FLOAT : GL_UNSIGNED_BYTE;

    glGenTextures (1, &this->color_);
    glBindTexture (GL_TEXTURE_2D, this->color_);
    glTexImage2D (GL_TEXTURE_2D, 0, static_cast<GLint> (internal_format), width, height, 0, GL_RGBA, upload_type,
                  nullptr);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture (GL_TEXTURE_2D, 0);

    glGenFramebuffers (1, &this->framebuffer_);
    glBindFramebuffer (GL_FRAMEBUFFER, this->framebuffer_);
    glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, this->color_, 0);
    glDrawBuffer (GL_COLOR_ATTACHMENT0);

    const GLenum status = glCheckFramebufferStatus (GL_FRAMEBUFFER);
    glBindFramebuffer (GL_FRAMEBUFFER, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        this->destroy ();
        return false;
    }

    this->width_ = width;
    this->height_ = height;
    this->format_ = format;
    return true;
}

void Framebuffer::bind () const { glBindFramebuffer (GL_FRAMEBUFFER, this->framebuffer_); }

void Framebuffer::unbind () { glBindFramebuffer (GL_FRAMEBUFFER, 0); }

Texture Framebuffer::view () const { return Texture::view (this->color_, this->width_, this->height_); }

} // namespace we::gfx
