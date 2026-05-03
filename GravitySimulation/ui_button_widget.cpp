#include "ui_button_widget.h"

#include "ui_render_pipeline.h"

ui_button_widget::ui_button_widget(std::string id)
    : ui_widget(std::move(id)) {
}

void ui_button_widget::render(ui_context& context) {
    if (!is_visible())
        return;

    context.pipeline.draw_button(context.engine_instance, {
        get_id(),
        text_,
        get_ui_offset(),
        size_,
        color_,
        hover_color_,
        text_color_,
        padding_,
        get_scene_anchor(),
        get_anchor_offset(),
        on_click_
    });
}
