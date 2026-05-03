#include "ui_container_widget.h"

ui_container_widget::ui_container_widget(std::string id)
    : ui_widget(std::move(id)) {
}

ui_widget* ui_container_widget::add_child(std::unique_ptr<ui_widget> child) {
    if (!child)
        return nullptr;

    child->set_parent_widget(this);
    ui_widget* child_ptr = child.get();
    children_.push_back(std::move(child));
    return child_ptr;
}

void ui_container_widget::clear_children() {
    for (auto& child : children_) {
        if (child)
            child->set_parent_widget(nullptr);
    }

    children_.clear();
}

void ui_container_widget::render_children(ui_context& context) {
    for (auto& child : children_) {
        if (child)
            child->render(context);
    }
}
