// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// GL implementation detail of LauncherWindow, behind GamePresentation. No GL types escape to layout/settings/UI.
#include <frontend/glad/glad.h>
#include <SDL3/SDL.h>
#include "screen_layout.hpp"

namespace st {

class GlScreenCompositor {
public:
    ~GlScreenCompositor() {
        if (vao_) glDeleteVertexArrays(1, &vao_);
        if (program_) glDeleteProgram(program_);
    }

    bool create() {
        const char* vertex = R"(#version 330 core
uniform vec4 rect;
uniform vec2 source;
out vec2 uv;
void main() {
    const vec2 corners[6] = vec2[6](vec2(0,0), vec2(1,0), vec2(0,1),
                                  vec2(0,1), vec2(1,0), vec2(1,1));
    vec2 p = corners[gl_VertexID];
    gl_Position = vec4(rect.xy + p * rect.zw, 0, 1);
    uv = vec2(p.x, source.x + p.y * source.y);
})";
        const char* fragment = R"(#version 330 core
uniform sampler2D image;
uniform float opacity;
in vec2 uv;
out vec4 color;
void main() { color = vec4(texture(image, uv).rgb, opacity); }
)";
        const GLuint vs = compile(GL_VERTEX_SHADER, vertex), fs = compile(GL_FRAGMENT_SHADER, fragment);
        if (!vs || !fs) {
            if (vs) glDeleteShader(vs);
            if (fs) glDeleteShader(fs);
            return false;
        }
        program_ = glCreateProgram();
        glAttachShader(program_, vs); glAttachShader(program_, fs);
        glLinkProgram(program_);
        glDeleteShader(vs); glDeleteShader(fs);
        GLint linked = 0;
        glGetProgramiv(program_, GL_LINK_STATUS, &linked);
        if (!linked) return SDL_SetError("Could not link the screen compositor");
        rect_ = glGetUniformLocation(program_, "rect");
        source_ = glGetUniformLocation(program_, "source");
        opacity_ = glGetUniformLocation(program_, "opacity");
        image_ = glGetUniformLocation(program_, "image");
        glGenVertexArrays(1, &vao_);
        return true;
    }

    void begin(GLuint texture, int width, int height) {
        width_ = width; height_ = height;
        glViewport(0, 0, width, height);
        glDisable(GL_DEPTH_TEST); glDisable(GL_STENCIL_TEST); glDisable(GL_CULL_FACE);
        glUseProgram(program_);
        glBindVertexArray(vao_);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glBindSampler(0, 0);
        glUniform1i(image_, 0);
        glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    }

    void draw(const ScreenRect& rect, int plane, int opacity = 100) {
        if (rect.width <= 0 || rect.height <= 0) return;
        // The uploaded atlas is top-down: invert texture Y while drawing in GL's bottom-up coordinates.
        glUniform4f(rect_, 2.f * rect.x / width_ - 1.f, 1.f - 2.f * (rect.y + rect.height) / height_,
                    2.f * rect.width / width_, 2.f * rect.height / height_);
        glUniform2f(source_, float(plane + 1) / 3.f, -1.f / 3.f);
        glUniform1f(opacity_, opacity / 100.f);
        if (opacity < 100) glEnable(GL_BLEND); else glDisable(GL_BLEND);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    void end() {
        glDisable(GL_BLEND);
        glBindVertexArray(0);
        glUseProgram(0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

private:
    static GLuint compile(GLenum type, const char* code) {
        const GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &code, nullptr);
        glCompileShader(shader);
        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            char message[1024]{};
            glGetShaderInfoLog(shader, sizeof(message), nullptr, message);
            SDL_SetError("Screen compositor shader: %s", message);
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    }
    GLuint program_ = 0, vao_ = 0;
    GLint rect_ = -1, source_ = -1, opacity_ = -1, image_ = -1;
    int width_ = 0, height_ = 0;
};

} // namespace st
