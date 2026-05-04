#include "galactic_stress_test.h"

#include "g_shape.h"

#include <random>

namespace {
    float random_float(float min, float max) {
        static std::mt19937 gen(std::random_device{}());
        std::uniform_real_distribution<float> dist(min, max);
        return dist(gen);
    }
}

std::vector<physics_data> galactic_stress_test::create_stress_particles(int count) {
    unit_system u_sys(1e24f, 1e6f, 3.872e6f / 3600.f);
    std::vector<physics_data> particles;
    particles.reserve(static_cast<size_t>(count) + 1);
    std::mt19937 gen(std::random_device{}());
    std::uniform_real_distribution<float> dist_unit(0.f, 1.f);
    std::uniform_real_distribution<float> dist_arm_offset(-0.22f, 0.22f);
    std::uniform_real_distribution<float> dist_thickness(-55.f, 55.f);
    std::uniform_real_distribution<float> dist_mass_e24(0.05f, 9.0f);
    std::uniform_real_distribution<float> dist_speed_scale(1.08f, 1.32f);
    std::uniform_real_distribution<float> dist_vertical_velocity(-0.035f, 0.035f);
    std::uniform_real_distribution<float> dist_radial_velocity(-0.015f, 0.015f);
    std::uniform_int_distribution<int> dist_arm(0, 3);

    constexpr int arm_count = 4;
    constexpr float min_radius = 140.f;
    constexpr float max_radius = 2200.f;
    constexpr float spiral_twist = 0.0075f;
    constexpr float tau = 6.28318530718f;
    const float core_mass = u_sys.mass(8.5e30f);
    particles.push_back(physics_data(
        glm::vec4(0.f, 0.f, 0.f, core_mass),
        glm::vec4(0.f, 0.f, 0.f, core_mass),
        glm::vec4(0.f, 0.f, 0.f, core_mass)));

    for (int i = 0; i < count; ++i) {
        const float radius = min_radius + std::sqrt(dist_unit(gen)) * (max_radius - min_radius);
        const float base_angle = (static_cast<float>(dist_arm(gen)) / static_cast<float>(arm_count)) * tau;
        const float angle = base_angle + radius * spiral_twist + dist_arm_offset(gen);
        const float height = dist_thickness(gen) * (0.35f + 0.65f * radius / max_radius);
        const float body_mass_e24 = dist_mass_e24(gen);
        const float body_mass = u_sys.mass(body_mass_e24 * 1e24f);

        const glm::vec3 radial_direction(std::cos(angle), 0.f, std::sin(angle));
        const glm::vec3 tangent_direction(-radial_direction.z, 0.f, radial_direction.x);
        const glm::vec3 position = radial_direction * radius + glm::vec3(0.f, height, 0.f);
        const float orbital_speed = std::sqrt(std::max(u_sys.scaled_G() * core_mass / std::max(radius, min_radius), 0.0f));
        const glm::vec3 velocity = tangent_direction * orbital_speed * dist_speed_scale(gen)
            + radial_direction * dist_radial_velocity(gen)
            + glm::vec3(0.f, dist_vertical_velocity(gen), 0.f);

        particles.push_back(physics_data(
            glm::vec4(position, body_mass),
            glm::vec4(velocity, body_mass),
            glm::vec4(0.f)));
    }

    return particles;
}

void galactic_stress_test::initialize_stress_scene(scene* scene_to_initialize, std::vector<renderer*>& planet_renderers, int count) {
    auto& assets = scene_to_initialize->get_asset_manager();
    shader* planet_shader = assets.create_shader("stress.cpu.planets", "GravitySimulation/camera.vs.shader", "GravitySimulation/camera.fs.shader");

    auto tmp = g_shape::generate_sphere();
    auto* sphere_mesh_data = new MeshData();
    *sphere_mesh_data = tmp;
    auto* sphere_mesh = assets.create_mesh(*sphere_mesh_data);

    auto particles = create_stress_particles(count);
    for (size_t i = 0; i < particles.size(); ++i) {
        auto* node = scene_to_initialize->create_scene_node("stress_body_" + std::to_string(i));
        auto* p = new physics_data(particles[i]);
        node->add_component<rigid_body>(node, p);
        auto* render = node->add_component<renderer>(node, planet_shader, sphere_mesh);
        render->set_gpu_driven_positions(false);
        planet_renderers.push_back(render);
        node->set_global_position(p->position);
        node->set_global_scale(i == 0 ? glm::vec3(14.f) : glm::vec3(1.55f));
    }
}
