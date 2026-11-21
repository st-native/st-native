// SPDX-License-Identifier: GPL-3.0-or-later
#include "launcher_window.hpp"
#include "cursor_interface.hpp"
#include "../input/gamepads.hpp"

#include <RmlUi/Core.h>
#include <RmlUi_Platform_SDL.h>
#include <RmlUi_Renderer_GL3.h>
#include <frontend/glad/glad.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <mutex>
#include <vector>

namespace {

void update_minimum_size(SDL_Window* window) {
    const float scale = SDL_GetWindowDisplayScale(window);
    SDL_SetWindowMinimumSize(window, int(360 * scale), int(280 * scale));
}

constexpr int kDsWidth = st::ds_width, kDsHeight = st::ds_screen_height;

const char* kPresentVS = R"(#version 140
uniform vec4 uRect;
uniform float uLayer;
in vec2 vPos;
out vec3 fUv;
void main() {
    gl_Position = vec4(mix(uRect.xy, uRect.zw, vPos), 0.0, 1.0);
    fUv = vec3(vPos, uLayer);
}
)";

const char* kPresentArrayFS = R"(#version 140
uniform sampler2DArray uTex;
uniform float uOpacity;
in vec3 fUv;
out vec4 oColor;
void main() {
    oColor = vec4(texture(uTex, fUv).rgb, uOpacity);
}
)";

const char* kPresent2DFS = R"(#version 140
uniform sampler2D uTex;
in vec3 fUv;
out vec4 oColor;
void main() {
    oColor = vec4(texture(uTex, fUv.xy).rgb, 1.0);
}
)";

// Capture software sees this pointer because it is part of the fullscreen frame.
void draw_fullscreen_cursor(SDL_Window* window) {
    float mouse_x, mouse_y;
    SDL_GetMouseState(&mouse_x, &mouse_y);
    const bool draw = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) &&
                      SDL_GetMouseFocus() == window;
    int height = 0;
    if (!draw || !SDL_GetWindowSizeInPixels(window, nullptr, &height) ||
        !SDL_HideCursor()) {
        SDL_ShowCursor();
        return;
    }

    const float density = SDL_GetWindowPixelDensity(window);
    const int x = static_cast<int>(mouse_x * density);
    const int y = height - 1 - static_cast<int>(mouse_y * density);
    const int pixel = std::max(1, static_cast<int>(density + .5f));
    constexpr std::array<const char*, 15> arrow{
        "X", "XX", "X.X", "X..X", "X...X", "X....X", "X.....X", "X......X",
        "X.......X", "X....XXXXX", "X..X.X", "X.X X.X", "XX  X.X", "X    X.X", "     XX"
    };
    glEnable(GL_SCISSOR_TEST);
    for (int row = 0; row < static_cast<int>(arrow.size()); ++row) {
        for (int col = 0; arrow[row][col]; ++col) {
            const char color = arrow[row][col];
            if (color == ' ') continue;
            const float value = color == '.' ? 1.f : 0.f;
            glScissor(x + col * pixel, y - (row + 1) * pixel + 1, pixel, pixel);
            glClearColor(value, value, value, 1.f);
            glClear(GL_COLOR_BUFFER_BIT);
        }
    }
    glDisable(GL_SCISSOR_TEST);
}

GLuint compile_shader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    if (!shader) return 0;
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint build_program(const char* vs_src, const char* fs_src) {
    const GLuint vs = compile_shader(GL_VERTEX_SHADER, vs_src);
    const GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fs_src);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return 0;
    }
    const GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glBindAttribLocation(prog, 0, "vPos");
    glBindFragDataLocation(prog, 0, "oColor");
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

bool save_preview(int width, int height, const char* path) {
    std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> image(
        SDL_CreateSurface(width, height, SDL_PIXELFORMAT_RGBA32), SDL_DestroySurface);
    if (!image) return false;
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, image->pixels);
    auto* pixels = static_cast<unsigned char*>(image->pixels);
    for (int y = 0; y < height / 2; ++y) {
        auto* row = pixels + y * image->pitch;
        std::swap_ranges(row, row + image->pitch, pixels + (height - 1 - y) * image->pitch);
    }
    return SDL_SaveBMP(image.get(), path);
}

