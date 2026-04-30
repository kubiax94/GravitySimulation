#pragma once

#include <iostream>
#include <string>
#include <utility>

#include "Component.h"

class collision_debug_logger_component final : public component
{
    std::string label_;
    mutable uint32_t stay_log_counter_ = 0u;

    void log_event(const char* phase, const collision_event& event) const {
        std::cout << "[collision][" << phase << "] " << label_ << " -> ";
        if (event.other && event.other->get_node())
            std::cout << event.other->get_node()->get_id().to_string();
        else
            std::cout << "null";

        std::cout << " trigger=" << (event.is_trigger_interaction ? 1 : 0);
        if (event.overlap_bounds.valid) {
            const glm::vec3 overlap = event.overlap_bounds.get_size();
            std::cout << " overlap=(" << overlap.x << ", " << overlap.y << ", " << overlap.z << ")";
        }
        std::cout << "\n";
    }

public:
    static type_id_t type_id() {
        return ::get_type_id<collision_debug_logger_component>();
    }

    explicit collision_debug_logger_component(scene_node* owner_node, std::string label)
        : component(owner_node), label_(std::move(label)) {
    }

    [[nodiscard]] type_id_t get_type_id() const override {
     return type_id();
    }

    void on_collision_enter(const collision_event& event) override {
        log_event("enter", event);
    }

    void on_collision_stay(const collision_event& event) override {
        ++stay_log_counter_;
        if (stay_log_counter_ % 30u == 0u)
            log_event("stay", event);
    }

    void on_collision_exit(const collision_event& event) override {
        log_event("exit", event);
    }
};
