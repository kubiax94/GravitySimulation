#pragma once

#include "ray_cast.h"
#include <cstdint>
#include "transformable.h"
#include "bounding_box.h"
#include "spatial_query.h"

class collider : public transformable
{
protected:
    bounding_box local_bounds_;
    mutable bounding_box world_bounds_cache_;
    mutable uint64_t world_bounds_revision_ = 0;
    bool enabled_ = true;
    bool is_trigger_ = false;
    bool auto_generated_ = false;

public:
    static type_id_t type_id();

    explicit collider(scene_node* node)
        : transformable(node) {
    }

    collider(scene_node* node, i_transformable* transformable)
        : transformable(node, transformable) {
    }

    type_id_t get_type_id() const override;
    void append_association_type_ids(std::vector<type_id_t>& type_ids) const override;
    void attach_to(scene_node* n_node) override;
    bool detach() override;

  void set_local_bounds(const bounding_box& bounds);
    [[nodiscard]] virtual bounding_box get_local_bounds() const;
    [[nodiscard]] virtual bounding_box get_world_bounds() const;
    [[nodiscard]] bool raycast(const world_ray& ray, ray_cast_hit& hit) const;

 void set_enabled(bool enabled);
    [[nodiscard]] bool is_enabled() const;
    void set_trigger(bool is_trigger);
    [[nodiscard]] bool is_trigger() const;
  [[nodiscard]] bool is_solid() const;
  void set_auto_generated(bool auto_generated);
    [[nodiscard]] bool is_auto_generated() const;
};

inline type_id_t collider::type_id() {
    return ::get_type_id<collider>();
}

inline type_id_t collider::get_type_id() const {
    return type_id();
}

inline void collider::append_association_type_ids(std::vector<type_id_t>& type_ids) const {
    component::append_association_type_ids(type_ids);
    type_ids.push_back(collider::type_id());
}

inline void collider::attach_to(scene_node* n_node) {
    transformable::attach_to(n_node);
    if (auto* s_manager = n_node ? n_node->get_scene_manager() : nullptr)
        s_manager->register_in(this);
}

inline bool collider::detach() {
    if (auto* node = get_node()) {
        if (auto* s_manager = node->get_scene_manager())
            s_manager->register_out(this);
    }

    return transformable::detach();
}

inline void collider::set_local_bounds(const bounding_box& bounds) {
    local_bounds_ = bounds;
}

inline bounding_box collider::get_local_bounds() const {
    return local_bounds_;
}

inline bounding_box collider::get_world_bounds() const {
    const auto* transform = get_transform();
    if (!transform)
        return local_bounds_;

    const auto* node = get_node();
    if (!node)
        return local_bounds_;

    const uint64_t revision = node->get_transform_revision();
    if (world_bounds_revision_ != revision) {
        world_bounds_cache_ = transform_bounding_box(local_bounds_, transform->get_global_matrix_model());
        world_bounds_revision_ = revision;
    }

    return world_bounds_cache_;
}

inline bool collider::raycast(const world_ray& ray, ray_cast_hit& hit) const {
    if (!enabled_)
        return false;

    const bounding_box world_bounds = get_world_bounds();
    float hit_t_min = 0.0f;
    float hit_t_max = 0.0f;
    if (!intersect_ray_aabb(ray, world_bounds, hit_t_min, hit_t_max))
        return false;

    hit.node = get_node();
    hit.collider_component = const_cast<collider*>(this);
    hit.render = nullptr;
    hit.bounds = world_bounds;
    hit.distance = std::max(hit_t_min, 0.0f);
    hit.point = ray.origin + ray.direction * hit.distance;
    return true;
}

inline void collider::set_enabled(bool enabled) {
    enabled_ = enabled;
}

inline bool collider::is_enabled() const {
    return enabled_;
}

inline void collider::set_trigger(bool is_trigger) {
    is_trigger_ = is_trigger;
}

inline bool collider::is_trigger() const {
    return is_trigger_;
}

inline bool collider::is_solid() const {
    return !is_trigger_;
}

inline void collider::set_auto_generated(bool auto_generated) {
    auto_generated_ = auto_generated;
}

inline bool collider::is_auto_generated() const {
    return auto_generated_;
}

inline bool intersects(const collider& lhs, const collider& rhs, bounding_box* overlap_bounds = nullptr) {
    if (!lhs.is_enabled() || !rhs.is_enabled())
        return false;

    const bounding_box lhs_bounds = lhs.get_world_bounds();
    const bounding_box rhs_bounds = rhs.get_world_bounds();
    if (!lhs_bounds.valid || !rhs_bounds.valid)
        return false;

    const glm::vec3 overlap_min = glm::max(lhs_bounds.min, rhs_bounds.min);
    const glm::vec3 overlap_max = glm::min(lhs_bounds.max, rhs_bounds.max);
    const bool has_overlap = overlap_min.x <= overlap_max.x
        && overlap_min.y <= overlap_max.y
        && overlap_min.z <= overlap_max.z;

    if (!has_overlap)
        return false;

    if (overlap_bounds) {
        overlap_bounds->min = overlap_min;
        overlap_bounds->max = overlap_max;
        overlap_bounds->valid = true;
    }

    return true;
}