struct FrameSlot {
    bool is_gl = false;
    bool shared_scene = false;
    bool cinematic = false;
    bool cinematic_is_cpu = false;
    int scale = 1;
    st::GameMenu menu{};
    uint64_t frame_number = 0;
    uint64_t published_ns = 0;
    std::vector<uint32_t> cpu_pixels;
    std::vector<uint32_t> cpu_cinematic_pixels;
    GLuint gl_screens_tex = 0;
    int gl_w = 0, gl_h = 0;
    GLuint gl_cinematic_tex = 0;
    int gl_cw = 0, gl_ch = 0;
    GLsync fence = nullptr;

    void release_gl() {
        if (fence) {
            glDeleteSync(fence);
            fence = nullptr;
        }
        if (gl_screens_tex) {
            glDeleteTextures(1, &gl_screens_tex);
            gl_screens_tex = 0;
            gl_w = gl_h = 0;
        }
        if (gl_cinematic_tex) {
            glDeleteTextures(1, &gl_cinematic_tex);
            gl_cinematic_tex = 0;
            gl_cw = gl_ch = 0;
        }
    }
};

}  // namespace

struct LauncherWindow::State {
    std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window{nullptr, SDL_DestroyWindow};
    std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> worker_window{nullptr, SDL_DestroyWindow};
    SDL_GLContext gl = nullptr;
    SDL_GLContext worker_gl = nullptr;
    GLuint worker_read_fb = 0, worker_draw_fb = 0;
    std::unique_ptr<st::CursorInterface> system;
    std::unique_ptr<RenderInterface_GL3> renderer;
    Rml::Context* pumping_context = nullptr;

    GLuint array_prog = 0, tex2d_prog = 0, quad_vao = 0, quad_vbo = 0;
    GLint u_array_rect = -1, u_array_layer = -1, u_array_tex = -1, u_array_opacity = -1;
    GLint u_2d_rect = -1, u_2d_tex = -1;
    GLuint soft_screens_tex = 0, soft_cinematic_tex = 0;
    GLuint active_screens_tex = 0, active_cinematic_tex = 0;
    int active_scale = 1, active_cinematic_scale = 1;

    std::mutex frame_mutex;
    std::array<FrameSlot, 3> slots{};
    int write_idx = 0, ready_idx = 1, read_idx = 2;
    bool frame_fresh = false;

    st::ScreenLayout layout = st::ScreenLayout::focus;
    std::atomic<int> internal_scale{st::default_internal_scale};
    bool automatic_join = true, shared_scene = false, classic_override = false, cinematic = false;
    st::ScreenEnhancements enhancements;
    st::SecondaryViewSettings secondary;
    bool secondary_peek = false;
    mutable std::mutex audio_mutex;
    std::atomic<uint64_t> audio_revision{1};
    st::AudioSettings audio;
    std::atomic<bool> pause_on_menu{false};
    st::input::Controls controls;
    std::unique_ptr<st::input::Gamepads> gamepads;
    std::function<bool(const SDL_Event&)> input_filter;

    bool show_cinematic() const {
        return cinematic && active_cinematic_tex != 0 && shared_scene && (automatic_join != classic_override);
    }

    st::ScreenViews views(int width, int height) const {
        return st::screen_views(st::effective_screen_layout(layout, automatic_join, shared_scene, classic_override),
                                width, height, secondary, secondary_peek);
    }

