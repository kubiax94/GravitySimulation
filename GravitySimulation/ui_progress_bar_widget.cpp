#include "ui_progress_bar_widget.h"

#include <algorithm>

#include "ui_render_pipeline.h"

ui_progress_bar_widget::ui_progress_bar_widget(std::string id)
    : ui_widget(std::move(id)) {
}

void ui_progress_bar_widget::set_progress(float progress) {
    progress_ = std::clamp(progress, 0.0f, 1.0f);
}

void ui_progress_bar_widget::render(ui_context& context) {
    if (!is_visible() || size_.x <= 0.0f || size_.y <= 0.0f)
        return;

    context.pipeline.draw_panel_with_children(context.engine_instance,
        ui_render_pipeline::panel_desc{
            get_ui_offset(),
            size_,
            background_color_,
            ui_spacing(0.0f),
            get_scene_anchor(),
            get_anchor_offset()
        },
        [&]() {
            const glm::vec2 inner_size = glm::max(size_ - fill_inset_ * 2.0f, glm::vec2(0.0f));
            const glm::vec2 fill_size(inner_size.x * progress_, inner_size.y);
            if (fill_size.x <= 0.0f || fill_size.y <= 0.0f)
                return;

            context.pipeline.draw_panel_with_children(context.engine_instance,
                ui_render_pipeline::panel_desc{
                    get_ui_offset() + fill_inset_,
                    fill_size,
                    fill_color_,
                    ui_spacing(0.0f),
                    get_scene_anchor(),
                    get_anchor_offset()
                },
                {});
        });
}
