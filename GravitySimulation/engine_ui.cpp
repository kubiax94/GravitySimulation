#include "engine_ui.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

#include <glm/common.hpp>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "Scene.h"
#include "engine.h"
#include "engine_state.h"
#include "font_resource.h"
#include "input_system.h"

bool engine_ui::initialize(scene& scene_context) {
    if (bound_scene_ != nullptr && bound_scene_ != &scene_context)
        shutdown();

    const bool text_ready = text_renderer_.initialize(scene_context);
    const bool pipeline_ready = text_ready && ui_pipeline_.initialize(scene_context, text_renderer_);
    bound_scene_ = (text_ready && pipeline_ready) ? &scene_context : nullptr;
    return text_ready && pipeline_ready;
}

void engine_ui::shutdown() {
    ui_pipeline_.shutdown();
    text_renderer_.shutdown();
    bound_scene_ = nullptr;
}

void engine_ui::begin_frame() {
    anchored_labels_.clear();
}

std::string engine_ui::build_profiler_overlay_text(const frame_profiler::report_snapshot& report) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2);
    stream << "Profiler interval: " << report.frame_count << " frames";

    if (report.sections.empty() && report.values.empty()) {
        stream << "\nWaiting for frame profiler snapshot...";
        return stream.str();
    }

    const size_t section_limit = std::min<size_t>(report.sections.size(), 8u);
    if (section_limit > 0u) {
        stream << "\n\nTop frame sections:";
        for (size_t i = 0; i < section_limit; ++i) {
            const auto& entry = report.sections[i];
            stream << "\n- " << entry.name << ": " << entry.avg_frame_ms << " ms"
                   << " avg / " << entry.max_frame_ms << " max";
        }
    }

    const size_t value_limit = std::min<size_t>(report.values.size(), 5u);
    if (value_limit > 0u) {
        stream << "\n\nPipeline values:";
        for (size_t i = 0; i < value_limit; ++i) {
            const auto& entry = report.values[i];
            stream << "\n- " << entry.name << ": " << entry.avg_frame_value
                   << " avg / " << entry.max_frame_value << " max";
        }
    }

    return stream.str();
}