    bool ensure_present_pipeline() {
        if (array_prog) return true;
        array_prog = build_program(kPresentVS, kPresentArrayFS);
        tex2d_prog = build_program(kPresentVS, kPresent2DFS);
        if (!array_prog || !tex2d_prog) return SDL_SetError("Could not compile game presentation shaders");

        u_array_rect = glGetUniformLocation(array_prog, "uRect");
        u_array_layer = glGetUniformLocation(array_prog, "uLayer");
        u_array_tex = glGetUniformLocation(array_prog, "uTex");
        u_array_opacity = glGetUniformLocation(array_prog, "uOpacity");
        u_2d_rect = glGetUniformLocation(tex2d_prog, "uRect");
        u_2d_tex = glGetUniformLocation(tex2d_prog, "uTex");

        const float quad[] = {0.f, 0.f, 1.f, 0.f, 0.f, 1.f, 1.f, 1.f};
        glGenBuffers(1, &quad_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
        glGenVertexArrays(1, &quad_vao);
        glBindVertexArray(quad_vao);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        glBindVertexArray(0);

        glGenTextures(1, &soft_screens_tex);
        glBindTexture(GL_TEXTURE_2D_ARRAY, soft_screens_tex);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, kDsWidth, kDsHeight, 2, 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glGenTextures(1, &soft_cinematic_tex);
        glBindTexture(GL_TEXTURE_2D, soft_cinematic_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kDsWidth, kDsHeight, 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        return true;
    }

    void upload_soft_planes(const uint32_t* top, const uint32_t* bottom, const uint32_t* cinematic_pixels) {
        glActiveTexture(GL_TEXTURE0);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glBindTexture(GL_TEXTURE_2D_ARRAY, soft_screens_tex);
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, kDsWidth, kDsHeight, 1, GL_BGRA, GL_UNSIGNED_BYTE, top);
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 1, kDsWidth, kDsHeight, 1, GL_BGRA, GL_UNSIGNED_BYTE, bottom);
        active_screens_tex = soft_screens_tex;
        active_scale = 1;
        if (cinematic_pixels) {
            glBindTexture(GL_TEXTURE_2D, soft_cinematic_tex);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kDsWidth, kDsHeight, GL_BGRA, GL_UNSIGNED_BYTE, cinematic_pixels);
            active_cinematic_tex = soft_cinematic_tex;
            active_cinematic_scale = 1;
        } else {
            active_cinematic_tex = 0;
        }
    }

    static bool SDLCALL redraw_while_exposed(void* userdata, SDL_Event* event) {
        if (event->type != SDL_EVENT_WINDOW_EXPOSED || !SDL_IsMainThread()) return true;
        auto& launcher = *static_cast<LauncherWindow*>(userdata);
        if (auto* context = launcher.state_->pumping_context;
            context && event->window.windowID == SDL_GetWindowID(launcher.window())) {
            // Windows can keep the event pump blocked while the resize handle is held.
            launcher.state_->pumping_context = nullptr;
            launcher.draw(*context);
            launcher.state_->pumping_context = context;
        }
        return true;
    }

    ~State() {
        gamepads.reset();
        if (window) SDL_ShowCursor();
        if (gl && window) {
            SDL_GL_MakeCurrent(window.get(), gl);
            for (auto& slot : slots) slot.release_gl();
            if (soft_screens_tex) glDeleteTextures(1, &soft_screens_tex);
            if (soft_cinematic_tex) glDeleteTextures(1, &soft_cinematic_tex);
            if (quad_vbo) glDeleteBuffers(1, &quad_vbo);
            if (quad_vao) glDeleteVertexArrays(1, &quad_vao);
            if (array_prog) glDeleteProgram(array_prog);
            if (tex2d_prog) glDeleteProgram(tex2d_prog);
        }
        renderer.reset();
        system.reset();
        if (worker_gl) SDL_GL_DestroyContext(worker_gl);
        worker_window.reset();
        if (gl) SDL_GL_DestroyContext(gl);
    }
};

LauncherWindow::LauncherWindow() = default;

LauncherWindow::~LauncherWindow() {
    SDL_RemoveEventWatch(State::redraw_while_exposed, this);
    state_.reset();
    SDL_Quit();
}

bool LauncherWindow::open(const char* title, int width, int height) {
    if (!SDL_Init(SDL_INIT_VIDEO)) return false;
    SDL_InitSubSystem(SDL_INIT_AUDIO);
    if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) return false;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    const float scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    state_ = std::make_unique<State>();
    state_->gamepads = std::make_unique<st::input::Gamepads>();
    for (const auto [major, minor] : {std::pair{4, 3}, std::pair{3, 3}}) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, major);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, minor);
        if (!window())
            state_->window.reset(SDL_CreateWindow(title, int(width * scale), int(height * scale),
                                                  SDL_WINDOW_OPENGL | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_RESIZABLE));
        if (!window()) return false;
        if ((state_->gl = SDL_GL_CreateContext(window())) != nullptr) break;
    }
    update_minimum_size(window());
    if (!state_->gl || !SDL_GL_MakeCurrent(window(), state_->gl) ||
        !gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress)))
        return false;

    // Create a hidden shared OpenGL context for the game thread's melonDS GLRenderer so neither thread
    // contends for the main window's HDC/context.
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
    state_->worker_window.reset(SDL_CreateWindow("st-worker", 1, 1, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN));
    if (state_->worker_window) state_->worker_gl = SDL_GL_CreateContext(state_->worker_window.get());
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 0);
    if (!SDL_GL_MakeCurrent(window(), state_->gl)) return false;

    SDL_GL_SetSwapInterval(1);
    state_->system = std::make_unique<st::CursorInterface>(window());
    state_->renderer = std::make_unique<RenderInterface_GL3>();
    if (!*state_->renderer) return SDL_SetError("Could not initialize the RmlUi OpenGL renderer");
    if (!state_->ensure_present_pipeline()) return false;
    Rml::SetSystemInterface(state_->system.get());
    Rml::SetRenderInterface(state_->renderer.get());
    SDL_AddEventWatch(State::redraw_while_exposed, this);
    return true;
}

