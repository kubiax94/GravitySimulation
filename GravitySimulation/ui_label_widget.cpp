#include "ui_label_widget.h"

#include "ui_render_pipeline.h"

ui_label_widget::ui_label_widget(std::string id)
    : ui_widget(std::move(id)) {
}

void ui_label_widget::render(ui_context& context) {
    if (!is_visible() || text_.empty())
        return;

    context.pipeline.draw_label(context.engine_instance, {
        text_,
        get_ui_offset(),
        scale_,
        color_,
        bounds_size_,
        horizontal_alignment_,
        vertical_alignment_,
        get_scene_anchor(),
        get_anchor_offset()
    });
}
