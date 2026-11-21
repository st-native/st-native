// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace st {

// Offscreen OpenGL context and framebuffer readback for CLI / headless runs with internal_scale > 1.
// Keeps all OpenGL calls inside app/present/ (AGENTS.md).
class OffscreenGlContext {
public:
    OffscreenGlContext();
    ~OffscreenGlContext();
    OffscreenGlContext(const OffscreenGlContext&) = delete;
    OffscreenGlContext& operator=(const OffscreenGlContext&) = delete;

    bool open();
    bool read_screens_bgra(unsigned gl_array_tex, int scale,
                           std::vector<uint32_t>& top_out, std::vector<uint32_t>& bottom_out);
    bool read_texture_bgra(unsigned gl_tex_2d, int width, int height, std::vector<uint32_t>& out);

private:
    struct State;
    std::unique_ptr<State> state_;
};

}  // namespace st