SDL_Window* LauncherWindow::window() const { return state_->window.get(); }

st::input::Controls LauncherWindow::controls_settings() const { return state_->controls; }
void LauncherWindow::set_controls_settings(const st::input::Controls& controls) {
    if (controls.validate()) state_->controls = controls;
}
void LauncherWindow::observe_input(const SDL_Event& event) { state_->gamepads->event(event); }
SDL_JoystickID LauncherWindow::active_gamepad() const { return state_->gamepads->active(); }
void LauncherWindow::set_input_filter(std::function<bool(const SDL_Event&)> filter) { state_->input_filter = std::move(filter); }
bool LauncherWindow::bind_worker_graphics() {
    {
        std::lock_guard lock(state_->frame_mutex);
        state_->frame_fresh = false;
    }
    if (!state_->worker_window || !state_->worker_gl ||
        !SDL_GL_MakeCurrent(state_->worker_window.get(), state_->worker_gl))
        return false;
    GLuint fbs[2]{};
    glGenFramebuffers(2, fbs);
    state_->worker_read_fb = fbs[0];
    state_->worker_draw_fb = fbs[1];
    return state_->worker_read_fb != 0 && state_->worker_draw_fb != 0;
}

void LauncherWindow::unbind_worker_graphics() {
    if (!state_->worker_window || !state_->worker_gl) return;
    if (SDL_GL_GetCurrentContext() == state_->worker_gl) {
        GLuint fbs[2] = {state_->worker_read_fb, state_->worker_draw_fb};
        if (fbs[0] || fbs[1]) glDeleteFramebuffers(2, fbs);
        state_->worker_read_fb = state_->worker_draw_fb = 0;
        glFinish();
        SDL_GL_MakeCurrent(nullptr, nullptr);
    }
}

