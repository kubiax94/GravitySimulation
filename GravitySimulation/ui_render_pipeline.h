#pragma once

#include <functional>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "ui_layout.h"

class engine;
class scene;
class shader;
class Mesh;
class ui_text_renderer;
class i_transformable;

class ui_render_pipeline
{
public:
    struct panel_desc {
        glm::vec2 top_left = glm::vec2(0.0f);
        glm::vec2 size = glm::vec2(0.0f);
        glm::vec4 color = glm::vec4(0.08f, 0.10f, 0.14f, 0.82f);
        ui_spacing padding = ui_spacing(0.0f);
        const i_transformable* anchor = nullptr;
        glm::vec2 anchor_offset = glm::vec2(0.0f);
        bool clip_children = false;
    };

    struct label_desc {
        std::string text;
        glm::vec2 top_left = glm::vec2(0.0f);
        float scale = 1.0f;
        glm::vec3 color = glm::vec3(1.0f);
        glm::vec2 bounds_size = glm::vec2(0.0f);
        ui_horizontal_alignment horizontal_alignment = ui_horizontal_alignment::left;
        ui_vertical_alignment vertical_alignment = ui_vertical_alignment::top;
        const i_transformable* anchor = nullptr;
        glm::vec2 anchor_offset = glm::vec2(0.0f);
    };

    struct button_desc {
        std::string id;
        std::string text;
        glm::vec2 top_left = glm::vec2(0.0f);
        glm::vec2 size = glm::vec2(0.0f);
        glm::vec4 color = glm::vec4(0.16f, 0.19f, 0.26f, 0.92f);
        glm::vec4 hover_color = glm::vec4(0.23f, 0.28f, 0.38f, 0.96f);
        glm::vec3 text_color = glm::vec3(0.96f, 0.98f, 1.0f);
        ui_spacing padding = ui_spacing(12.0f, 10.0f);
        const i_transformable* anchor = nullptr;
        glm::vec2 anchor_offset = glm::vec2(0.0f);
        std::function<void()> on_click;
    };

    struct metric_row_desc {
        std::string label;
        std::string value;
        glm::vec2 top_left = glm::vec2(0.0f);
        glm::vec2 size = glm::vec2(0.0f);
        float scale = 1.0f;
        glm::vec3 label_color = glm::vec3(1.0f);
        glm::vec3 value_color = glm::vec3(1.0f);
        float column_gap = 12.0f;
    };

    struct vertical_layout {
        glm::vec2 top_left = glm::vec2(0.0f);
        glm::vec2 size = glm::vec2(0.0f);
        ui_spacing padding = ui_spacing(0.0f);
        float spacing = 0.0f;
        glm::vec2 cursor = glm::vec2(0.0f);
    };

private:
    shader* color_shader_ = nullptr;
    Mesh* quad_mesh_ = nullptr;
    ui_text_renderer* text_renderer_ = nullptr;
    bool initialized_ = false;
    bool previous_left_mouse_down_ = false;
    bool current_left_mouse_down_ = false;
    const scene* scene_context_ = nullptr;

    bool ensure_resources(scene& scene_context);
    bool is_point_inside(const glm::vec2& point, const glm::vec2& top_left, const glm::vec2& size) const;
    glm::vec2 resolve_top_left(engine& engine, const glm::vec2& top_left, const i_transformable* anchor, const glm::vec2& anchor_offset) const;

public:
    bool initialize(scene& scene_context, ui_text_renderer& text_renderer);
    void shutdown();
    bool is_ready() const;

    void begin_frame(const scene* scene_context = nullptr);
    void end_frame();
    void draw_panel(engine& engine, const panel_desc& panel) const;
    void begin_clip_rect(engine& engine, const glm::vec2& top_left, const glm::vec2& size) const;
    void end_clip_rect() const;
    void draw_label(engine& engine, const label_desc& label) const;
    void draw_button(engine& engine, const button_desc& button);
    void draw_metric_row(engine& engine, const metric_row_desc& row) const;
    void draw_panel_with_children(engine& engine, const panel_desc& panel, const std::function<void()>& content_draw);
    vertical_layout begin_vertical_layout(const panel_desc& panel, float spacing = 0.0f) const;
    glm::vec2 push_layout_item(vertical_layout& layout, float height, const ui_spacing& margin = ui_spacing(0.0f));
    float get_layout_content_width(const vertical_layout& layout) const;
};
