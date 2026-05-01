#include "ui_render_pipeline.h"

#include <algorithm>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_projection.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>

#include "Camera.h"
#include "Mesh.h"
#include "Scene.h"
#include "Shader.h"
#include "Transform.h"
#include "engine.h"
#include "input_system.h"
#include "ui_text_renderer.h"

namespace {
const char* ui_color_vertex_shader_path = "GravitySimulation/ui_panel.vs.shader";
const char* ui_color_fragment_shader_path = "GravitySimulation/ui_panel.fs.shader";

MeshData create_ui_quad_mesh() {
    MeshData data;
    Vertex v0{};
    v0.Position = glm::vec3(0.f, 0.f, 0.f);
    v0.Normal = glm::vec3(0.f, 0.f, 1.f);
    v0.TextCoords = glm::vec2(0.f, 0.f);
    Vertex v1{};
    v1.Position = glm::vec3(1.f, 0.f, 0.f);
    v1.Normal = glm::vec3(0.f, 0.f, 1.f);
    v1.TextCoords = glm::vec2(1.f, 0.f);
    Vertex v2{};
    v2.Position = glm::vec3(1.f, 1.f, 0.f);
    v2.Normal = glm::vec3(0.f, 0.f, 1.f);
    v2.TextCoords = glm::vec2(1.f, 1.f);
    Vertex v3{};
    v3.Position = glm::vec3(0.f, 1.f, 0.f);
    v3.Normal = glm::vec3(0.f, 0.f, 1.f);
    v3.TextCoords = glm::vec2(0.f, 1.f);
    data.vertecies = { v0, v1, v2, v3 };
    data.indices = { 0u, 1u, 2u, 0u, 2u, 3u };
    return data;
}
}

bool ui_render_pipeline::ensure_resources(scene& scene_context) {
    auto& assets = scene_context.get_asset_manager();
    if (!color_shader_) {
        color_shader_ = assets.create_shader("ui.panel", ui_color_vertex_shader_path, ui_color_fragment_shader_path);
        if (!color_shader_)
            return false;
    }

    if (!quad_mesh_) {
        static MeshData quad_mesh_data = create_ui_quad_mesh();
        quad_mesh_ = assets.create_mesh(quad_mesh_data);
        if (!quad_mesh_)
            return false;
    }

    return true;
}

bool ui_render_pipeline::initialize(scene& scene_context, ui_text_renderer& text_renderer) {
    scene_context_ = &scene_context;
    text_renderer_ = &text_renderer;
    initialized_ = ensure_resources(scene_context) && text_renderer_ != nullptr;
    return is_ready();
}

void ui_render_pipeline::shutdown() {
    color_shader_ = nullptr;
    quad_mesh_ = nullptr;
    text_renderer_ = nullptr;
    initialized_ = false;
    previous_left_mouse_down_ = false;
    current_left_mouse_down_ = false;
    scene_context_ = nullptr;
}

bool ui_render_pipeline::is_ready() const {
    return initialized_
        && color_shader_
        && color_shader_->is_vaild()
        && quad_mesh_
        && quad_mesh_->is_vaild()
        && text_renderer_
        && text_renderer_->is_ready();
}

void ui_render_pipeline::begin_frame(const scene* scene_context) {
    scene_context_ = scene_context;
    current_left_mouse_down_ = input_system::is_button_down(GLFW_MOUSE_BUTTON_LEFT);
}

void ui_render_pipeline::end_frame() {
    previous_left_mouse_down_ = current_left_mouse_down_;
}

bool ui_render_pipeline::is_point_inside(const glm::vec2& point, const glm::vec2& top_left, const glm::vec2& size) const {
    return point.x >= top_left.x
        && point.y >= top_left.y
        && point.x <= top_left.x + size.x
        && point.y <= top_left.y + size.y;
}

glm::vec2 ui_render_pipeline::resolve_top_left(engine& engine, const glm::vec2& top_left, const i_transformable* anchor, const glm::vec2& anchor_offset) const {
    if (!anchor)
        return top_left;

    GLFWwindow* window = engine.get_window();
    if (!window)
        return top_left + anchor_offset;

    auto* camera = scene_context_ ? scene_context_->get_main_camera() : nullptr;
    if (!camera)
        return top_left + anchor_offset;

    int framebuffer_width = 0;
    int framebuffer_height = 0;
    glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
    if (framebuffer_width <= 0 || framebuffer_height <= 0)
        return top_left + anchor_offset;

    const float aspect = static_cast<float>(framebuffer_width) / static_cast<float>(framebuffer_height);
    const glm::mat4 view = camera->GetViewMatrix();
    const glm::mat4 projection = camera->GetProjectionMatrix(aspect);
    const glm::vec3 screen = glm::project(anchor->get_global_position(), view, projection, glm::vec4(0.0f, 0.0f, static_cast<float>(framebuffer_width), static_cast<float>(framebuffer_height)));
    return glm::vec2(screen.x, static_cast<float>(framebuffer_height) - screen.y) + top_left + anchor_offset;
}