bool LauncherWindow::publish_game_frame(const st::GameFrameInput& input) {
    auto& slot = state_->slots[state_->write_idx];
    if (slot.fence) {
        glDeleteSync(slot.fence);
        slot.fence = nullptr;
    }

    if (input.gl_screens_texture != 0 && state_->worker_read_fb && state_->worker_draw_fb) {
        const int scale = std::max(1, input.scale);
        const int w = kDsWidth * scale, h = kDsHeight * scale;
        glActiveTexture(GL_TEXTURE0);
        if (!slot.gl_screens_tex || slot.gl_w != w || slot.gl_h != h) {
            if (slot.gl_screens_tex) glDeleteTextures(1, &slot.gl_screens_tex);
            glGenTextures(1, &slot.gl_screens_tex);
            glBindTexture(GL_TEXTURE_2D_ARRAY, slot.gl_screens_tex);
            glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, w, h, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            slot.gl_w = w;
            slot.gl_h = h;
        }
        glDisable(GL_SCISSOR_TEST);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, state_->worker_read_fb);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, state_->worker_draw_fb);
        for (int layer = 0; layer < 2; ++layer) {
            glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, input.gl_screens_texture, 0, layer);
            glReadBuffer(GL_COLOR_ATTACHMENT0);
            glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, slot.gl_screens_tex, 0, layer);
            glDrawBuffer(GL_COLOR_ATTACHMENT0);
            glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        }
        if (scale > 1) {
            glBindTexture(GL_TEXTURE_2D_ARRAY, slot.gl_screens_tex);
            glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
        }

        slot.cinematic = false;
        slot.cinematic_is_cpu = false;
        if (input.gl_cinematic_texture != 0) {
            if (!slot.gl_cinematic_tex || slot.gl_cw != w || slot.gl_ch != h) {
                if (slot.gl_cinematic_tex) glDeleteTextures(1, &slot.gl_cinematic_tex);
                glGenTextures(1, &slot.gl_cinematic_tex);
                glBindTexture(GL_TEXTURE_2D, slot.gl_cinematic_tex);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                slot.gl_cw = w;
                slot.gl_ch = h;
            }
            glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, input.gl_cinematic_texture, 0);
            glReadBuffer(GL_COLOR_ATTACHMENT0);
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, slot.gl_cinematic_tex, 0);
            glDrawBuffer(GL_COLOR_ATTACHMENT0);
            glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
            if (scale > 1) {
                glBindTexture(GL_TEXTURE_2D, slot.gl_cinematic_tex);
                glGenerateMipmap(GL_TEXTURE_2D);
            }
            slot.cinematic = true;
        } else if (input.cinematic_pixels != nullptr) {
            slot.cpu_cinematic_pixels.assign(input.cinematic_pixels, input.cinematic_pixels + kDsWidth * kDsHeight);
            slot.cinematic = true;
            slot.cinematic_is_cpu = true;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        slot.fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        glFlush();
        slot.is_gl = true;
        slot.scale = scale;
    } else if (input.top && input.bottom) {
        const size_t plane = size_t(kDsWidth) * size_t(kDsHeight);
        slot.cpu_pixels.resize(plane * 3);
        std::memcpy(slot.cpu_pixels.data(), input.top, plane * sizeof(uint32_t));
        std::memcpy(slot.cpu_pixels.data() + plane, input.bottom, plane * sizeof(uint32_t));
        if (input.cinematic_pixels)
            std::memcpy(slot.cpu_pixels.data() + 2 * plane, input.cinematic_pixels, plane * sizeof(uint32_t));
        slot.is_gl = false;
        slot.scale = 1;
        slot.cinematic = input.cinematic_pixels != nullptr;
        slot.cinematic_is_cpu = slot.cinematic;
    } else {
        return false;
    }

    slot.shared_scene = input.shared_scene;
    slot.menu = input.menu;
    slot.frame_number = input.frame_number;
    slot.published_ns = input.published_ns;

    std::lock_guard lock(state_->frame_mutex);
    const bool wake = !state_->frame_fresh;
    std::swap(state_->write_idx, state_->ready_idx);
    state_->frame_fresh = true;
    return wake;
}

bool LauncherWindow::present_published_game(bool touch_captured, st::PresentedFrameInfo& info, bool render,
                                            const st::SoftwareFrameOverlay& overlay) {
    info = {};
    {
        std::lock_guard lock(state_->frame_mutex);
        if (!state_->frame_fresh) return true;
        std::swap(state_->read_idx, state_->ready_idx);
        state_->frame_fresh = false;
    }
    if (SDL_GL_GetCurrentContext() != state_->gl && !SDL_GL_MakeCurrent(window(), state_->gl)) return false;
    if (!state_->ensure_present_pipeline()) return false;

    auto& slot = state_->slots[state_->read_idx];
    if (slot.fence) {
        glWaitSync(slot.fence, 0, GL_TIMEOUT_IGNORED);
        glDeleteSync(slot.fence);
        slot.fence = nullptr;
    }
    if (!touch_captured && state_->shared_scene != slot.shared_scene) {
        state_->shared_scene = slot.shared_scene;
        if (state_->automatic_join) state_->classic_override = false;
    }
    state_->cinematic = slot.cinematic;

    if (slot.is_gl) {
        state_->active_screens_tex = slot.gl_screens_tex;
        state_->active_scale = slot.scale;
        if (slot.cinematic && !slot.cinematic_is_cpu) {
            state_->active_cinematic_tex = slot.gl_cinematic_tex;
            state_->active_cinematic_scale = slot.scale;
        } else if (slot.cinematic && slot.cinematic_is_cpu) {
            glActiveTexture(GL_TEXTURE0);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
            glBindTexture(GL_TEXTURE_2D, state_->soft_cinematic_tex);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kDsWidth, kDsHeight, GL_BGRA, GL_UNSIGNED_BYTE,
                            slot.cpu_cinematic_pixels.data());
            state_->active_cinematic_tex = state_->soft_cinematic_tex;
            state_->active_cinematic_scale = 1;
        } else {
            state_->active_cinematic_tex = 0;
        }
    } else {
        const size_t plane = size_t(kDsWidth) * size_t(kDsHeight);
        auto* bottom = slot.cpu_pixels.data() + plane;
        auto* cinematic = slot.cinematic ? bottom + plane : nullptr;
        if (overlay) overlay(bottom, cinematic, slot.menu);
        state_->upload_soft_planes(slot.cpu_pixels.data(), bottom, cinematic);
    }

    info.drew = true;
    info.shared_scene = slot.shared_scene;
    info.cinematic = slot.cinematic;
    info.frame_number = slot.frame_number;
    info.published_ns = slot.published_ns;
    if (!render) return true;
    state_->system->set_game_mode(true);
    if (!draw_game()) return false;
    draw_fullscreen_cursor(window());
    return SDL_GL_SwapWindow(window());
}

