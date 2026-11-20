// SPDX-License-Identifier: GPL-3.0-or-later
#include "launcher_window.hpp"
#include "cursor_interface.hpp"
#include "gl_screen_compositor.hpp"

#include <RmlUi/Core.h>
#include <RmlUi_Platform_SDL.h>
#include <RmlUi_Renderer_GL3.h>
#include <frontend/glad/glad.h>

#include <algorithm>
#include <atomic>
#include <mutex>

namespace {

void update_minimum_size(SDL_Window* window) {
    const float scale = SDL_GetWindowDisplayScale(window);
    SDL_SetWindowMinimumSize(window, int(360 * scale), int(280 * scale));
}

constexpr int kDsWidth = st::ds_width, kTextureHeight = 3 * st::ds_screen_height;

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

}  // namespace

struct LauncherWindow::State {
    std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window{nullptr, SDL_DestroyWindow};
    SDL_GLContext gl = nullptr;
    std::unique_ptr<st::CursorInterface> system;
    std::unique_ptr<RenderInterface_GL3> renderer;
    Rml::Context* pumping_context = nullptr;
    GLuint game_framebuffer = 0;
    GLuint game_texture = 0;
    st::ScreenLayout layout = st::ScreenLayout::focus;
    bool automatic_join = true, shared_scene = false, classic_override = false, cinematic = false;
    st::ScreenEnhancements enhancements;
    st::SecondaryViewSettings secondary;
    bool secondary_peek = false;
    std::unique_ptr<st::GlScreenCompositor> compositor;
    mutable std::mutex audio_mutex;
    std::atomic<uint64_t> audio_revision{1};
    st::AudioSettings audio;
    std::atomic<bool> pause_on_menu{false};

    bool show_cinematic() const { return cinematic && shared_scene && (automatic_join != classic_override); }

    st::ScreenViews views(int width, int height) const {
        return st::screen_views(st::effective_screen_layout(layout, automatic_join, shared_scene, classic_override),
                                width, height, secondary, secondary_peek);
    }

    bool create_game_target() {
        glGenTextures(1, &game_texture);
        glBindTexture(GL_TEXTURE_2D, game_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kDsWidth, kTextureHeight, 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glGenFramebuffers(1, &game_framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, game_framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, game_texture, 0);
        const bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        if (!complete) {
            glDeleteFramebuffers(1, &game_framebuffer);
            glDeleteTextures(1, &game_texture);
            game_framebuffer = game_texture = 0;
            return SDL_SetError("Could not create the game framebuffer");
        }
        return true;
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
        compositor.reset();
        if (game_framebuffer) glDeleteFramebuffers(1, &game_framebuffer);
        if (game_texture) glDeleteTextures(1, &game_texture);
        renderer.reset();
        system.reset();
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
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    const float scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    state_ = std::make_unique<State>();
    state_->window.reset(SDL_CreateWindow(title, int(width * scale), int(height * scale),
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_RESIZABLE));
    if (!window()) return false;
    update_minimum_size(window());
    state_->gl = SDL_GL_CreateContext(window());
    if (!state_->gl || !gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress))) return false;

    SDL_GL_SetSwapInterval(1);
    state_->system = std::make_unique<st::CursorInterface>(window());
    state_->renderer = std::make_unique<RenderInterface_GL3>();
    if (!*state_->renderer) return SDL_SetError("Could not initialize the RmlUi OpenGL renderer");
    Rml::SetSystemInterface(state_->system.get());
    Rml::SetRenderInterface(state_->renderer.get());
    SDL_AddEventWatch(State::redraw_while_exposed, this);
    return true;
}

SDL_Window* LauncherWindow::window() const { return state_->window.get(); }

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
    if (!touch_captured && state_->shared_scene != shared_scene) {
        state_->shared_scene = shared_scene;
        if (state_->automatic_join) state_->classic_override = false;
    }
    state_->cinematic = cinematic;
    if (!state_->game_framebuffer && !state_->create_game_target()) return false;
    glBindTexture(GL_TEXTURE_2D, state_->game_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kDsWidth, kTextureHeight, GL_BGRA, GL_UNSIGNED_BYTE, pixels);
    return true;
}

bool LauncherWindow::present_game(const uint32_t* pixels, bool shared_scene, bool touch_captured, bool cinematic) {
    state_->system->set_game_mode(true);
    return update_game_frame(pixels, shared_scene, touch_captured, cinematic) && draw_game() &&
           SDL_GL_SwapWindow(window());
}

bool LauncherWindow::draw_game() {
    int width, height;
    if (!SDL_GetWindowSizeInPixels(window(), &width, &height)) return false;
    if (width <= 0 || height <= 0) return true;

    // Keep the launcher's OpenGL context and native swap surface for both views.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    if (!state_->game_framebuffer) return true;
    if (!state_->compositor) {
        state_->compositor = std::make_unique<st::GlScreenCompositor>();
        if (!state_->compositor->create()) { state_->compositor.reset(); return false; }
    }
    auto& compositor = *state_->compositor;
    compositor.begin(state_->game_texture, width, height);
    if (state_->show_cinematic() && !state_->secondary_peek) {
        compositor.draw(st::cinematic_view(width, height), 2);
    } else {
        const auto views = state_->views(width, height);
        compositor.draw(views.touch, 1);
        const bool overlay = state_->layout == st::ScreenLayout::single && !state_->secondary_peek &&
            !state_->classic_override && !(state_->automatic_join && state_->shared_scene);
        compositor.draw(views.top, 0, overlay ? state_->secondary.opacity : 100);
    }
    compositor.end();
    return true;
}

void LauncherWindow::menu_input(Rml::Context& context, SDL_Event& event) {
    RmlSDL::InputEventHandler(&context, window(), event);
}

bool LauncherWindow::draw_pause(Rml::Context& context) {
    state_->system->set_game_mode(false);
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
