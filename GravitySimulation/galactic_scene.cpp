#include "galactic_scene.h"

#include "galactic_simulation_test.h"

#include "Camera.h"

#include <algorithm>

namespace {
constexpr glm::vec3 initial_camera_position(2250.f, 650.f, 5200.f);
constexpr glm::vec3 initial_camera_rotation(-7.f, 0.f, 0.f);
constexpr float axial_rotation_base_speed = 0.002f;

constexpr float planet_spin_speed_multipliers[] = {
    0.017f,
    -0.004f,
    1.0f,
    0.973f,
    2.414f,
    2.245f,
    -1.392f,
    1.49f
};

constexpr const char* planet_spin_node_names[] = {
    "Mercury",
    "Venus",
    "Earth",
    "Mars",
    "Jupiter",
    "Saturn",
    "Uranus",
    "Neptune"
};
}

galactic_scene::galactic_scene(sim::time* time)
    : scene(time) {
    initialize_scene_content();
}

void galactic_scene::initialize_scene_content() {
    set_simulation_speed(3600.f);

    auto* cam_node = create_scene_node("galactic_cam");
    cam_node->add_component<Camera>(cam_node);
    cam_node->set_global_position(initial_camera_position);
    cam_node->set_global_rotation(initial_camera_rotation);

    simtest::init_gravity_test(this, planet_renderers_);
}

void galactic_scene::update() {
    scene::update();

    constexpr float fixed_dt = 1.f / 60.f;
    const float rotation_step = fixed_dt * get_simulation_speed() * axial_rotation_base_speed;
    const size_t count = std::min(std::size(planet_spin_node_names), std::size(planet_spin_speed_multipliers));
    for (size_t i = 0; i < count; ++i) {
        auto* node = find_scene_node(planet_spin_node_names[i]);
        if (!node)
            continue;

        glm::vec3 rotation = node->get_rotation();
        rotation.y += rotation_step * planet_spin_speed_multipliers[i];
        node->set_rotation(rotation);
    }
}