bool LauncherWindow::process_events(Rml::Context& context, bool* audio_devices_changed) {
    if (audio_devices_changed) *audio_devices_changed = false;
    SDL_Event event;
    auto next_event = [&](bool wait) {
        state_->pumping_context = &context;
        const bool ready = wait ? SDL_WaitEventTimeout(&event, 16) : SDL_PollEvent(&event);
        state_->pumping_context = nullptr;
        return ready;
    };
    bool open = true;
    for (bool queued = next_event(true); queued; queued = next_event(false)) {
        if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) open = false;
        else {
            observe_input(event);
            if (state_->input_filter && state_->input_filter(event)) continue;
            if (event.type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED) update_minimum_size(window());
            if (audio_devices_changed &&
                (event.type == SDL_EVENT_AUDIO_DEVICE_ADDED || event.type == SDL_EVENT_AUDIO_DEVICE_REMOVED))
                *audio_devices_changed = true;
            RmlSDL::InputEventHandler(&context, window(), event);
        }
    }
    return open;
}

bool LauncherWindow::draw(Rml::Context& context, const char* preview_path) {
    state_->system->set_game_mode(false);
    SDL_ShowCursor();
    if (SDL_GL_GetCurrentContext() != state_->gl && !SDL_GL_MakeCurrent(window(), state_->gl)) return false;
    int width, height;
    if (!SDL_GetWindowSizeInPixels(window(), &width, &height) || width <= 0 || height <= 0) return false;
    context.SetDimensions({width, height});
    context.SetDensityIndependentPixelRatio(SDL_GetWindowDisplayScale(window()));
    context.Update();
    auto& renderer = *state_->renderer;
    renderer.SetViewport(width, height);
    renderer.BeginFrame();
    renderer.Clear();
    context.Render();
    renderer.EndFrame();
    const bool saved = !preview_path || save_preview(width, height, preview_path);
    return SDL_GL_SwapWindow(window()) && saved;
}

bool LauncherWindow::update_game_frame(const uint32_t* pixels, bool shared_scene, bool touch_captured, bool cinematic) {
    if (SDL_GL_GetCurrentContext() != state_->gl && !SDL_GL_MakeCurrent(window(), state_->gl)) return false;
    if (!state_->ensure_present_pipeline()) return false;
    if (!touch_captured && state_->shared_scene != shared_scene) {
        state_->shared_scene = shared_scene;
        if (state_->automatic_join) state_->classic_override = false;
    }
    state_->cinematic = cinematic;
    const size_t plane = size_t(kDsWidth) * size_t(kDsHeight);
    state_->upload_soft_planes(pixels, pixels + plane, cinematic ? pixels + 2 * plane : nullptr);
    return true;
}

bool LauncherWindow::present_game(const uint32_t* pixels, bool shared_scene, bool touch_captured, bool cinematic) {
    state_->system->set_game_mode(true);
    return update_game_frame(pixels, shared_scene, touch_captured, cinematic) && draw_game() &&
           SDL_GL_SwapWindow(window());
}

