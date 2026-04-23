#include "scene_loader.h"

#include <algorithm>
#include <chrono>

scene_loader::resource_entry* scene_loader::find_entry(resource* resource_ptr) {
    for (auto& entry : resources_) {
        if (entry.resource_ptr == resource_ptr)
            return &entry;
    }

    return nullptr;
}

const scene_loader::resource_entry* scene_loader::find_entry(resource* resource_ptr) const {
    for (const auto& entry : resources_) {
        if (entry.resource_ptr == resource_ptr)
            return &entry;
    }

    return nullptr;
}

void scene_loader::reset() {
    resources_.clear();
    started_ = false;
    completed_ = false;
    failed_ = false;
}

void scene_loader::add_resource(resource& resource_ref) {
   add_resource(resource_ref, {});
}

void scene_loader::add_resource(resource& resource_ref, std::function<void(resource&)> on_finalized) {
    if (started_ || find_entry(&resource_ref) != nullptr)
        return;

    resources_.push_back({ .resource_ptr = &resource_ref, .on_finalized = std::move(on_finalized) });
}

void scene_loader::start(async_priority priority) {
    if (started_)
        return;

    started_ = true;
    completed_ = resources_.empty();
    failed_ = false;

    for (auto& entry : resources_) {
        if (!entry.resource_ptr)
            continue;

        entry.resource_ptr->execute_async(priority);
        entry.started = true;
    }
}

void scene_loader::update() {
    if (!started_ || completed_ || failed_)
        return;

    bool all_finalized = true;
    for (auto& entry : resources_) {
        if (!entry.resource_ptr || entry.failed)
            continue;

        if (!entry.started) {
            all_finalized = false;
            continue;
        }

        if (!entry.finalized) {
            const auto result = entry.resource_ptr->try_get_result(std::chrono::milliseconds::zero());
            if (!result.has_value()) {
                if (entry.resource_ptr->is_failed()) {
                    entry.failed = true;
                    failed_ = true;
                }
                all_finalized = false;
                continue;
            }

            if (!result.value() || !entry.resource_ptr->finalize()) {
                entry.failed = true;
                failed_ = true;
                all_finalized = false;
                continue;
            }

         if (entry.on_finalized)
                entry.on_finalized(*entry.resource_ptr);

            entry.finalized = true;
        }
    }

    completed_ = all_finalized && !failed_;
}

void scene_loader::cancel() {
    for (auto& entry : resources_) {
        if (entry.resource_ptr)
            entry.resource_ptr->cancel();
    }

    started_ = false;
    completed_ = false;
    failed_ = false;
}

float scene_loader::get_progress() const {
    if (resources_.empty())
        return 1.0f;

  float total_progress = 0.0f;
    for (const auto& entry : resources_) {
        if (!entry.resource_ptr)
            continue;

        if (entry.finalized)
            total_progress += 1.0f;
        else
            total_progress += std::clamp(entry.resource_ptr->get_progress(), 0.0f, 1.0f);
    }

    return total_progress / static_cast<float>(resources_.size());
}

std::size_t scene_loader::get_completed_resources() const {
    return static_cast<std::size_t>(std::count_if(resources_.begin(), resources_.end(), [](const resource_entry& entry) {
        return entry.finalized;
    }));
}
