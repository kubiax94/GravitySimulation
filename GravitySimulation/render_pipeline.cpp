#include "render_pipeline.h"

void render_pipeline::begin_frame() {
    items_.clear();
}

void render_pipeline::submit(renderer* render) {
    if (!render)
        return;

    items_.push_back({ render });
}

void render_pipeline::submit(const render_item& item) {
    if (!item.render)
        return;

    items_.push_back(item);
}

void render_pipeline::flush(Camera* camera, const std::function<void(shader&)>& pre_draw) {
    if (!camera)
        return;

    std::unordered_map<batch_key, std::vector<renderer*>, batch_key_hash> batches;
    for (const auto& item : items_) {
        if (!item.render || !item.render->get_shader() || !item.render->get_mesh())
            continue;

        batches[{ item.render->get_shader(), item.render->get_mesh() }].push_back(item.render);
    }

    for (auto& [key, renders] : batches) {
        if (renders.empty())
            continue;

        if (renders.size() == 1)
            renders.front()->draw(camera, pre_draw);
        else
            instance_manager_.draw_instanced(renders, camera, pre_draw);
    }
}