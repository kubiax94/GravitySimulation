#include "ui_widget.h"

#include <glm/gtx/euler_angles.hpp>

namespace {
const glm::mat4 identity_matrix = glm::mat4(1.0f);
}

ui_widget::ui_widget(std::string id)
    : id_(std::move(id)) {
}

glm::vec2 ui_widget::get_ui_offset() const {
    const auto& position = transform_.GetPosition();
    const glm::vec2 local_offset(position.x, position.y);
    if (parent_widget_)
        return parent_widget_->get_ui_offset() + local_offset;

    return local_offset;
}

glm::vec2 ui_widget::get_anchor_offset() const {
    return glm::vec2(0.0f);
}

const i_transformable* ui_widget::get_scene_anchor() const {
    if (scene_anchor_)
        return scene_anchor_;

    return parent_widget_ ? parent_widget_->get_scene_anchor() : nullptr;
}

const glm::mat4& ui_widget::get_parent_anchor_matrix() const {
    if (parent_widget_)
        return parent_widget_->get_global_matrix_model();

    return scene_anchor_ ? scene_anchor_->get_global_matrix_model() : identity_matrix;
}

void ui_widget::set_parent_widget(const ui_widget* parent_widget) {
    parent_widget_ = parent_widget;
    invalidate_cached_transform();
}

void ui_widget::invalidate_cached_transform() const {
    global_model_matrix_ = glm::mat4(1.0f);
}

void ui_widget::attach_to_scene_anchor(const i_transformable* anchor) {
    scene_anchor_ = anchor;
    invalidate_cached_transform();
}

void ui_widget::detach_scene_anchor() {
    scene_anchor_ = nullptr;
    invalidate_cached_transform();
}

glm::vec3 ui_widget::forward() const {
    return transform_.forward();
}

const glm::vec3& ui_widget::forward_local() const {
    return transform_.forward();
}

glm::vec3 ui_widget::right() const {
    return transform_.right();
}

const glm::vec3& ui_widget::right_local() const {
    return transform_.right();
}

glm::vec3 ui_widget::up() const {
    return transform_.up();
}

const glm::vec3& ui_widget::up_local() const {
    return transform_.up();
}

const glm::vec3& ui_widget::get_position() const {
    return transform_.GetPosition();
}

glm::vec3 ui_widget::get_global_position() const {
    const glm::mat4 model = get_parent_anchor_matrix() * transform_.get_local_model_matrix();
    return glm::vec3(model[3]);
}

const glm::vec3& ui_widget::get_rotation() const {
    return transform_.GetRotation();
}

glm::vec3 ui_widget::get_global_rotation() {
    const glm::mat4 model = get_parent_anchor_matrix() * transform_.get_local_model_matrix();
    return glm::degrees(glm::eulerAngles(glm::quat_cast(model)));
}

const glm::vec3& ui_widget::get_scale() const {
    return transform_.GetScale();
}

glm::vec3 ui_widget::get_global_scale() const {
    const glm::mat4 model = get_parent_anchor_matrix() * transform_.get_local_model_matrix();
    return glm::vec3(
        glm::length(glm::vec3(model[0])),
        glm::length(glm::vec3(model[1])),
        glm::length(glm::vec3(model[2])));
}

const glm::mat4& ui_widget::get_global_matrix_model() const {
    global_model_matrix_ = get_parent_anchor_matrix() * transform_.get_local_model_matrix();
    return global_model_matrix_;
}

void ui_widget::set_global_position(const glm::vec3& n_pos) {
    if (parent_widget_ || scene_anchor_) {
        const glm::mat4 inverse_parent = glm::inverse(get_parent_anchor_matrix());
        const glm::vec4 local = inverse_parent * glm::vec4(n_pos, 1.0f);
        transform_.setPosition(glm::vec3(local));
    }
    else {
        transform_.setPosition(n_pos);
    }

    invalidate_cached_transform();
}

void ui_widget::set_position(const glm::vec3& n_pos) {
    transform_.setPosition(n_pos);
    invalidate_cached_transform();
}

void ui_widget::set_position(const float& x, const float& y, const float& z) {
    set_position(glm::vec3(x, y, z));
}

void ui_widget::set_global_rotation(const glm::vec3& global_euler_deg) {
    transform_.setRotation(global_euler_deg);
    invalidate_cached_transform();
}

void ui_widget::set_rotation(const glm::vec3& n_rot) {
    transform_.setRotation(n_rot);
    invalidate_cached_transform();
}

void ui_widget::set_rotation(const float& x, const float& y, const float& z) {
    set_rotation(glm::vec3(x, y, z));
}

void ui_widget::set_global_scale(const glm::vec3& scalar) {
    transform_.setScale(scalar);
    invalidate_cached_transform();
}

void ui_widget::set_scale(const float& x) {
    set_scale(glm::vec3(x));
}

void ui_widget::set_scale(const glm::vec3& n_sc) {
    transform_.setScale(n_sc);
    invalidate_cached_transform();
}

void ui_widget::set_scale(const float& x, const float& y, const float& z) {
    set_scale(glm::vec3(x, y, z));
}
