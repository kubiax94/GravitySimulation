#include "ui_profiler_panel.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

#include "frame_profiler.h"
#include "font_resource.h"
#include <memory>

#include "ui_button_widget.h"
#include "input_system.h"
#include "ui_label_widget.h"
#include "ui_metric_row_widget.h"
#include "ui_render_pipeline.h"
#include "ui_stack_panel.h"
#include "ui_text_renderer.h"

namespace {
std::string build_metric_value(double average_value, double max_value) {
    std::ostringstream value_stream;
    value_stream << std::fixed << std::setprecision(2) << average_value << " avg / " << max_value << " max";
    return value_stream.str();
}

bool starts_with(const std::string& value, const char* prefix) {
    return value.rfind(prefix, 0) == 0;
}

bool is_gpu_section_name(const std::string& name);
bool is_ui_section_name(const std::string& name);

bool is_cpu_section_name(const std::string& name) {
    if (is_gpu_section_name(name) || is_ui_section_name(name))
        return false;

    return starts_with(name, "frame_")
        || starts_with(name, "fixed_update_")
        || starts_with(name, "poll_events")
        || starts_with(name, "handle_input")
        || starts_with(name, "update")
        || starts_with(name, "render")
        || starts_with(name, "swap_buffers");
}

bool is_gpu_section_name(const std::string& name) {
    return starts_with(name, "fixed_update_gpu_")
        || starts_with(name, "gpu_");
}

bool is_ui_section_name(const std::string& name) {
    return starts_with(name, "ui_")
        || starts_with(name, "render_ui");
}

bool is_top_level_cpu_section_name(const std::string& name) {
    return name == "frame_total"
        || name == "frame_cpu"
        || name == "poll_events"
        || name == "handle_input"
        || name == "fixed_update_total"
        || name == "fixed_update_step"
        || name == "update"
        || name == "render"
        || name == "swap_buffers";
}

bool is_point_inside_rect(const glm::vec2& point, const glm::vec2& top_left, const glm::vec2& size) {
    return point.x >= top_left.x
        && point.y >= top_left.y
        && point.x <= top_left.x + size.x
        && point.y <= top_left.y + size.y;
}

const char* resolve_section_title(const std::string& name) {
    if (is_ui_section_name(name))
        return "UI profiler";
    if (is_gpu_section_name(name))
        return "GPU profiler";
    return "CPU profiler";
}

std::vector<const frame_profiler::report_entry*> collect_section_entries(
    const std::vector<frame_profiler::report_entry>& sections,
    const std::function<bool(const std::string&)>& predicate,
    size_t limit) {
    std::vector<const frame_profiler::report_entry*> entries;
    entries.reserve(std::min(limit, sections.size()));
    for (const auto& section : sections) {
        if (!predicate(section.name))
            continue;

        entries.push_back(&section);
        if (entries.size() >= limit)
            break;
    }

    return entries;
}

struct scroll_thumb_geometry {
    glm::vec2 top_left = glm::vec2(0.0f);
    glm::vec2 size = glm::vec2(0.0f);
    float max_scroll = 0.0f;
    float thumb_range = 0.0f;
};

scroll_thumb_geometry compute_scroll_thumb_geometry(const ui_profiler_panel& panel, float panel_width, float header_height, float viewport_height, float content_height) {
    scroll_thumb_geometry geometry;
    geometry.max_scroll = std::max(content_height - viewport_height, 0.0f);
    if (geometry.max_scroll <= 0.0f)
        return geometry;

    const float track_margin = 6.0f;
    const float track_width = 8.0f;
    const float track_height = std::max(viewport_height - 8.0f, 0.0f);
    const float thumb_ratio = std::clamp(viewport_height / std::max(content_height, 1.0f), 0.0f, 1.0f);
    const float thumb_height = std::max(track_height * thumb_ratio, 22.0f);
    geometry.thumb_range = std::max(track_height - thumb_height, 0.0f);
    const float thumb_offset = geometry.max_scroll > 0.0f
        ? (panel.get_body_scroll_offset() / geometry.max_scroll) * geometry.thumb_range
        : 0.0f;
    geometry.top_left = panel.get_panel_top_left() + glm::vec2(panel_width - track_margin - track_width, header_height + 4.0f + thumb_offset);
    geometry.size = glm::vec2(track_width, thumb_height);
    return geometry;
}
}

