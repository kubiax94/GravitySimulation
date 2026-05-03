#include "ui_panel.h"
#include "ui_render_pipeline.h"

ui_panel::ui_panel(std::string id)
    : ui_container_widget(std::move(id)) {
}

void ui_panel::render(ui_context& context) {
    if (!is_visible())
        return;

    context.pipeline.draw_panel_with_children(context.engine_instance,
        ui_render_pipeline::panel_desc{
            get_ui_offset(),
            size_,
            color_,
            get_padding(),
            get_scene_anchor(),
            get_anchor_offset(),
            false
        },
        [&]() {
            render_children(context);
        });
}
