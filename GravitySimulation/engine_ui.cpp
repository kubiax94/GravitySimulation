#include "engine_ui.h"

#include "Scene.h"
#include "engine.h"
#include "engine_state.h"
#include "frame_profiler.h"
#include "input_system.h"
#include "ui_loading_panel.h"
#include "ui_label_widget.h"
#include "ui_profiler_panel.h"
#include "ui_widget.h"

bool engine_ui::initialize(scene& scene_context) {
    if (bound_scene_ != nullptr && bound_scene_ != &scene_context)
        shutdown();

    const bool text_ready = text_renderer_.initialize(scene_context);
    const bool pipeline_ready = text_ready && ui_pipeline_.initialize(scene_context, text_renderer_);
    bound_scene_ = (text_ready && pipeline_ready) ? &scene_context : nullptr;
    if (bound_scene_)
        ensure_default_widgets();
    return text_ready && pipeline_ready;
}

void engine_ui::shutdown() {
    ui_pipeline_.shutdown();
    text_renderer_.shutdown();
    bound_scene_ = nullptr;
    runtime_widgets_.clear();
    editor_widgets_.clear();
}

void engine_ui::begin_frame() {
    anchored_labels_.clear();
}

void engine_ui::ensure_default_widgets() {
    if (!runtime_widgets_.empty())
        return;

    register_runtime_widget(std::make_unique<ui_profiler_panel>());
    register_runtime_widget(std::make_unique<ui_loading_panel>());
}

void engine_ui::render(engine& engine, engine_state* state, scene* scene_context) {
    auto ui_render_section = frame_profiler::measure_active("ui_render_total");

    if (!scene_context) {
        shutdown();
        return;
    }

    if (bound_scene_ != scene_context || !is_ready()) {
        initialize(*scene_context);
        if (!is_ready())
            return;
    }

    ui_pipeline_.begin_frame(scene_context);

    ui_context context{
        engine,
        ui_pipeline_,
        text_renderer_,
        engine.get_frame_profiler(),
        &loading_feedback_state_,
        scene_context,
        state
    };

    {
        auto runtime_widgets_section = frame_profiler::measure_active("ui_render_runtime_widgets");
        for (auto& widget : runtime_widgets_) {
            if (widget)
                widget->render(context);
        }
    }

    {
        auto editor_widgets_section = frame_profiler::measure_active("ui_render_editor_widgets");
        for (auto& widget : editor_widgets_) {
            if (widget)
                widget->render(context);
        }
    }

    {
        auto anchored_labels_section = frame_profiler::measure_active("ui_render_anchored_labels");
        for (const auto& label : anchored_labels_) {
            if (!label.anchor)
                continue;

            ui_render_pipeline::label_desc label_desc{};
            label_desc.text = label.text;
            label_desc.top_left = glm::vec2(0.0f);
            label_desc.scale = label.scale;
            label_desc.color = label.color;
            label_desc.bounds_size = glm::vec2(0.0f);
            label_desc.horizontal_alignment = ui_horizontal_alignment::left;
            label_desc.vertical_alignment = ui_vertical_alignment::top;
            label_desc.anchor = label.anchor;
            label_desc.anchor_offset = label.offset;
            ui_pipeline_.draw_label(engine, label_desc);
        }
    }
}

void engine_ui::end_frame() {
    ui_pipeline_.end_frame();
}

bool engine_ui::is_ready() const {
    return text_renderer_.is_ready() && ui_pipeline_.is_ready();
}

void engine_ui::submit_anchored_label(anchored_label label) {
    anchored_labels_.push_back(std::move(label));
}

void engine_ui::register_runtime_widget(std::unique_ptr<ui_widget> widget) {
    if (!widget)
        return;

    runtime_widgets_.push_back(std::move(widget));
}

void engine_ui::register_editor_widget(std::unique_ptr<ui_widget> widget) {
    if (!widget)
        return;

    editor_widgets_.push_back(std::move(widget));
}