ui_profiler_panel::ui_profiler_panel(std::string id)
    : ui_panel(std::move(id)) {
    set_position(18.0f, 18.0f, 0.0f);
    set_padding(ui_spacing(0.0f));

    auto content_stack = std::make_unique<ui_stack_panel>("ui.profiler.content");
    content_stack->set_color(glm::vec4(0.0f));
    content_stack->set_padding(ui_spacing(14.0f, 10.0f, 14.0f, 12.0f));
    content_stack->set_spacing(8.0f);
    content_stack_ = static_cast<ui_stack_panel*>(add_child(std::move(content_stack)));

    auto header_stack = std::make_unique<ui_stack_panel>("ui.profiler.header");
    header_stack->set_color(glm::vec4(0.0f));
    header_stack->set_direction(ui_layout_direction::horizontal);
    header_stack->set_spacing(14.0f);
    header_stack->set_layout_fill_x(true);
    header_stack_ = content_stack_ ? static_cast<ui_stack_panel*>(content_stack_->add_child(std::move(header_stack))) : nullptr;

    auto body_stack = std::make_unique<ui_stack_panel>("ui.profiler.body");
    body_stack->set_color(glm::vec4(0.0f));
    body_stack->set_spacing(8.0f);
    body_stack->set_layout_fill_x(true);
    body_stack_ = content_stack_ ? static_cast<ui_stack_panel*>(content_stack_->add_child(std::move(body_stack))) : nullptr;

    auto title = std::make_unique<ui_label_widget>("ui.profiler.title");
    title->set_text("Engine Profiler");
    title->set_scale_value(0.82f);
    title->set_color(glm::vec3(0.96f, 0.98f, 1.0f));
    title->set_layout_weight(1.0f);
    title->set_layout_fill_x(true);
    title->set_vertical_alignment(ui_vertical_alignment::center);
    title_label_ = header_stack_ ? static_cast<ui_label_widget*>(header_stack_->add_child(std::move(title))) : nullptr;

    auto toggle = std::make_unique<ui_button_widget>("ui.profiler.toggle");
    toggle->set_size(glm::vec2(132.0f, 28.0f));
    toggle->set_color(glm::vec4(0.14f, 0.17f, 0.24f, 0.94f));
    toggle->set_hover_color(glm::vec4(0.20f, 0.24f, 0.33f, 0.98f));
    toggle->set_text_color(glm::vec3(0.95f, 0.97f, 1.0f));
    toggle->set_padding(ui_spacing(12.0f, 10.0f));
    toggle->set_layout_weight(0.0f);
    toggle->set_on_click([this]() { visible_body_ = !visible_body_; });
    toggle_button_ = header_stack_ ? static_cast<ui_button_widget*>(header_stack_->add_child(std::move(toggle))) : nullptr;

    auto status = std::make_unique<ui_label_widget>("ui.profiler.interval");
    status->set_scale_value(0.72f);
    status->set_color(glm::vec3(0.86f, 0.91f, 0.98f));
    status->set_layout_fill_x(true);
    status->set_vertical_alignment(ui_vertical_alignment::center);
    interval_label_ = body_stack_ ? static_cast<ui_label_widget*>(body_stack_->add_child(std::move(status))) : nullptr;

    auto cpu_title = std::make_unique<ui_label_widget>("ui.profiler.cpu.title");
    cpu_title->set_text("CPU profiler");
    cpu_title->set_scale_value(0.72f);
    cpu_title->set_color(glm::vec3(0.96f, 0.98f, 1.0f));
    cpu_title->set_layout_fill_x(true);
    cpu_title->set_vertical_alignment(ui_vertical_alignment::center);
    cpu_title_label_ = body_stack_ ? static_cast<ui_label_widget*>(body_stack_->add_child(std::move(cpu_title))) : nullptr;

    auto cpu_stack = std::make_unique<ui_stack_panel>("ui.profiler.cpu.stack");
    cpu_stack->set_color(glm::vec4(0.0f));
    cpu_stack->set_spacing(0.0f);
    cpu_stack->set_layout_fill_x(true);
    cpu_stack_ = body_stack_ ? static_cast<ui_stack_panel*>(body_stack_->add_child(std::move(cpu_stack))) : nullptr;

    auto gpu_title = std::make_unique<ui_label_widget>("ui.profiler.gpu.title");
    gpu_title->set_text("GPU profiler");
    gpu_title->set_scale_value(0.72f);
    gpu_title->set_color(glm::vec3(0.96f, 0.98f, 1.0f));
    gpu_title->set_layout_fill_x(true);
    gpu_title->set_vertical_alignment(ui_vertical_alignment::center);
    gpu_title_label_ = body_stack_ ? static_cast<ui_label_widget*>(body_stack_->add_child(std::move(gpu_title))) : nullptr;

    auto gpu_stack = std::make_unique<ui_stack_panel>("ui.profiler.gpu.stack");
    gpu_stack->set_color(glm::vec4(0.0f));
    gpu_stack->set_spacing(0.0f);
    gpu_stack->set_layout_fill_x(true);
    gpu_stack_ = body_stack_ ? static_cast<ui_stack_panel*>(body_stack_->add_child(std::move(gpu_stack))) : nullptr;

    auto ui_title = std::make_unique<ui_label_widget>("ui.profiler.ui.title");
    ui_title->set_text("UI profiler");
    ui_title->set_scale_value(0.72f);
    ui_title->set_color(glm::vec3(0.96f, 0.98f, 1.0f));
    ui_title->set_layout_fill_x(true);
    ui_title->set_vertical_alignment(ui_vertical_alignment::center);
    ui_title_label_ = body_stack_ ? static_cast<ui_label_widget*>(body_stack_->add_child(std::move(ui_title))) : nullptr;

    auto ui_stack = std::make_unique<ui_stack_panel>("ui.profiler.ui.stack");
    ui_stack->set_color(glm::vec4(0.0f));
    ui_stack->set_spacing(0.0f);
    ui_stack->set_layout_fill_x(true);
    ui_stack_ = body_stack_ ? static_cast<ui_stack_panel*>(body_stack_->add_child(std::move(ui_stack))) : nullptr;

    auto values_title = std::make_unique<ui_label_widget>("ui.profiler.values.title");
    values_title->set_text("Pipeline values");
    values_title->set_scale_value(0.72f);
    values_title->set_color(glm::vec3(0.96f, 0.98f, 1.0f));
    values_title->set_layout_fill_x(true);
    values_title->set_vertical_alignment(ui_vertical_alignment::center);
    values_title_label_ = body_stack_ ? static_cast<ui_label_widget*>(body_stack_->add_child(std::move(values_title))) : nullptr;

    auto values_stack = std::make_unique<ui_stack_panel>("ui.profiler.values.stack");
    values_stack->set_color(glm::vec4(0.0f));
    values_stack->set_spacing(0.0f);
    values_stack->set_layout_fill_x(true);
    values_stack_ = body_stack_ ? static_cast<ui_stack_panel*>(body_stack_->add_child(std::move(values_stack))) : nullptr;
}

