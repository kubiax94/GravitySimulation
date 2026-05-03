#pragma once

#include <memory>
#include <vector>

#include "ui_layout.h"
#include "ui_widget.h"

class ui_container_widget : public ui_widget
{
    std::vector<std::unique_ptr<ui_widget>> children_;
    ui_spacing padding_ = ui_spacing(0.0f);

protected:
    void render_children(ui_context& context);

public:
    explicit ui_container_widget(std::string id = {});

    void set_padding(const ui_spacing& padding) { padding_ = padding; }
    [[nodiscard]] const ui_spacing& get_padding() const { return padding_; }

    ui_widget* add_child(std::unique_ptr<ui_widget> child);
    void clear_children();
    [[nodiscard]] const std::vector<std::unique_ptr<ui_widget>>& get_children() const { return children_; }
    std::vector<std::unique_ptr<ui_widget>>& get_children() { return children_; }
};
