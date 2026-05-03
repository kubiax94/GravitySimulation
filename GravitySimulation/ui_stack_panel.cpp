#include "ui_stack_panel.h"

#include <algorithm>

#include "ui_button_widget.h"
#include "frame_profiler.h"
#include "ui_label_widget.h"
#include "ui_metric_row_widget.h"
#include "ui_panel.h"
#include "ui_progress_bar_widget.h"
#include "ui_render_pipeline.h"

namespace {
void set_widget_main_extent(ui_widget* child, ui_layout_direction direction, float main_extent) {
    if (!child)
        return;

    if (auto* label = dynamic_cast<ui_label_widget*>(child)) {
        glm::vec2 bounds = label->get_bounds_size();
        if (direction == ui_layout_direction::vertical)
            bounds.y = main_extent;
        else
            bounds.x = main_extent;
        label->set_bounds_size(bounds);
        return;
    }

    if (auto* button = dynamic_cast<ui_button_widget*>(child)) {
        glm::vec2 size = button->get_size();
        if (direction == ui_layout_direction::vertical)
            size.y = main_extent;
        else
            size.x = main_extent;
        button->set_size(size);
        return;
    }

    if (auto* progress_bar = dynamic_cast<ui_progress_bar_widget*>(child)) {
        glm::vec2 size = progress_bar->get_size();
        if (direction == ui_layout_direction::vertical)
            size.y = main_extent;
        else
            size.x = main_extent;
        progress_bar->set_size(size);
        return;
    }

    if (auto* metric_row = dynamic_cast<ui_metric_row_widget*>(child)) {
        glm::vec2 size = metric_row->get_size();
        if (direction == ui_layout_direction::vertical)
            size.y = main_extent;
        else
            size.x = main_extent;
        metric_row->set_size(size);
        return;
    }

    if (auto* panel = dynamic_cast<ui_panel*>(child)) {
        glm::vec2 size = panel->get_size();
        if (direction == ui_layout_direction::vertical)
            size.y = main_extent;
        else
            size.x = main_extent;
        panel->set_size(size);
        return;
    }

    if (auto* stack_panel = dynamic_cast<ui_stack_panel*>(child)) {
        glm::vec2 size = stack_panel->get_size();
        if (direction == ui_layout_direction::vertical)
            size.y = main_extent;
        else
            size.x = main_extent;
        stack_panel->set_size(size);
    }
}

void apply_fill_to_child(ui_widget* child, ui_layout_direction direction, float available_main, float available_cross) {
    if (!child)
        return;

    const float available_width = direction == ui_layout_direction::vertical ? available_cross : available_main;
    const float available_height = direction == ui_layout_direction::vertical ? available_main : available_cross;

    if (auto* label = dynamic_cast<ui_label_widget*>(child)) {
        glm::vec2 bounds = label->get_bounds_size();
        if (child->get_layout_fill_x())
            bounds.x = available_width;
        if (child->get_layout_fill_y())
            bounds.y = available_height;
        label->set_bounds_size(bounds);
        return;
    }

    if (auto* button = dynamic_cast<ui_button_widget*>(child)) {
        glm::vec2 size = button->get_size();
        if (child->get_layout_fill_x())
            size.x = available_width;
        if (child->get_layout_fill_y())
            size.y = available_height;
        button->set_size(size);
        return;
    }

    if (auto* progress_bar = dynamic_cast<ui_progress_bar_widget*>(child)) {
        glm::vec2 size = progress_bar->get_size();
        if (child->get_layout_fill_x())
            size.x = available_width;
        if (child->get_layout_fill_y())
            size.y = available_height;
        progress_bar->set_size(size);
        return;
    }

    if (auto* metric_row = dynamic_cast<ui_metric_row_widget*>(child)) {
        glm::vec2 size = metric_row->get_size();
        if (child->get_layout_fill_x())
            size.x = available_width;
        if (child->get_layout_fill_y())
            size.y = available_height;
        metric_row->set_size(size);
        return;
    }

    if (auto* panel = dynamic_cast<ui_panel*>(child)) {
        glm::vec2 size = panel->get_size();
        if (child->get_layout_fill_x())
            size.x = available_width;
        if (child->get_layout_fill_y())
            size.y = available_height;
        panel->set_size(size);
        return;
    }

    if (auto* stack_panel = dynamic_cast<ui_stack_panel*>(child)) {
        glm::vec2 size = stack_panel->get_size();
        if (child->get_layout_fill_x())
            size.x = available_width;
        if (child->get_layout_fill_y())
            size.y = available_height;
        stack_panel->set_size(size);
    }
}

float resolve_widget_extent(ui_context& context, ui_widget* widget, ui_layout_direction direction) {
    if (!widget)
        return 0.0f;

    if (auto* label = dynamic_cast<ui_label_widget*>(widget)) {
        const auto bounds = context.text_renderer.measure_text(label->get_text(), label->get_scale_value());
        const glm::vec2 explicit_bounds = label->get_bounds_size();
        if (direction == ui_layout_direction::vertical)
            return explicit_bounds.y > 0.0f ? explicit_bounds.y : std::max(bounds.size.y, bounds.line_height);

        return explicit_bounds.x > 0.0f ? explicit_bounds.x : bounds.max_line_width;
    }

    if (auto* button = dynamic_cast<ui_button_widget*>(widget))
        return direction == ui_layout_direction::vertical ? button->get_size().y : button->get_size().x;

    if (auto* progress_bar = dynamic_cast<ui_progress_bar_widget*>(widget))
        return direction == ui_layout_direction::vertical ? progress_bar->get_size().y : progress_bar->get_size().x;

    if (auto* metric_row = dynamic_cast<ui_metric_row_widget*>(widget))
        return direction == ui_layout_direction::vertical ? metric_row->get_size().y : metric_row->get_size().x;

    if (auto* panel = dynamic_cast<ui_panel*>(widget))
        return direction == ui_layout_direction::vertical ? panel->get_size().y : panel->get_size().x;

    if (auto* stack_panel = dynamic_cast<ui_stack_panel*>(widget))
        return direction == ui_layout_direction::vertical ? stack_panel->get_size().y : stack_panel->get_size().x;

    return 0.0f;
}

float resolve_widget_cross_extent(ui_context& context, ui_widget* widget, ui_layout_direction direction) {
    const ui_layout_direction cross_direction = direction == ui_layout_direction::vertical
        ? ui_layout_direction::horizontal
        : ui_layout_direction::vertical;
    return resolve_widget_extent(context, widget, cross_direction);
}
}

