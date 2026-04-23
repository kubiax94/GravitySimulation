#include "aabb_collider.h"

type_id_t aabb_collider::type_id() {
    return ::get_type_id<aabb_collider>();
}

aabb_collider::aabb_collider(scene_node* node)
    : collider(node) {
}

aabb_collider::aabb_collider(scene_node* node, const bounding_box& local_bounds)
    : collider(node) {
    set_local_bounds(local_bounds);
}

aabb_collider::aabb_collider(scene_node* node, i_transformable* transformable, const bounding_box& local_bounds)
    : collider(node, transformable) {
    set_local_bounds(local_bounds);
}

type_id_t aabb_collider::get_type_id() const {
    return type_id();
}

void aabb_collider::set_local_bounds(const bounding_box& bounds) {
    collider::set_local_bounds(bounds);
}

uint64_t aabb_collider::get_source_revision() const {
    return source_revision_;
}

void aabb_collider::set_source_revision(uint64_t revision) {
    source_revision_ = revision;
}