void ui_render_pipeline::draw_panel(engine& engine, const panel_desc& panel) const {
    if (!is_ready())
        return;

    GLFWwindow* window = engine.get_window();
    if (!window)
        return;

    int framebuffer_width = 0;
    int framebuffer_height = 0;
    glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
    if (framebuffer_width <= 0 || framebuffer_height <= 0)
        return;

    const glm::vec2 panel_top_left = resolve_top_left(engine, panel.top_left, panel.anchor, panel.anchor_offset);
    const glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(framebuffer_width), 0.0f, static_cast<float>(framebuffer_height));
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(panel_top_left.x, static_cast<float>(framebuffer_height) - panel_top_left.y - panel.size.y, 0.0f));
    model = glm::scale(model, glm::vec3(panel.size, 1.0f));

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    color_shader_->use();
    color_shader_->set_uniform_mat4("projection", projection);
    color_shader_->set_uniform_mat4("model", model);
    color_shader_->set_uni_vec4("uiColor", panel.color);
    quad_mesh_->Draw();

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

void ui_render_pipeline::draw_label(engine& engine, const label_desc& label) const {
    if (!is_ready())
        return;

    glm::vec2 draw_position = resolve_top_left(engine, label.top_left, label.anchor, label.anchor_offset);
    if (label.horizontal_alignment != ui_horizontal_alignment::left && label.bounds_size.x > 0.0f) {
        const auto bounds = text_renderer_->measure_text(label.text, label.scale);
        if (label.horizontal_alignment == ui_horizontal_alignment::center)
            draw_position.x += std::max((label.bounds_size.x - bounds.max_line_width) * 0.5f, 0.0f);
        else if (label.horizontal_alignment == ui_horizontal_alignment::right)
            draw_position.x += std::max(label.bounds_size.x - bounds.max_line_width, 0.0f);
    }

    text_renderer_->render_text(engine, label.text, draw_position, label.scale, label.color);
}

void ui_render_pipeline::draw_button(engine& engine, const button_desc& button) {
    if (!is_ready())
        return;

    const glm::vec3 mouse = input_system::get_mouse_pos();
    const glm::vec2 mouse_pos(mouse.x, mouse.y);
    const glm::vec2 button_top_left = resolve_top_left(engine, button.top_left, button.anchor, button.anchor_offset);
    const bool hovered = is_point_inside(mouse_pos, button_top_left, button.size);
    const bool clicked = hovered && current_left_mouse_down_ && !previous_left_mouse_down_;

    draw_panel(engine, panel_desc{ button_top_left, button.size, hovered ? button.hover_color : button.color });
    draw_label(engine, label_desc{
        button.text,
        button_top_left + glm::vec2(button.padding.left, button.padding.top),
        0.72f,
        button.text_color,
        button.size - glm::vec2(button.padding.left + button.padding.right, 0.0f),
        ui_horizontal_alignment::center,
        nullptr,
        glm::vec2(0.0f)
    });

    if (clicked && button.on_click)
        button.on_click();
}

void ui_render_pipeline::draw_metric_row(engine& engine, const metric_row_desc& row) const {
    if (!is_ready())
        return;

    const float label_width = std::max(row.size.x * 0.56f, 0.0f);
    const float value_width = std::max(row.size.x - label_width - row.column_gap, 0.0f);

    draw_label(engine, {
        row.label,
        row.top_left,
        row.scale,
        row.label_color,
        glm::vec2(label_width, row.size.y),
        ui_horizontal_alignment::left
    });

    draw_label(engine, {
        row.value,
        row.top_left + glm::vec2(label_width + row.column_gap, 0.0f),
        row.scale,
        row.value_color,
        glm::vec2(value_width, row.size.y),
        ui_horizontal_alignment::right
    });
}

void ui_render_pipeline::draw_panel_with_children(engine& engine, const panel_desc& panel, const std::function<void()>& content_draw) {
    draw_panel(engine, panel);
    if (content_draw)
        content_draw();
}

ui_render_pipeline::vertical_layout ui_render_pipeline::begin_vertical_layout(const panel_desc& panel, float spacing) const {
    vertical_layout layout;
    layout.top_left = panel.top_left;
    layout.size = panel.size;
    layout.padding = panel.padding;
    layout.spacing = spacing;
    layout.cursor = panel.top_left + glm::vec2(panel.padding.left, panel.padding.top);
    return layout;
}

glm::vec2 ui_render_pipeline::push_layout_item(vertical_layout& layout, float height, const ui_spacing& margin) {
    const glm::vec2 position = glm::vec2(layout.cursor.x + margin.left, layout.cursor.y + margin.top);
    layout.cursor.y += margin.top + height + margin.bottom + layout.spacing;
    return position;
}

float ui_render_pipeline::get_layout_content_width(const vertical_layout& layout) const {
    return std::max(layout.size.x - layout.padding.left - layout.padding.right, 0.0f);
}
