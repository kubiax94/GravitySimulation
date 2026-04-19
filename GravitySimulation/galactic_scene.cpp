#include "galactic_scene.h"

#include "galactic_simulation_test.h"

#include "Camera.h"

namespace {
constexpr float camera_height = 220.f;
constexpr float camera_distance = 520.f;
}

galactic_scene::galactic_scene(sim::time* time)
    : scene(time) {
    initialize_scene_content();
}

void galactic_scene::initialize_scene_content() {
    auto* cam_node = create_scene_node("galactic_cam");
    cam_node->add_component<Camera>(cam_node);
    cam_node->set_global_position(glm::vec3(0.f, camera_height, camera_distance));
    cam_node->set_global_rotation(glm::vec3(-18.f, 0.f, 0.f));

    simtest::init_gravity_test(this, planet_renderers_);
}
