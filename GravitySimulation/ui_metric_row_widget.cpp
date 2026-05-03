#include "ui_metric_row_widget.h"

#include <algorithm>
#include <memory>

#include "frame_profiler.h"
#include "ui_label_widget.h"

ui_metric_row_widget::ui_metric_row_widget(std::string id)
    : ui_stack_panel(std::move(id)) {
    set_color(glm::vec4(0.0f));
    set_direction(ui_layout_direction::horizontal);
    set_spacing(column_gap_);

    auto label = std::make_unique<ui_label_widget>(get_id() + ".label");
    label->set_horizontal_alignment(ui_horizontal_alignment::left);
    label->set_vertical_alignment(ui_vertical_alignment::center);
    label->set_layout_weight(label_width_ratio_);
    label_widget_ = static_cast<ui_label_widget*>(add_child(std::move(label)));

    auto value = std::make_unique<ui_label_widget>(get_id() + ".value");
    value->set_horizontal_alignment(ui_horizontal_alignment::right);
    value->set_vertical_alignment(ui_vertical_alignment::center);
    value->set_layout_weight(std::max(1.0f - label_width_ratio_, 0.0f));
    value_widget_ = static_cast<ui_label_widget*>(add_child(std::move(value)));
}

void ui_metric_row_widget::render(ui_context& context) {
    auto section = frame_profiler::measure_active("ui_metric_row_render");

    const float clamped_ratio = std::clamp(label_width_ratio_, 0.0f, 1.0f);
    const float label_width = std::max(size_.x * clamped_ratio, 0.0f);
    const float value_width = std::max(size_.x - label_width - column_gap_, 0.0f);

    set_size(size_);
    set_spacing(column_gap_);
    set_auto_size_enabled(false);

    if (label_widget_) {
        label_widget_->set_layout_weight(clamped_ratio);
        label_widget_->set_layout_fill_x(true);
        label_widget_->set_text(label_text_);
        label_widget_->set_scale_value(scale_);
        label_widget_->set_color(label_color_);
        label_widget_->set_bounds_size(glm::vec2(label_width, size_.y));
    }

    if (value_widget_) {
        value_widget_->set_layout_weight(std::max(1.0f - clamped_ratio, 0.0f));
        value_widget_->set_layout_fill_x(true);
        value_widget_->set_text(value_text_);
        value_widget_->set_scale_value(scale_);
        value_widget_->set_color(value_color_);
        value_widget_->set_bounds_size(glm::vec2(value_width, size_.y));
    }

    ui_stack_panel::render(context);
}
