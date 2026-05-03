#pragma once

#include "ui_stack_panel.h"

class ui_label_widget;

class ui_metric_row_widget : public ui_stack_panel
{
    ui_label_widget* label_widget_ = nullptr;
    ui_label_widget* value_widget_ = nullptr;
    glm::vec2 size_ = glm::vec2(0.0f);
    std::string label_text_;
    std::string value_text_;
    float scale_ = 1.0f;
    glm::vec3 label_color_ = glm::vec3(1.0f);
    glm::vec3 value_color_ = glm::vec3(1.0f);
    float column_gap_ = 12.0f;
    float label_width_ratio_ = 0.56f;

public:
    explicit ui_metric_row_widget(std::string id = {});

    void set_size(const glm::vec2& size) { size_ = size; }
    [[nodiscard]] const glm::vec2& get_size() const { return size_; }

    void set_label(std::string label) { label_text_ = std::move(label); }
    [[nodiscard]] const std::string& get_label() const { return label_text_; }

    void set_value(std::string value) { value_text_ = std::move(value); }
    [[nodiscard]] const std::string& get_value() const { return value_text_; }

    void set_scale_value(float scale) { scale_ = scale; }
    [[nodiscard]] float get_scale_value() const { return scale_; }

    void set_label_color(const glm::vec3& color) { label_color_ = color; }
    [[nodiscard]] const glm::vec3& get_label_color() const { return label_color_; }

    void set_value_color(const glm::vec3& color) { value_color_ = color; }
    [[nodiscard]] const glm::vec3& get_value_color() const { return value_color_; }

    void set_column_gap(float column_gap) { column_gap_ = column_gap; }
    [[nodiscard]] float get_column_gap() const { return column_gap_; }

    void set_label_width_ratio(float ratio) { label_width_ratio_ = ratio; }
    [[nodiscard]] float get_label_width_ratio() const { return label_width_ratio_; }

    void render(ui_context& context) override;
};
