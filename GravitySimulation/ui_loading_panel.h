#pragma once

#include "ui_panel.h"

class ui_label_widget;
class ui_progress_bar_widget;
class ui_stack_panel;

class ui_loading_panel : public ui_panel
{
    ui_stack_panel* content_stack_ = nullptr;
    ui_label_widget* title_label_ = nullptr;
    ui_label_widget* resources_value_label_ = nullptr;
    ui_progress_bar_widget* progress_bar_ = nullptr;
    ui_label_widget* status_label_ = nullptr;

public:
    explicit ui_loading_panel(std::string id = "ui.loading.panel");

    void render(ui_context& context) override;
};
