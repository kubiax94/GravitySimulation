#pragma once

#include <memory>
#include <string>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

#include "Transform.h"

class engine;
class scene;
class engine_state;
class frame_profiler;
class ui_text_renderer;

#include "frame_profiler.h"
#include "ui_loading_feedback_state.h"
#include "ui_render_pipeline.h"
#include "ui_text_renderer.h"

struct ui_context {
    engine& engine_instance;
    ui_render_pipeline& pipeline;
    ui_text_renderer& text_renderer;
    frame_profiler& profiler;
    const ui_loading_feedback_state* loading_feedback = nullptr;
    scene* scene_context = nullptr;
    engine_state* state_context = nullptr;
};

class ui_widget : public i_transformable
{
    std::string id_;
    bool visible_ = true;
    bool layout_fill_x_ = false;
    bool layout_fill_y_ = false;
    float layout_weight_ = 0.0f;
    const ui_widget* parent_widget_ = nullptr;
    const i_transformable* scene_anchor_ = nullptr;

protected:
    transform transform_;
    mutable glm::mat4 global_model_matrix_ = glm::mat4(1.0f);

    [[nodiscard]] glm::vec2 get_ui_offset() const;
    [[nodiscard]] glm::vec2 get_anchor_offset() const;
    [[nodiscard]] const i_transformable* get_scene_anchor() const;
    [[nodiscard]] const glm::mat4& get_parent_anchor_matrix() const;
    [[nodiscard]] const ui_widget* get_parent_widget() const { return parent_widget_; }
    void set_parent_widget(const ui_widget* parent_widget);
    void invalidate_cached_transform() const;

    friend class ui_container_widget;

public:
    explicit ui_widget(std::string id = {});
    virtual ~ui_widget() = default;

    [[nodiscard]] const std::string& get_id() const { return id_; }
    void set_id(std::string id) { id_ = std::move(id); }

    void set_visible(bool visible) { visible_ = visible; }
    [[nodiscard]] bool is_visible() const { return visible_; }

    void set_layout_fill_x(bool fill) { layout_fill_x_ = fill; }
    [[nodiscard]] bool get_layout_fill_x() const { return layout_fill_x_; }

    void set_layout_fill_y(bool fill) { layout_fill_y_ = fill; }
    [[nodiscard]] bool get_layout_fill_y() const { return layout_fill_y_; }

    void set_layout_weight(float weight) { layout_weight_ = weight; }
    [[nodiscard]] float get_layout_weight() const { return layout_weight_; }

    void attach_to_scene_anchor(const i_transformable* anchor);
    void detach_scene_anchor();

    virtual void render(ui_context& context) = 0;

    glm::vec3 forward() const override;
    const glm::vec3& forward_local() const override;
    glm::vec3 right() const override;
    const glm::vec3& right_local() const override;
    glm::vec3 up() const override;
    const glm::vec3& up_local() const override;
    const glm::vec3& get_position() const override;
    glm::vec3 get_global_position() const override;
    const glm::vec3& get_rotation() const override;
    glm::vec3 get_global_rotation() override;
    const glm::vec3& get_scale() const override;
    glm::vec3 get_global_scale() const override;
    const glm::mat4& get_global_matrix_model() const override;
    void set_global_position(const glm::vec3& n_pos) override;
    void set_position(const glm::vec3& n_pos) override;
    void set_position(const float& x, const float& y, const float& z) override;
    void set_global_rotation(const glm::vec3& global_euler_deg) override;
    void set_rotation(const glm::vec3& n_rot) override;
    void set_rotation(const float& x, const float& y, const float& z) override;
    void set_global_scale(const glm::vec3& scalar) override;
    void set_scale(const float& x) override;
    void set_scale(const glm::vec3& n_sc) override;
    void set_scale(const float& x, const float& y, const float& z) override;
};
