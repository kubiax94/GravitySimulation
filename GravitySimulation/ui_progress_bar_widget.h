#pragma once

#include "ui_widget.h"

class ui_progress_bar_widget : public ui_widget
{
    glm::vec2 size_ = glm::vec2(0.0f);
    float progress_ = 0.0f;
    glm::vec4 background_color_ = glm::vec4(0.10f, 0.13f, 0.18f, 0.92f);
    glm::vec4 fill_color_ = glm::vec4(0.26f, 0.60f, 0.96f, 0.96f);
    glm::vec2 fill_inset_ = glm::vec2(2.0f, 2.0f);

public:
    explicit ui_progress_bar_widget(std::string id = {});

    void set_size(const glm::vec2& size) { size_ = size; }
    [[nodiscard]] const glm::vec2& get_size() const { return size_; }

    void set_progress(float progress);
    [[nodiscard]] float get_progress() const { return progress_; }

    void set_background_color(const glm::vec4& color) { background_color_ = color; }
    [[nodiscard]] const glm::vec4& get_background_color() const { return background_color_; }

    void set_fill_color(const glm::vec4& color) { fill_color_ = color; }
    [[nodiscard]] const glm::vec4& get_fill_color() const { return fill_color_; }

    void set_fill_inset(const glm::vec2& inset) { fill_inset_ = inset; }
    [[nodiscard]] const glm::vec2& get_fill_inset() const { return fill_inset_; }

    void render(ui_context& context) override;
};
