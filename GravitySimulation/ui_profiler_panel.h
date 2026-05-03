#pragma once

#include <vector>

#include "ui_panel.h"

class ui_button_widget;
class ui_label_widget;
class ui_metric_row_widget;
class ui_stack_panel;

class ui_profiler_panel : public ui_panel
{
    bool visible_body_ = true;
    float body_scroll_offset_ = 0.0f;
    bool scroll_thumb_drag_active_ = false;
    bool previous_left_mouse_down_ = false;
    float scroll_thumb_drag_offset_ = 0.0f;
    ui_stack_panel* content_stack_ = nullptr;
    ui_stack_panel* header_stack_ = nullptr;
    ui_stack_panel* body_stack_ = nullptr;
    ui_label_widget* title_label_ = nullptr;
    ui_button_widget* toggle_button_ = nullptr;
    ui_label_widget* interval_label_ = nullptr;
    ui_label_widget* cpu_title_label_ = nullptr;
    ui_stack_panel* cpu_stack_ = nullptr;
    ui_label_widget* gpu_title_label_ = nullptr;
    ui_stack_panel* gpu_stack_ = nullptr;
    ui_label_widget* ui_title_label_ = nullptr;
    ui_stack_panel* ui_stack_ = nullptr;
    ui_label_widget* values_title_label_ = nullptr;
    ui_stack_panel* values_stack_ = nullptr;
    std::vector<ui_metric_row_widget*> cpu_row_widgets_;
    std::vector<ui_metric_row_widget*> gpu_row_widgets_;
    std::vector<ui_metric_row_widget*> ui_row_widgets_;
    std::vector<ui_metric_row_widget*> value_row_widgets_;

    void ensure_metric_rows(size_t count,
        const std::string& id_prefix,
        ui_stack_panel* parent_stack,
        std::vector<ui_metric_row_widget*>& row_widgets);
    void hide_metric_rows(std::vector<ui_metric_row_widget*>& row_widgets);
    void update_body_scroll(ui_context& context, float header_height, float viewport_height, float content_height);
    void draw_body_scroll_indicator(ui_context& context, float panel_width, float header_height, float viewport_height, float content_height) const;

public:
    explicit ui_profiler_panel(std::string id = "ui.profiler.panel");

    [[nodiscard]] float get_body_scroll_offset() const { return body_scroll_offset_; }
    [[nodiscard]] glm::vec2 get_panel_top_left() const { return get_ui_offset(); }
    void render(ui_context& context) override;
};