void engine_ui::render(engine& engine, engine_state* state, scene* scene_context) {
    (void)state;
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

    if (input_system::is_key_pressed(GLFW_KEY_F9))
        profiler_visible_ = !profiler_visible_;

    auto* font = text_renderer_.get_default_font();
    if (!font)
        return;

    const float line_height = static_cast<float>(std::max(font->get_line_height(), static_cast<int>(font->get_pixel_height())));
    const float title_scale = 0.82f;
    const float body_scale = 0.72f;
    const float panel_x = 18.0f;
    const float panel_y = 18.0f;
    const float minimum_panel_width = 470.0f;
    const float header_height = 42.0f;
    const float button_width = 132.0f;
    const float button_height = 28.0f;
    const float content_padding = 12.0f;
    const float panel_side_padding = 14.0f;
    const float row_height = line_height * body_scale;

    const auto& report = engine.get_frame_profiler().get_last_report();
    const auto title_bounds = text_renderer_.measure_text("Engine Profiler", title_scale);
    const auto interval_bounds = text_renderer_.measure_text(build_profiler_overlay_text({ report.frame_count, {}, {} }), body_scale);

    float metrics_max_width = interval_bounds.max_line_width;
    const auto measure_metric_row = [&](const std::string& label, const std::string& value) {
        const auto label_bounds = text_renderer_.measure_text(label, body_scale);
        const auto value_bounds = text_renderer_.measure_text(value, body_scale);
        metrics_max_width = std::max(metrics_max_width, label_bounds.max_line_width + 18.0f + value_bounds.max_line_width);
    };

    const size_t section_limit = std::min<size_t>(report.sections.size(), 8u);
    const size_t value_limit = std::min<size_t>(report.values.size(), 5u);
    for (size_t i = 0; i < section_limit; ++i) {
        const auto& entry = report.sections[i];
        std::ostringstream value_stream;
        value_stream << std::fixed << std::setprecision(2) << entry.avg_frame_ms << " avg / " << entry.max_frame_ms << " max";
        measure_metric_row(entry.name, value_stream.str());
    }
    for (size_t i = 0; i < value_limit; ++i) {
        const auto& entry = report.values[i];
        std::ostringstream value_stream;
        value_stream << std::fixed << std::setprecision(2) << entry.avg_frame_value << " avg / " << entry.max_frame_value << " max";
        measure_metric_row(entry.name, value_stream.str());
    }

    const float panel_width = std::max(
        minimum_panel_width,
        std::max(title_bounds.max_line_width + button_width + panel_side_padding * 3.0f,
                 metrics_max_width + panel_side_padding * 2.0f));

    float body_height = 0.0f;
    if (profiler_visible_) {
        body_height += row_height + content_padding;
        if (section_limit > 0u)
            body_height += row_height + static_cast<float>(section_limit) * row_height + content_padding;
        if (value_limit > 0u)
            body_height += row_height + static_cast<float>(value_limit) * row_height + content_padding;
    }
    const float panel_height = header_height + body_height;

    const ui_render_pipeline::panel_desc profiler_panel{
        glm::vec2(panel_x, panel_y),
        glm::vec2(panel_width, panel_height),
        glm::vec4(0.04f, 0.05f, 0.08f, 0.84f),
        ui_spacing(panel_side_padding, 10.0f, panel_side_padding, content_padding)
    };

    ui_pipeline_.draw_panel_with_children(engine,
        profiler_panel,
        [&]() {
            auto layout = ui_pipeline_.begin_vertical_layout(profiler_panel, 6.0f);
            const float content_width = ui_pipeline_.get_layout_content_width(layout);

            ui_pipeline_.draw_label(engine, {
                "Engine Profiler",
                ui_pipeline_.push_layout_item(layout, line_height * title_scale),
                title_scale,
                glm::vec3(0.96f, 0.98f, 1.0f)
            });

            ui_pipeline_.draw_button(engine, {
                "profiler.toggle",
                profiler_visible_ ? "Hide profiler" : "Show profiler",
                glm::vec2(panel_x + panel_width - button_width - 14.0f, panel_y + 7.0f),
                glm::vec2(button_width, button_height),
                glm::vec4(0.14f, 0.17f, 0.24f, 0.94f),
                glm::vec4(0.20f, 0.24f, 0.33f, 0.98f),
                glm::vec3(0.95f, 0.97f, 1.0f),
                ui_spacing(12.0f, 10.0f),
                nullptr,
                glm::vec2(0.0f),
                [&]() { profiler_visible_ = !profiler_visible_; }
            });

            if (!profiler_visible_)
                return;

            {
                auto interval_position = ui_pipeline_.push_layout_item(layout, row_height, ui_spacing(0.0f, 4.0f, 0.0f, 4.0f));
                std::ostringstream interval_stream;
                interval_stream << "Profiler interval: " << report.frame_count << " frames";
                ui_pipeline_.draw_label(engine, {
                    interval_stream.str(),
                    interval_position,
                    body_scale,
                    glm::vec3(0.86f, 0.91f, 0.98f)
                });
            }

            if (section_limit > 0u) {
                const auto heading_position = ui_pipeline_.push_layout_item(layout, row_height, ui_spacing(0.0f, 6.0f, 0.0f, 2.0f));
                ui_pipeline_.draw_label(engine, {
                    "Top frame sections",
                    heading_position,
                    body_scale,
                    glm::vec3(0.96f, 0.98f, 1.0f)
                });

                for (size_t i = 0; i < section_limit; ++i) {
                    const auto& entry = report.sections[i];
                    std::ostringstream value_stream;
                    value_stream << std::fixed << std::setprecision(2) << entry.avg_frame_ms << " avg / " << entry.max_frame_ms << " max";
                    ui_pipeline_.draw_metric_row(engine, {
                        entry.name,
                        value_stream.str(),
                        ui_pipeline_.push_layout_item(layout, row_height),
                        glm::vec2(content_width, row_height),
                        body_scale,
                        glm::vec3(0.86f, 0.91f, 0.98f),
                        glm::vec3(0.86f, 0.91f, 0.98f),
                        18.0f
                    });
                }
            }

            if (value_limit > 0u) {
                const auto heading_position = ui_pipeline_.push_layout_item(layout, row_height, ui_spacing(0.0f, 8.0f, 0.0f, 2.0f));
                ui_pipeline_.draw_label(engine, {
                    "Pipeline values",
                    heading_position,
                    body_scale,
                    glm::vec3(0.96f, 0.98f, 1.0f)
                });

                for (size_t i = 0; i < value_limit; ++i) {
                    const auto& entry = report.values[i];
                    std::ostringstream value_stream;
                    value_stream << std::fixed << std::setprecision(2) << entry.avg_frame_value << " avg / " << entry.max_frame_value << " max";
                    ui_pipeline_.draw_metric_row(engine, {
                        entry.name,
                        value_stream.str(),
                        ui_pipeline_.push_layout_item(layout, row_height),
                        glm::vec2(content_width, row_height),
                        body_scale,
                        glm::vec3(0.86f, 0.91f, 0.98f),
                        glm::vec3(0.86f, 0.91f, 0.98f),
                        18.0f
                    });
                }
            }
        });

    for (const auto& label : anchored_labels_) {
        if (!label.anchor)
            continue;

        ui_pipeline_.draw_label(engine, {
            label.text,
            glm::vec2(0.0f),
            label.scale,
            label.color,
            glm::vec2(0.0f),
            ui_horizontal_alignment::left,
            label.anchor,
            label.offset
        });
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
