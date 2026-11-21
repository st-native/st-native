// SPDX-License-Identifier: GPL-3.0-or-later
#include "gl_frame_bridge.hpp"

#include <SDL3/SDL.h>
#include <frontend/glad/glad.h>

#include <algorithm>

#include "screen_layout.hpp"

namespace st {

struct OffscreenGlContext::State {
    bool owns_video_subsystem = false;
    std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window{nullptr, SDL_DestroyWindow};
    SDL_GLContext gl = nullptr;
    GLuint read_fb = 0;

    ~State() {
        if (gl && window) {
            SDL_GL_MakeCurrent(window.get(), gl);
            if (read_fb) glDeleteFramebuffers(1, &read_fb);
            SDL_GL_MakeCurrent(nullptr, nullptr);
            SDL_GL_DestroyContext(gl);
        }
        window.reset();
        if (owns_video_subsystem) SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
};

OffscreenGlContext::OffscreenGlContext() = default;
OffscreenGlContext::~OffscreenGlContext() = default;

bool OffscreenGlContext::open() {
    state_ = std::make_unique<State>();
    if (!(SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO)) {
        if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
            state_.reset();
            return false;
        }
        state_->owns_video_subsystem = true;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    for (const auto [major, minor] : {std::pair{4, 3}, std::pair{3, 3}}) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, major);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, minor);
        if (!state_->window)
            state_->window.reset(SDL_CreateWindow("st-offscreen", 1, 1, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN));
        if (!state_->window) break;
        if ((state_->gl = SDL_GL_CreateContext(state_->window.get())) != nullptr) break;
    }
    if (!state_->gl || !SDL_GL_MakeCurrent(state_->window.get(), state_->gl) ||
        !gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress))) {
        state_.reset();
        return false;
    }
    glGenFramebuffers(1, &state_->read_fb);
    return state_->read_fb != 0;
}

bool OffscreenGlContext::read_screens_bgra(unsigned gl_array_tex, int scale,
                                           std::vector<uint32_t>& top_out, std::vector<uint32_t>& bottom_out) {
    if (!state_ || !state_->read_fb || !gl_array_tex) return false;
    const int s = std::max(1, scale);
    const int w = ds_width * s, h = ds_screen_height * s;
    top_out.resize(size_t(w) * size_t(h));
    bottom_out.resize(size_t(w) * size_t(h));

    glBindFramebuffer(GL_READ_FRAMEBUFFER, state_->read_fb);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);

    glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, gl_array_tex, 0, 0);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(0, 0, w, h, GL_BGRA, GL_UNSIGNED_BYTE, top_out.data());

    glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, gl_array_tex, 0, 1);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(0, 0, w, h, GL_BGRA, GL_UNSIGNED_BYTE, bottom_out.data());

    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    return true;
}

bool OffscreenGlContext::read_texture_bgra(unsigned gl_tex_2d, int width, int height, std::vector<uint32_t>& out) {
    if (!state_ || !state_->read_fb || !gl_tex_2d || width <= 0 || height <= 0) return false;
    out.resize(size_t(width) * size_t(height));
    glBindFramebuffer(GL_READ_FRAMEBUFFER, state_->read_fb);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl_tex_2d, 0);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(0, 0, width, height, GL_BGRA, GL_UNSIGNED_BYTE, out.data());
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    return true;
}

}  // namespace st
