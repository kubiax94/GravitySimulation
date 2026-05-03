#pragma once

#include <functional>

#include "ui_layout.h"
#include "ui_widget.h"

class ui_button_widget : public ui_widget
{
    std::string text_;
    glm::vec2 size_ = glm::vec2(0.0f);
    glm::vec4 color_ = glm::vec4(0.16f, 0.19f, 0.26f, 0.92f);
    glm::vec4 hover_color_ = glm::vec4(0.23f, 0.28f, 0.38f, 0.96f);
    glm::vec3 text_color_ = glm::vec3(0.96f, 0.98f, 1.0f);
    ui_spacing padding_ = ui_spacing(12.0f, 10.0f);
    std::function<void()> on_click_;

public:
    explicit ui_button_widget(std::string id = {});

    void set_text(std::string text) { text_ = std::move(text); }
    const std::string& get_text() const { return text_; }

    void set_size(const glm::vec2& size) { size_ = size; }
    const glm::vec2& get_size() const { return size_; }

    void set_color(const glm::vec4& color) { color_ = color; }
    void set_hover_color(const glm::vec4& color) { hover_color_ = color; }
    void set_text_color(const glm::vec3& color) { text_color_ = color; }
    void set_padding(const ui_spacing& padding) { padding_ = padding; }
    void set_on_click(std::function<void()> on_click) { on_click_ = std::move(on_click); }

    void render(ui_context& context) override;
};