bool LauncherWindow::draw_game() {
    if (SDL_GL_GetCurrentContext() != state_->gl && !SDL_GL_MakeCurrent(window(), state_->gl)) return false;
    int width, height;
    if (!SDL_GetWindowSizeInPixels(window(), &width, &height)) return false;
    if (width <= 0 || height <= 0) return true;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glViewport(0, 0, width, height);
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    if (!state_->active_screens_tex) return true;

    auto clip_rect = [width, height](const st::ScreenRect& view, float& x0, float& y0, float& x1, float& y1) {
        x0 = 2.f * view.x / width - 1.f;
        x1 = 2.f * (view.x + view.width) / width - 1.f;
        y0 = 1.f - 2.f * view.y / height;
        y1 = 1.f - 2.f * (view.y + view.height) / height;
    };

    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(state_->quad_vao);

    if (state_->show_cinematic() && !state_->secondary_peek) {
        const auto view = st::cinematic_view(width, height);
        if (view.width > 0 && view.height > 0) {
            glBindTexture(GL_TEXTURE_2D, state_->active_cinematic_tex);
            const bool hires = state_->active_cinematic_scale > 1;
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, hires ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, hires ? GL_LINEAR : GL_NEAREST);
            float x0, y0, x1, y1;
            clip_rect(view, x0, y0, x1, y1);
            glUseProgram(state_->tex2d_prog);
            glUniform1i(state_->u_2d_tex, 0);
            glUniform4f(state_->u_2d_rect, x0, y0, x1, y1);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }
        glUseProgram(0);
        glBindVertexArray(0);
        return true;
    }

    const auto views = state_->views(width, height);
    glBindTexture(GL_TEXTURE_2D_ARRAY, state_->active_screens_tex);
    const bool hires = state_->active_scale > 1;
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, hires ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, hires ? GL_LINEAR : GL_NEAREST);
    glUseProgram(state_->array_prog);
    glUniform1i(state_->u_array_tex, 0);
    auto draw_view = [&](const st::ScreenRect& view, float layer, float opacity) {
        if (view.width <= 0 || view.height <= 0) return;
        float x0, y0, x1, y1;
        clip_rect(view, x0, y0, x1, y1);
        glUniform4f(state_->u_array_rect, x0, y0, x1, y1);
        glUniform1f(state_->u_array_layer, layer);
        glUniform1f(state_->u_array_opacity, opacity);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    };
    draw_view(views.touch, 1.f, 1.f);
    const bool overlay = state_->layout == st::ScreenLayout::single && !state_->secondary_peek &&
        !state_->classic_override && !(state_->automatic_join && state_->shared_scene);
    if (overlay) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    draw_view(views.top, 0.f, overlay ? state_->secondary.opacity / 100.f : 1.f);
    glDisable(GL_BLEND);
    glUseProgram(0);
    glBindVertexArray(0);
    return true;
}

int LauncherWindow::active_frame_scale() const { return state_->active_scale; }

bool LauncherWindow::verify_rendered_views(int expected_scale) {
    if (state_->active_scale != expected_scale) return false;
    int width, height;
    if (!SDL_GetWindowSizeInPixels(window(), &width, &height) || width <= 0 || height <= 0) return false;
    if (!draw_game()) return false;

    std::vector<uint32_t> pixels(size_t(width) * size_t(height));
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    auto sample_at = [&](int x, int y) -> uint32_t {
        const int gl_y = height - 1 - y;
        return pixels[size_t(gl_y) * size_t(width) + size_t(x)] & 0x00ffffffu;
    };
    auto rect_has_content = [&](const st::ScreenRect& r, const st::ScreenRect* exclude) {
        if (r.width <= 4 || r.height <= 4) return false;
        const int step_x = std::max(1, r.width / 16);
        const int step_y = std::max(1, r.height / 16);
        for (int y = r.y + 2; y < r.y + r.height - 2; y += step_y) {
            for (int x = r.x + 2; x < r.x + r.width - 2; x += step_x) {
                if (exclude && exclude->contains(float(x), float(y))) continue;
                if (sample_at(x, y) != 0) return true;
            }
        }
        return false;
    };

    if (state_->show_cinematic() && !state_->secondary_peek) {
        const auto view = st::cinematic_view(width, height);
        if (!rect_has_content(view, nullptr)) return false;
        if (view.x >= 4 && sample_at(view.x / 2, height / 2) != 0) return false;
        if (view.y >= 4 && sample_at(width / 2, view.y / 2) != 0) return false;
        return true;
    }

    const auto views = state_->views(width, height);
    if (views.top.width && !rect_has_content(views.top, nullptr)) return false;
    if (views.touch.width && !rect_has_content(views.touch, &views.top)) return false;
    for (const auto [mx, my] : {std::pair{1, 1}, std::pair{width - 2, height - 2}}) {
        if (!views.top.contains(float(mx), float(my)) && !views.touch.contains(float(mx), float(my))) {
            if (sample_at(mx, my) != 0) return false;
        }
    }
    return true;
}