void ui_profiler_panel::ensure_metric_rows(size_t count,
    const std::string& id_prefix,
    ui_stack_panel* parent_stack,
    std::vector<ui_metric_row_widget*>& row_widgets) {
    if (!parent_stack)
        return;

    while (row_widgets.size() < count) {
        const size_t index = row_widgets.size();

        auto row_widget = std::make_unique<ui_metric_row_widget>(id_prefix + ".row." + std::to_string(index));
        row_widget->set_color(glm::vec4(0.0f));
        row_widget->set_column_gap(18.0f);
        row_widgets.push_back(static_cast<ui_metric_row_widget*>(parent_stack->add_child(std::move(row_widget))));
    }
}

void ui_profiler_panel::hide_metric_rows(std::vector<ui_metric_row_widget*>& row_widgets) {
    for (auto* row_widget : row_widgets) {
        if (row_widget)
            row_widget->set_visible(false);
    }
}

void ui_profiler_panel::update_body_scroll(ui_context& context, float header_height, float viewport_height, float content_height) {
    const float max_scroll = std::max(content_height - viewport_height, 0.0f);
    body_scroll_offset_ = std::clamp(body_scroll_offset_, 0.0f, max_scroll);
    const bool left_mouse_down = input_system::is_button_down(GLFW_MOUSE_BUTTON_LEFT);
    if (max_scroll <= 0.0f) {
        scroll_thumb_drag_active_ = false;
        previous_left_mouse_down_ = left_mouse_down;
        return;
    }

    const glm::vec2 panel_top_left = get_ui_offset();
    const glm::vec2 body_top_left = panel_top_left + glm::vec2(0.0f, header_height);
    const glm::vec2 body_size(get_size().x, viewport_height);
    const glm::vec2 mouse_pos(input_system::get_mouse_pos().x, input_system::get_mouse_pos().y);

    const scroll_thumb_geometry thumb_geometry = compute_scroll_thumb_geometry(*this, get_size().x, header_height, viewport_height, content_height);
    const bool pressed_this_frame = left_mouse_down && !previous_left_mouse_down_;

    if (pressed_this_frame && is_point_inside_rect(mouse_pos, thumb_geometry.top_left, thumb_geometry.size)) {
        scroll_thumb_drag_active_ = true;
        scroll_thumb_drag_offset_ = mouse_pos.y - thumb_geometry.top_left.y;
    }

    if (!left_mouse_down)
        scroll_thumb_drag_active_ = false;

    if (scroll_thumb_drag_active_) {
        const float track_top = panel_top_left.y + header_height + 4.0f;
        const float desired_thumb_top = mouse_pos.y - scroll_thumb_drag_offset_;
        const float clamped_thumb_offset = std::clamp(desired_thumb_top - track_top, 0.0f, thumb_geometry.thumb_range);
        body_scroll_offset_ = thumb_geometry.thumb_range > 0.0f
            ? (clamped_thumb_offset / thumb_geometry.thumb_range) * max_scroll
            : 0.0f;
        previous_left_mouse_down_ = left_mouse_down;
        return;
    }

    if (is_point_inside_rect(mouse_pos, body_top_left, body_size)) {
        const glm::vec2 scroll_delta = input_system::consume_mouse_scroll();
        body_scroll_offset_ = std::clamp(body_scroll_offset_ - scroll_delta.y * 24.0f, 0.0f, max_scroll);
    }

    previous_left_mouse_down_ = left_mouse_down;
}

