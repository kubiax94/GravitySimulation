#pragma once

#include "collider.h"

class aabb_collider : public collider
{
    uint64_t source_revision_ = 0;

public:
    static type_id_t type_id();

    explicit aabb_collider(scene_node* node);
    aabb_collider(scene_node* node, const bounding_box& local_bounds);
    aabb_collider(scene_node* node, i_transformable* transformable, const bounding_box& local_bounds);

    type_id_t get_type_id() const override;
    void set_local_bounds(const bounding_box& bounds);
    [[nodiscard]] uint64_t get_source_revision() const;
    void set_source_revision(uint64_t revision);
};