void LauncherWindow::menu_input(Rml::Context& context, SDL_Event& event) {
    RmlSDL::InputEventHandler(&context, window(), event);
}

bool LauncherWindow::draw_pause(Rml::Context& context, const char* preview_path) {
    state_->system->set_game_mode(false);
    SDL_ShowCursor();
    int width, height;
    if (!SDL_GetWindowSizeInPixels(window(), &width, &height) || width <= 0 || height <= 0) return true;
    if (!draw_game()) return false;
    context.SetDimensions({width, height});
    context.SetDensityIndependentPixelRatio(SDL_GetWindowDisplayScale(window()));
    context.Update();
    auto& renderer = *state_->renderer;
    renderer.SetViewport(width, height);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    renderer.BeginFrame();
    context.Render();
    renderer.EndFrame();
    if (preview_path && !save_preview(width,height,preview_path)) return false;
    return SDL_GL_SwapWindow(window());
}

bool LauncherWindow::game_coordinates(float& x, float& y, bool captured) const {
    int width, height, pixels_w, pixels_h;
    if (!SDL_GetWindowSize(window(), &width, &height) || width <= 0 || height <= 0 ||
        !SDL_GetWindowSizeInPixels(window(), &pixels_w, &pixels_h)) return false;
    x *= float(pixels_w) / width;
    y *= float(pixels_h) / height;
    if (state_->show_cinematic() && !state_->secondary_peek) {
        return st::to_touch({{}, st::cinematic_view(pixels_w, pixels_h)}, x, y, captured);
    }
    return st::to_touch(state_->views(pixels_w, pixels_h), x, y, captured);
}

void LauncherWindow::set_screen_layout(st::ScreenLayout layout, bool automatic_join, st::ScreenEnhancements enhancements) {
    state_->layout = layout;
    state_->automatic_join = automatic_join;
    state_->enhancements = enhancements;
    state_->classic_override = false;
    state_->secondary_peek = false;
}

void LauncherWindow::set_secondary_view(st::SecondaryViewSettings settings) {
    if (!st::valid_secondary_size(settings.size)) settings.size = 0;
    if (!st::valid_secondary_opacity(settings.opacity)) settings.opacity = 75;
    state_->secondary = settings;
}

void LauncherWindow::peek_secondary(bool held) { state_->secondary_peek = held; }

bool LauncherWindow::save_game_preview(const char* path) {
    int width = 0, height = 0;
    return draw_game() && SDL_GetWindowSizeInPixels(window(), &width, &height) && save_preview(width, height, path);
}

void LauncherWindow::toggle_classic_layout() {
    state_->secondary_peek = false;
    state_->classic_override = !state_->classic_override;
}

void LauncherWindow::set_internal_scale(int scale) {
    state_->internal_scale.store(st::normalize_internal_scale(scale));
}

int LauncherWindow::internal_scale() const {
    return state_->internal_scale.load();
}

double LauncherWindow::cinematic_aspect() const {
    if (state_->automatic_join == state_->classic_override) return 0;
    int width, height;
    if (!SDL_GetWindowSizeInPixels(window(), &width, &height) || width <= 0 || height <= 0) return 0;
    const auto view = st::cinematic_view(width, height);
    return view.width > 0 && view.height > 0 ? double(view.width) / view.height : 0;
}

uint32_t LauncherWindow::screen_features() const { return state_->enhancements.mask(); }

uint64_t LauncherWindow::audio_revision() const { return state_->audio_revision.load(); }

st::AudioSettings LauncherWindow::audio_settings() const {
    std::lock_guard lock(state_->audio_mutex);
    return state_->audio;
}

void LauncherWindow::set_audio_settings(const st::AudioSettings& audio) {
    {
        std::lock_guard lock(state_->audio_mutex);
        if (state_->audio == audio) return;
        state_->audio = audio;
    }
    state_->audio_revision.fetch_add(1);
}

bool LauncherWindow::pause_on_menu() const { return state_->pause_on_menu.load(); }

void LauncherWindow::set_pause_on_menu(bool pause) { state_->pause_on_menu.store(pause); }