ui_stack_panel::ui_stack_panel(std::string id)
    : ui_container_widget(std::move(id)) {
}

void ui_stack_panel::update_child_layout(ui_context& context) {
    auto section = frame_profiler::measure_active("ui_layout_update_child_layout");

    auto& children = get_children();
    glm::vec2 cursor(get_padding().left, get_padding().top);
    const float available_width = std::max(size_.x - get_padding().left - get_padding().right, 0.0f);
    const float available_height = std::max(size_.y - get_padding().top - get_padding().bottom, 0.0f);
    const float available_main = direction_ == ui_layout_direction::vertical ? available_height : available_width;
    float total_fixed_main = 0.0f;
    float total_weight = 0.0f;
    size_t visible_children = 0u;

    for (auto& child : children) {
        if (!child || !child->is_visible())
            continue;

        ++visible_children;
        total_weight += std::max(child->get_layout_weight(), 0.0f);
        total_fixed_main += resolve_widget_extent(context, child.get(), direction_);
    }

    if (visible_children > 1u)
        total_fixed_main += static_cast<float>(visible_children - 1u) * spacing_;

    const float remaining_main = std::max(available_main - total_fixed_main, 0.0f);

    for (auto& child : children) {
        if (!child || !child->is_visible())
            continue;

        if (total_weight > 0.0f && child->get_layout_weight() > 0.0f) {
            const float allocated_main = remaining_main * (child->get_layout_weight() / total_weight) + resolve_widget_extent(context, child.get(), direction_);
            set_widget_main_extent(child.get(), direction_, allocated_main);
        }

        apply_fill_to_child(child.get(), direction_, available_width, available_height);
        child->set_position(cursor.x, cursor.y, 0.0f);
        const float extent = resolve_widget_extent(context, child.get(), direction_);
        if (direction_ == ui_layout_direction::vertical)
            cursor.y += extent + spacing_;
        else
            cursor.x += extent + spacing_;
    }
}

glm::vec2 ui_stack_panel::measure(ui_context& context) {
    auto section = frame_profiler::measure_active("ui_layout_measure_stack_panel");

    auto& children = get_children();
    float main_extent = 0.0f;
    float cross_extent = 0.0f;
    size_t visible_children = 0u;

    for (auto& child : children) {
        if (!child || !child->is_visible())
            continue;

        ++visible_children;
        main_extent += resolve_widget_extent(context, child.get(), direction_);
        cross_extent = std::max(cross_extent, resolve_widget_cross_extent(context, child.get(), direction_));
    }

    if (visible_children > 1u)
        main_extent += static_cast<float>(visible_children - 1u) * spacing_;

    if (direction_ == ui_layout_direction::vertical) {
        return glm::vec2(
            cross_extent + get_padding().left + get_padding().right,
            main_extent + get_padding().top + get_padding().bottom);
    }

    return glm::vec2(
        main_extent + get_padding().left + get_padding().right,
        cross_extent + get_padding().top + get_padding().bottom);
}

void ui_stack_panel::update_auto_size(ui_context& context) {
    if (!auto_size_enabled_)
        return;

    size_ = measure(context);
}

void ui_stack_panel::render(ui_context& context) {
    if (!is_visible())
        return;

    update_auto_size(context);
    update_child_layout(context);

    context.pipeline.draw_panel_with_children(context.engine_instance,
        ui_render_pipeline::panel_desc{
            get_ui_offset(),
            size_,
            color_,
            get_padding(),
            get_scene_anchor(),
            get_anchor_offset()
        },
        [&]() {
            render_children(context);
        });
}
