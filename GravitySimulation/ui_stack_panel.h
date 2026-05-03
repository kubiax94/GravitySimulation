#pragma once

#include "ui_container_widget.h"

class ui_stack_panel : public ui_container_widget
{
    glm::vec2 size_ = glm::vec2(0.0f);
    glm::vec4 color_ = glm::vec4(0.08f, 0.10f, 0.14f, 0.82f);
    ui_layout_direction direction_ = ui_layout_direction::vertical;
    float spacing_ = 0.0f;
    bool auto_size_enabled_ = false;

    void update_child_layout(ui_context& context);
    void update_auto_size(ui_context& context);

public:
    explicit ui_stack_panel(std::string id = {});

    void set_size(const glm::vec2& size) { size_ = size; }
    [[nodiscard]] const glm::vec2& get_size() const { return size_; }

    void set_color(const glm::vec4& color) { color_ = color; }
    [[nodiscard]] const glm::vec4& get_color() const { return color_; }

    void set_direction(ui_layout_direction direction) { direction_ = direction; }
    [[nodiscard]] ui_layout_direction get_direction() const { return direction_; }

    void set_spacing(float spacing) { spacing_ = spacing; }
    [[nodiscard]] float get_spacing() const { return spacing_; }

    void set_auto_size_enabled(bool enabled) { auto_size_enabled_ = enabled; }
    [[nodiscard]] bool is_auto_size_enabled() const { return auto_size_enabled_; }

    [[nodiscard]] glm::vec2 measure(ui_context& context);

    void render(ui_context& context) override;
};