void ui_profiler_panel::draw_body_scroll_indicator(ui_context& context, float panel_width, float header_height, float viewport_height, float content_height) const {
    const float max_scroll = std::max(content_height - viewport_height, 0.0f);
    if (max_scroll <= 0.0f)
        return;

    const float track_margin = 6.0f;
    const float track_width = 8.0f;
    const float track_height = std::max(viewport_height - 8.0f, 0.0f);
    const glm::vec2 track_top_left = get_ui_offset() + glm::vec2(panel_width - track_margin - track_width, header_height + 4.0f);
    context.pipeline.draw_panel(context.engine_instance, ui_render_pipeline::panel_desc{
        track_top_left,
        glm::vec2(track_width, track_height),
        glm::vec4(0.10f, 0.12f, 0.16f, 0.88f)
    });

    const scroll_thumb_geometry thumb_geometry = compute_scroll_thumb_geometry(*this, panel_width, header_height, viewport_height, content_height);
    context.pipeline.draw_panel(context.engine_instance, ui_render_pipeline::panel_desc{
        thumb_geometry.top_left,
        thumb_geometry.size,
        scroll_thumb_drag_active_
            ? glm::vec4(0.58f, 0.66f, 0.82f, 0.98f)
            : glm::vec4(0.42f, 0.49f, 0.62f, 0.96f)
    });
}

