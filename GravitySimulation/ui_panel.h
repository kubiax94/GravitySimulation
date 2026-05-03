#pragma once

#include <memory>

#include "ui_layout.h"
#include "ui_container_widget.h"

class ui_panel : public ui_container_widget
{
    glm::vec2 size_ = glm::vec2(0.0f);
    glm::vec4 color_ = glm::vec4(0.08f, 0.10f, 0.14f, 0.82f);

public:
    explicit ui_panel(std::string id = {});

    void set_size(const glm::vec2& size) { size_ = size; }
    [[nodiscard]] const glm::vec2& get_size() const { return size_; }

    void set_color(const glm::vec4& color) { color_ = color; }
    [[nodiscard]] const glm::vec4& get_color() const { return color_; }

    void render(ui_context& context) override;
};
