#pragma once

#include <string>
#include <memory>
#include <vector>

#include "frame_profiler.h"
#include "transformable.h"
#include "ui_loading_feedback_state.h"
#include "ui_widget.h"
#include "ui_render_pipeline.h"
#include "ui_text_renderer.h"

class engine;
class scene;
class engine_state;

class engine_ui
{
public:
    struct anchored_label {
        const i_transformable* anchor = nullptr;
        std::string text;
        glm::vec2 offset = glm::vec2(0.0f);
        float scale = 1.0f;
        glm::vec3 color = glm::vec3(1.0f);
    };

private:
    ui_text_renderer text_renderer_;
    ui_render_pipeline ui_pipeline_;
    scene* bound_scene_ = nullptr;
    std::vector<anchored_label> anchored_labels_;
    std::vector<std::unique_ptr<ui_widget>> runtime_widgets_;
    std::vector<std::unique_ptr<ui_widget>> editor_widgets_;
    ui_loading_feedback_state loading_feedback_state_{};

    void ensure_default_widgets();

public:
    bool initialize(scene& scene_context);
    void shutdown();
    void begin_frame();
    void render(engine& engine, engine_state* state, scene* scene_context);
    void end_frame();
    bool is_ready() const;
    void submit_anchored_label(anchored_label label);
    void submit_loading_feedback(ui_loading_feedback_state state) { loading_feedback_state_ = std::move(state); }
    void clear_loading_feedback() { loading_feedback_state_ = ui_loading_feedback_state{}; }
    const ui_loading_feedback_state& get_loading_feedback_state() const { return loading_feedback_state_; }
    void register_runtime_widget(std::unique_ptr<ui_widget> widget);
    void register_editor_widget(std::unique_ptr<ui_widget> widget);
};
