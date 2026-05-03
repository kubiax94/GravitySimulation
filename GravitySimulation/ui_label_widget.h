#pragma once

#include "ui_layout.h"
#include "ui_widget.h"

class ui_label_widget : public ui_widget
{
    std::string text_;
    float scale_ = 1.0f;
    glm::vec3 color_ = glm::vec3(1.0f);
    glm::vec2 bounds_size_ = glm::vec2(0.0f);
    ui_horizontal_alignment horizontal_alignment_ = ui_horizontal_alignment::left;
    ui_vertical_alignment vertical_alignment_ = ui_vertical_alignment::top;

public:
    explicit ui_label_widget(std::string id = {});

    void set_text(std::string text) { text_ = std::move(text); }
    const std::string& get_text() const { return text_; }

    void set_scale_value(float scale) { scale_ = scale; }
    float get_scale_value() const { return scale_; }

    void set_color(const glm::vec3& color) { color_ = color; }
    const glm::vec3& get_color() const { return color_; }

    void set_bounds_size(const glm::vec2& bounds_size) { bounds_size_ = bounds_size; }
    const glm::vec2& get_bounds_size() const { return bounds_size_; }

    void set_horizontal_alignment(ui_horizontal_alignment alignment) { horizontal_alignment_ = alignment; }
    ui_horizontal_alignment get_horizontal_alignment() const { return horizontal_alignment_; }

    void set_vertical_alignment(ui_vertical_alignment alignment) { vertical_alignment_ = alignment; }
    ui_vertical_alignment get_vertical_alignment() const { return vertical_alignment_; }

    void render(ui_context& context) override;
};
