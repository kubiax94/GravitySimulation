#include "ui_text_renderer.h"

#include <array>
#include <filesystem>
#include <iostream>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/mat4x4.hpp>

#include "Shader.h"
#include "asset_manager.h"
#include "engine.h"
#include "font_resource.h"
#include "Scene.h"

namespace {
const char* text_vertex_shader_path = "GravitySimulation/ui_text.vs.shader";
const char* text_fragment_shader_path = "GravitySimulation/ui_text.fs.shader";
}

ui_text_renderer::~ui_text_renderer() {
    shutdown();
}

std::string ui_text_renderer::resolve_default_font_path() {
    static constexpr const char* candidate_paths[] = {
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/calibri.ttf"
    };

    for (const char* candidate : candidate_paths) {
        if (std::filesystem::exists(candidate))
            return candidate;
    }

    return {};
}

bool ui_text_renderer::ensure_gpu_objects() {
    if (vao_ != 0 && vbo_ != 0)
        return true;

    if (vao_ == 0)
        glGenVertexArrays(1, &vao_);
    if (vbo_ == 0)
        glGenBuffers(1, &vbo_);

    if (vao_ == 0 || vbo_ == 0)
        return false;

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 4, nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    return true;
}

bool ui_text_renderer::initialize(scene& scene_context) {
    if (initialized_)
        return is_ready();

    auto& assets = scene_context.get_asset_manager();
    text_shader_ = assets.create_shader("ui.text", text_vertex_shader_path, text_fragment_shader_path);
    if (!text_shader_) {
        std::cerr << "ERROR::UI_TEXT::SHADER_LOAD_FAILED" << std::endl;
        return false;
    }

    const std::string font_path = resolve_default_font_path();
    if (font_path.empty()) {
        std::cerr << "ERROR::UI_TEXT::DEFAULT_FONT_NOT_FOUND" << std::endl;
        return false;
    }

    default_font_ = assets.create_font_resource("ui.default_font", font_path, 32u, 32u, 126u);
    if (!default_font_) {
        std::cerr << "ERROR::UI_TEXT::FONT_LOAD_FAILED: " << font_path << std::endl;
        return false;
    }

    if (!ensure_gpu_objects()) {
        std::cerr << "ERROR::UI_TEXT::GPU_INIT_FAILED" << std::endl;
        return false;
    }

    initialized_ = true;
    return true;
}

void ui_text_renderer::shutdown() {
    if (vbo_ != 0) {
        glDeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
    if (vao_ != 0) {
        glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }

    text_shader_ = nullptr;
    default_font_ = nullptr;
    initialized_ = false;
}

bool ui_text_renderer::is_ready() const {
    return initialized_
        && text_shader_
        && text_shader_->is_vaild()
        && default_font_
        && default_font_->is_vaild()
        && vao_ != 0
        && vbo_ != 0;
}

ui_text_renderer::text_bounds ui_text_renderer::measure_text(const std::string& text, float scale) const {
    text_bounds bounds;
    if (!default_font_ || text.empty())
        return bounds;

    const float line_advance = static_cast<float>(std::max(default_font_->get_line_height(), static_cast<int>(default_font_->get_pixel_height()))) * scale;
    float current_line_width = 0.0f;
    float max_line_width = 0.0f;
    int line_count = 1;

    for (const unsigned char ch : text) {
        if (ch == '\n') {
            max_line_width = std::max(max_line_width, current_line_width);
            current_line_width = 0.0f;
            ++line_count;
            continue;
        }

        const auto* glyph = default_font_->find_glyph(static_cast<std::uint32_t>(ch));
        if (!glyph || !glyph->valid) {
            current_line_width += static_cast<float>(default_font_->get_pixel_height()) * scale * 0.25f;
            continue;
        }

        current_line_width += static_cast<float>(glyph->advance) * scale;
    }

    max_line_width = std::max(max_line_width, current_line_width);
    bounds.max_line_width = max_line_width;
    bounds.line_height = line_advance;
    bounds.line_count = line_count;
    bounds.size = glm::vec2(max_line_width, static_cast<float>(line_count) * line_advance);
    return bounds;
}

void ui_text_renderer::render_text(engine& engine, const std::string& text, const glm::vec2& top_left, float scale, const glm::vec3& color) {
    if (!is_ready() || text.empty())
        return;

    GLFWwindow* window = engine.get_window();
    if (!window)
        return;

    int framebuffer_width = 0;
    int framebuffer_height = 0;
    glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
    if (framebuffer_width <= 0 || framebuffer_height <= 0)
        return;

    const glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(framebuffer_width), 0.0f, static_cast<float>(framebuffer_height));
    float pen_x = top_left.x;
    float baseline_y = static_cast<float>(framebuffer_height) - top_left.y - static_cast<float>(default_font_->get_ascender()) * scale;
    const float line_advance = static_cast<float>(std::max(default_font_->get_line_height(), static_cast<int>(default_font_->get_pixel_height()))) * scale;

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    text_shader_->use();
    text_shader_->set_uniform_mat4("projection", projection);
    text_shader_->set_uni_vec3("textColor", color);
    text_shader_->set_uni_int("textAtlas", 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, default_font_->get_atlas_texture().get_id());
    glBindVertexArray(vao_);

    for (const unsigned char ch : text) {
        if (ch == '\n') {
            pen_x = top_left.x;
            baseline_y -= line_advance;
            continue;
        }

        const auto* glyph = default_font_->find_glyph(static_cast<std::uint32_t>(ch));
        if (!glyph || !glyph->valid) {
            pen_x += static_cast<float>(default_font_->get_pixel_height()) * scale * 0.25f;
            continue;
        }

        const float xpos = pen_x + static_cast<float>(glyph->bearing.x) * scale;
        const float ypos = baseline_y - static_cast<float>(glyph->size.y - glyph->bearing.y) * scale;
        const float width = static_cast<float>(glyph->size.x) * scale;
        const float height = static_cast<float>(glyph->size.y) * scale;

        const std::array<float, 24> vertices = {
            xpos, ypos + height, glyph->uv_min.x, glyph->uv_min.y,
            xpos, ypos, glyph->uv_min.x, glyph->uv_max.y,
            xpos + width, ypos, glyph->uv_max.x, glyph->uv_max.y,
            xpos, ypos + height, glyph->uv_min.x, glyph->uv_min.y,
            xpos + width, ypos, glyph->uv_max.x, glyph->uv_max.y,
            xpos + width, ypos + height, glyph->uv_max.x, glyph->uv_min.y
        };

        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices.data());
        glDrawArrays(GL_TRIANGLES, 0, 6);

        pen_x += static_cast<float>(glyph->advance) * scale;
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}
