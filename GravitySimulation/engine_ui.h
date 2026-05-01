#pragma once

#include <string>
#include <vector>

#include "frame_profiler.h"
#include "transformable.h"
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
    bool profiler_visible_ = true;
    scene* bound_scene_ = nullptr;
    std::vector<anchored_label> anchored_labels_;

    static std::string build_profiler_overlay_text(const frame_profiler::report_snapshot& report);

public:
    bool initialize(scene& scene_context);
    void shutdown();
    void begin_frame();
    void render(engine& engine, engine_state* state, scene* scene_context);
    void end_frame();
    bool is_ready() const;
    void submit_anchored_label(anchored_label label);
};