void ui_profiler_panel::render(ui_context& context) {
    auto profiler_panel_section = frame_profiler::measure_active("ui_profiler_panel_render");

    if (!is_visible())
        return;

    if (input_system::is_key_pressed(GLFW_KEY_F9))
        visible_body_ = !visible_body_;

    auto* font = context.text_renderer.get_default_font();
    if (!font)
        return;

    const frame_profiler::report_snapshot& report = context.profiler.get_last_report();
    const float title_scale = 0.82f;
    const float body_scale = 0.72f;
    const float minimum_panel_width = 470.0f;
    const float maximum_panel_height = 520.0f;
    const float header_height = 42.0f;
    const float panel_side_padding = 14.0f;
    const float content_padding = 12.0f;
    const float button_width = 132.0f;
    const float button_height = 28.0f;
    const float column_gap = 18.0f;
    const float line_height = static_cast<float>(std::max(font->get_line_height(), static_cast<int>(font->get_pixel_height())));
    const float row_height = line_height * body_scale;

    const ui_text_renderer::text_bounds title_bounds = context.text_renderer.measure_text("Engine Profiler", title_scale);
    std::ostringstream interval_stream;
    interval_stream << "Profiler interval: " << report.frame_count << " frames";
    const ui_text_renderer::text_bounds interval_bounds = context.text_renderer.measure_text(interval_stream.str(), body_scale);

    float metrics_max_width = interval_bounds.max_line_width;
    const auto measure_metric_row = [&](const std::string& label, const std::string& value) {
        const ui_text_renderer::text_bounds label_bounds = context.text_renderer.measure_text(label, body_scale);
        const ui_text_renderer::text_bounds value_bounds = context.text_renderer.measure_text(value, body_scale);
        metrics_max_width = std::max(metrics_max_width, label_bounds.max_line_width + column_gap + value_bounds.max_line_width);
    };

    const auto cpu_sections = collect_section_entries(report.sections, is_top_level_cpu_section_name, 8u);
    const auto gpu_sections = collect_section_entries(report.sections, is_gpu_section_name, 8u);
    const auto ui_sections = collect_section_entries(report.sections, is_ui_section_name, 8u);
    const size_t value_limit = std::min<size_t>(report.values.size(), 5u);
    for (const auto* entry : cpu_sections)
        measure_metric_row(entry->name, build_metric_value(entry->avg_frame_ms, entry->max_frame_ms));
    for (const auto* entry : gpu_sections)
        measure_metric_row(entry->name, build_metric_value(entry->avg_frame_ms, entry->max_frame_ms));
    for (const auto* entry : ui_sections)
        measure_metric_row(entry->name, build_metric_value(entry->avg_frame_ms, entry->max_frame_ms));
    for (size_t i = 0; i < value_limit; ++i)
        measure_metric_row(report.values[i].name, build_metric_value(report.values[i].avg_frame_value, report.values[i].max_frame_value));

    const float panel_width = std::max(
        minimum_panel_width,
        std::max(title_bounds.max_line_width + button_width + panel_side_padding * 3.0f,
            metrics_max_width + panel_side_padding * 2.0f));

    float body_height = 0.0f;
    if (visible_body_) {
        body_height += row_height + content_padding;
        if (!cpu_sections.empty())
            body_height += row_height + static_cast<float>(cpu_sections.size()) * row_height + content_padding;
        if (!gpu_sections.empty())
            body_height += row_height + static_cast<float>(gpu_sections.size()) * row_height + content_padding;
        if (!ui_sections.empty())
            body_height += row_height + static_cast<float>(ui_sections.size()) * row_height + content_padding;
        if (value_limit > 0u)
            body_height += row_height + static_cast<float>(value_limit) * row_height + content_padding;
    }

    const float content_height = header_height + body_height;
    const float panel_height = std::min(content_height, maximum_panel_height);
    const float body_viewport_height = std::max(panel_height - header_height, 0.0f);
    const float body_content_height = std::max(content_height - header_height, 0.0f);

    set_size(glm::vec2(panel_width, panel_height));
    set_color(glm::vec4(0.04f, 0.05f, 0.08f, 0.84f));
    set_padding(ui_spacing(0.0f));
    update_body_scroll(context, header_height, body_viewport_height, body_content_height);

    if (content_stack_) {
        content_stack_->set_size(glm::vec2(panel_width, content_height));
        content_stack_->set_color(glm::vec4(0.0f));
        content_stack_->set_auto_size_enabled(false);
        content_stack_->set_padding(ui_spacing(panel_side_padding, 10.0f, panel_side_padding, content_padding));
        content_stack_->set_spacing(8.0f);
        content_stack_->set_position(0.0f, 0.0f, 0.0f);
    }

    if (header_stack_) {
        header_stack_->set_size(glm::vec2(panel_width - panel_side_padding * 2.0f, button_height));
        header_stack_->set_color(glm::vec4(0.0f));
        header_stack_->set_auto_size_enabled(false);
        header_stack_->set_spacing(14.0f);
    }

    if (body_stack_) {
        body_stack_->set_color(glm::vec4(0.0f));
        body_stack_->set_auto_size_enabled(true);
        body_stack_->set_spacing(8.0f);
        body_stack_->set_position(0.0f, header_height - body_scroll_offset_, 0.0f);
    }

    if (cpu_stack_) {
        cpu_stack_->set_color(glm::vec4(0.0f));
        cpu_stack_->set_auto_size_enabled(true);
        cpu_stack_->set_spacing(0.0f);
    }

    if (gpu_stack_) {
        gpu_stack_->set_color(glm::vec4(0.0f));
        gpu_stack_->set_auto_size_enabled(true);
        gpu_stack_->set_spacing(0.0f);
    }

    if (ui_stack_) {
        ui_stack_->set_color(glm::vec4(0.0f));
        ui_stack_->set_auto_size_enabled(true);
        ui_stack_->set_spacing(0.0f);
    }

    if (values_stack_) {
        values_stack_->set_color(glm::vec4(0.0f));
        values_stack_->set_auto_size_enabled(true);
        values_stack_->set_spacing(0.0f);
    }

    if (toggle_button_) {
        toggle_button_->set_text(visible_body_ ? "Hide profiler" : "Show profiler");
        toggle_button_->set_size(glm::vec2(button_width, button_height));
    }

    if (interval_label_) {
        interval_label_->set_text(interval_stream.str());
        interval_label_->set_bounds_size(glm::vec2(panel_width - panel_side_padding * 2.0f, row_height));
        interval_label_->set_visible(visible_body_);
    }

    const float content_width = std::max(panel_width - panel_side_padding * 2.0f, 0.0f);

    hide_metric_rows(cpu_row_widgets_);
    hide_metric_rows(gpu_row_widgets_);
    hide_metric_rows(ui_row_widgets_);
    hide_metric_rows(value_row_widgets_);

    if (!visible_body_) {
        if (body_stack_)
            body_stack_->set_visible(false);
        if (cpu_title_label_)
            cpu_title_label_->set_visible(false);
        if (gpu_title_label_)
            gpu_title_label_->set_visible(false);
        if (ui_title_label_)
            ui_title_label_->set_visible(false);
        if (values_title_label_)
            values_title_label_->set_visible(false);

        context.pipeline.draw_panel(context.engine_instance,
            ui_render_pipeline::panel_desc{
                get_ui_offset(),
                get_size(),
                get_color(),
                get_padding(),
                get_scene_anchor(),
                get_anchor_offset(),
                false
            });

        if (header_stack_)
            header_stack_->render(context);

        return;
    }

    if (body_stack_)
        body_stack_->set_visible(true);

    if (cpu_title_label_) {
        cpu_title_label_->set_text(resolve_section_title("frame_cpu"));
        cpu_title_label_->set_visible(!cpu_sections.empty());
        cpu_title_label_->set_bounds_size(glm::vec2(content_width, row_height));
    }

    if (cpu_stack_)
        cpu_stack_->set_visible(!cpu_sections.empty());

    ensure_metric_rows(cpu_sections.size(), "ui.profiler.cpu", cpu_stack_, cpu_row_widgets_);
    for (size_t i = 0; i < cpu_row_widgets_.size(); ++i) {
        const bool row_visible = i < cpu_sections.size();
        if (cpu_row_widgets_[i])
            cpu_row_widgets_[i]->set_visible(row_visible);
        if (!row_visible)
            continue;

        const auto& entry = *cpu_sections[i];
        if (cpu_row_widgets_[i]) {
            cpu_row_widgets_[i]->set_size(glm::vec2(content_width, row_height));
            cpu_row_widgets_[i]->set_label(entry.name);
            cpu_row_widgets_[i]->set_value(build_metric_value(entry.avg_frame_ms, entry.max_frame_ms));
            cpu_row_widgets_[i]->set_scale_value(body_scale);
            cpu_row_widgets_[i]->set_label_color(glm::vec3(0.86f, 0.91f, 0.98f));
            cpu_row_widgets_[i]->set_value_color(glm::vec3(0.86f, 0.91f, 0.98f));
            cpu_row_widgets_[i]->set_column_gap(column_gap);
        }
    }

    if (gpu_title_label_) {
        gpu_title_label_->set_text(resolve_section_title("gpu"));
        gpu_title_label_->set_visible(!gpu_sections.empty());
        gpu_title_label_->set_bounds_size(glm::vec2(content_width, row_height));
    }

    if (gpu_stack_)
        gpu_stack_->set_visible(!gpu_sections.empty());

    ensure_metric_rows(gpu_sections.size(), "ui.profiler.gpu", gpu_stack_, gpu_row_widgets_);
    for (size_t i = 0; i < gpu_row_widgets_.size(); ++i) {
        const bool row_visible = i < gpu_sections.size();
        if (gpu_row_widgets_[i])
            gpu_row_widgets_[i]->set_visible(row_visible);
        if (!row_visible)
            continue;

        const auto& entry = *gpu_sections[i];
        if (gpu_row_widgets_[i]) {
            gpu_row_widgets_[i]->set_size(glm::vec2(content_width, row_height));
            gpu_row_widgets_[i]->set_label(entry.name);
            gpu_row_widgets_[i]->set_value(build_metric_value(entry.avg_frame_ms, entry.max_frame_ms));
            gpu_row_widgets_[i]->set_scale_value(body_scale);
            gpu_row_widgets_[i]->set_label_color(glm::vec3(0.86f, 0.91f, 0.98f));
            gpu_row_widgets_[i]->set_value_color(glm::vec3(0.86f, 0.91f, 0.98f));
            gpu_row_widgets_[i]->set_column_gap(column_gap);
        }
    }

    if (ui_title_label_) {
        ui_title_label_->set_text(resolve_section_title("ui"));
        ui_title_label_->set_visible(!ui_sections.empty());
        ui_title_label_->set_bounds_size(glm::vec2(content_width, row_height));
    }

    if (ui_stack_)
        ui_stack_->set_visible(!ui_sections.empty());

    ensure_metric_rows(ui_sections.size(), "ui.profiler.ui", ui_stack_, ui_row_widgets_);
    for (size_t i = 0; i < ui_row_widgets_.size(); ++i) {
        const bool row_visible = i < ui_sections.size();
        if (ui_row_widgets_[i])
            ui_row_widgets_[i]->set_visible(row_visible);
        if (!row_visible)
            continue;

        const auto& entry = *ui_sections[i];
        if (ui_row_widgets_[i]) {
            ui_row_widgets_[i]->set_size(glm::vec2(content_width, row_height));
            ui_row_widgets_[i]->set_label(entry.name);
            ui_row_widgets_[i]->set_value(build_metric_value(entry.avg_frame_ms, entry.max_frame_ms));
            ui_row_widgets_[i]->set_scale_value(body_scale);
            ui_row_widgets_[i]->set_label_color(glm::vec3(0.86f, 0.91f, 0.98f));
            ui_row_widgets_[i]->set_value_color(glm::vec3(0.86f, 0.91f, 0.98f));
            ui_row_widgets_[i]->set_column_gap(column_gap);
        }
    }

    if (values_title_label_) {
        values_title_label_->set_visible(value_limit > 0u);
        values_title_label_->set_bounds_size(glm::vec2(content_width, row_height));
    }

    if (values_stack_)
        values_stack_->set_visible(value_limit > 0u);

    ensure_metric_rows(value_limit, "ui.profiler.values", values_stack_, value_row_widgets_);
    for (size_t i = 0; i < value_row_widgets_.size(); ++i) {
        const bool row_visible = i < value_limit;
        if (value_row_widgets_[i])
            value_row_widgets_[i]->set_visible(row_visible);
        if (!row_visible)
            continue;

        const auto& entry = report.values[i];
        if (value_row_widgets_[i]) {
            value_row_widgets_[i]->set_size(glm::vec2(content_width, row_height));
            value_row_widgets_[i]->set_label(entry.name);
            value_row_widgets_[i]->set_value(build_metric_value(entry.avg_frame_value, entry.max_frame_value));
            value_row_widgets_[i]->set_scale_value(body_scale);
            value_row_widgets_[i]->set_label_color(glm::vec3(0.86f, 0.91f, 0.98f));
            value_row_widgets_[i]->set_value_color(glm::vec3(0.86f, 0.91f, 0.98f));
            value_row_widgets_[i]->set_column_gap(column_gap);
        }
    }

    context.pipeline.draw_panel(context.engine_instance,
        ui_render_pipeline::panel_desc{
            get_ui_offset(),
            get_size(),
            get_color(),
            get_padding(),
            get_scene_anchor(),
            get_anchor_offset(),
            false
        });

    if (header_stack_)
        header_stack_->render(context);

    if (visible_body_ && body_stack_ && body_viewport_height > 0.0f) {
        context.pipeline.begin_clip_rect(context.engine_instance,
            get_ui_offset() + glm::vec2(0.0f, header_height),
            glm::vec2(panel_width, body_viewport_height));
        body_stack_->render(context);
        context.pipeline.end_clip_rect();
        draw_body_scroll_indicator(context, panel_width, header_height, body_viewport_height, body_content_height);
    }
}
