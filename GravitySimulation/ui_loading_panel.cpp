#include "ui_loading_panel.h"

#include <iomanip>
#include <memory>
#include <sstream>

#include "ui_label_widget.h"
#include "ui_progress_bar_widget.h"
#include "ui_stack_panel.h"

ui_loading_panel::ui_loading_panel(std::string id)
    : ui_panel(std::move(id)) {
    set_position(18.0f, 420.0f, 0.0f);
    set_padding(ui_spacing(0.0f));

    auto content_stack = std::make_unique<ui_stack_panel>("ui.loading.content");
    content_stack->set_color(glm::vec4(0.0f));
    content_stack->set_padding(ui_spacing(14.0f, 12.0f, 14.0f, 12.0f));
    content_stack->set_spacing(8.0f);
    content_stack->set_layout_fill_x(true);
    content_stack_ = static_cast<ui_stack_panel*>(add_child(std::move(content_stack)));

    auto title = std::make_unique<ui_label_widget>("ui.loading.title");
    title->set_text("Runtime loading");
    title->set_scale_value(0.76f);
    title->set_color(glm::vec3(0.96f, 0.98f, 1.0f));
    title->set_horizontal_alignment(ui_horizontal_alignment::center);
    title->set_vertical_alignment(ui_vertical_alignment::center);
    title->set_layout_fill_x(true);
    title_label_ = content_stack_ ? static_cast<ui_label_widget*>(content_stack_->add_child(std::move(title))) : nullptr;

    auto resources_value = std::make_unique<ui_label_widget>("ui.loading.resources.value");
    resources_value->set_scale_value(0.68f);
    resources_value->set_color(glm::vec3(0.86f, 0.91f, 0.98f));
    resources_value->set_layout_fill_x(true);
    resources_value->set_horizontal_alignment(ui_horizontal_alignment::center);
    resources_value->set_vertical_alignment(ui_vertical_alignment::center);
    resources_value_label_ = content_stack_ ? static_cast<ui_label_widget*>(content_stack_->add_child(std::move(resources_value))) : nullptr;

    auto progress_bar = std::make_unique<ui_progress_bar_widget>("ui.loading.progress");
    progress_bar->set_background_color(glm::vec4(0.10f, 0.13f, 0.18f, 0.94f));
    progress_bar->set_fill_color(glm::vec4(0.24f, 0.58f, 0.94f, 0.98f));
    progress_bar->set_fill_inset(glm::vec2(2.0f, 2.0f));
    progress_bar->set_layout_fill_x(true);
    progress_bar_ = content_stack_ ? static_cast<ui_progress_bar_widget*>(content_stack_->add_child(std::move(progress_bar))) : nullptr;

    auto status = std::make_unique<ui_label_widget>("ui.loading.status");
    status->set_scale_value(0.68f);
    status->set_color(glm::vec3(0.82f, 0.88f, 0.96f));
    status->set_layout_fill_x(true);
    status->set_horizontal_alignment(ui_horizontal_alignment::center);
    status->set_vertical_alignment(ui_vertical_alignment::center);
    status_label_ = content_stack_ ? static_cast<ui_label_widget*>(content_stack_->add_child(std::move(status))) : nullptr;
}

void ui_loading_panel::render(ui_context& context) {
    if (!is_visible())
        return;

    const auto* loading_state = context.loading_feedback;
    if (!loading_state || !loading_state->active)
        return;

    const float panel_width = 340.0f;
    const float panel_height = 118.0f;

    set_size(glm::vec2(panel_width, panel_height));
    set_color(glm::vec4(0.04f, 0.06f, 0.10f, 0.88f));
    set_padding(ui_spacing(0.0f));

    if (content_stack_) {
        content_stack_->set_size(glm::vec2(panel_width, panel_height));
        content_stack_->set_color(glm::vec4(0.0f));
        content_stack_->set_auto_size_enabled(false);
        content_stack_->set_spacing(8.0f);
    }

    if (title_label_) {
        title_label_->set_text(loading_state->title.empty() ? "Runtime loading" : loading_state->title);
        title_label_->set_bounds_size(glm::vec2(panel_width - 28.0f, 22.0f));
    }

    if (resources_value_label_) {
        std::ostringstream value_stream;
        value_stream << loading_state->completed_resources << "/" << loading_state->total_resources
                     << " (" << loading_state->progress_text << ")";
        resources_value_label_->set_text(value_stream.str());
        resources_value_label_->set_bounds_size(glm::vec2(panel_width - 28.0f, 20.0f));
    }

    if (progress_bar_) {
        progress_bar_->set_size(glm::vec2(panel_width - 28.0f, 16.0f));
        progress_bar_->set_progress(loading_state->progress);
        progress_bar_->set_fill_color(loading_state->failed ? glm::vec4(0.94f, 0.32f, 0.32f, 0.98f) : glm::vec4(0.24f, 0.58f, 0.94f, 0.98f));
    }

    if (status_label_) {
        status_label_->set_text(loading_state->status);
        status_label_->set_color(loading_state->failed ? glm::vec3(1.0f, 0.45f, 0.45f) : glm::vec3(0.82f, 0.88f, 0.96f));
        status_label_->set_bounds_size(glm::vec2(panel_width - 28.0f, 20.0f));
    }

    ui_panel::render(context);
}
