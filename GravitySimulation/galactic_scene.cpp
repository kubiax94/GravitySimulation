#include "galactic_scene.h"

#include "galactic_simulation_test.h"

#include "Camera.h"
#include "gpu_particle_system_component.h"

#include <algorithm>
#include <random>

namespace {
constexpr glm::vec3 initial_camera_position(2250.f, 650.f, 5200.f);
constexpr glm::vec3 initial_camera_rotation(-7.f, 0.f, 0.f);
constexpr float axial_rotation_base_speed = 0.002f;
constexpr int background_star_count = 2200;
constexpr int background_galaxy_count = 42;
constexpr float background_star_min_radius = 18000.f;
constexpr float background_star_max_radius = 42000.f;
constexpr float background_galaxy_min_radius = 26000.f;
constexpr float background_galaxy_max_radius = 46000.f;

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
    "Mercury_visual_spin",
    "Venus_visual_spin",
    "Earth_visual_spin",
    "Mars_visual_spin",
    "Jupiter_visual_spin",
    "Saturn_visual_spin",
    "Uranus_visual_spin",
    "Neptune_visual_spin"
};

MeshData create_particle_point_mesh() {
    MeshData data;
    Vertex vertex{};
    vertex.Position = glm::vec3(0.f);
    vertex.Normal = glm::vec3(0.f, 1.f, 0.f);
    data.vertecies.push_back(vertex);
    data.indices = { 0u };
    return data;
}

std::vector<physics_data> create_background_particles(int count, float min_radius, float max_radius, uint32_t seed, float band_bias) {
    std::vector<physics_data> particles;
    particles.reserve(static_cast<size_t>(count));

    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> unit_dist(0.f, 1.f);
    std::uniform_real_distribution<float> angle_dist(0.f, glm::two_pi<float>());

    for (int i = 0; i < count; ++i) {
        const float band_mix = unit_dist(rng);
        const float z = glm::mix(
            unit_dist(rng) * 2.0f - 1.0f,
            glm::clamp((unit_dist(rng) * 2.0f - 1.0f) * band_bias, -1.0f, 1.0f),
            glm::clamp(band_mix * 0.75f + 0.25f, 0.0f, 1.0f));
        const float azimuth = angle_dist(rng);
        const float xy = std::sqrt(glm::max(0.0f, 1.0f - z * z));
        const glm::vec3 normal(
            std::cos(azimuth) * xy,
            z,
            std::sin(azimuth) * xy);
        const float radius = glm::mix(min_radius, max_radius, std::pow(unit_dist(rng), 0.82f));

        physics_data particle;
        particle.position = glm::vec4(normal * radius, 1.0f);
        particle.velocity = glm::vec4(0.f, 0.f, 0.f, 1.0f);
        particle.accumulated_force = glm::vec4(0.f, 0.f, 0.f, 1.0f);
        particles.push_back(particle);
    }

    return particles;
}
}

galactic_scene::galactic_scene(sim::time* time)
    : scene(time) {
    initialize_scene_content();
}

void galactic_scene::initialize_scene_content() {
    set_simulation_speed(3600.f);

    auto& assets = get_asset_manager();

    camera_node_ = create_scene_node("galactic_cam");
    camera_node_->add_component<Camera>(camera_node_);
    camera_node_->set_global_position(initial_camera_position);
    camera_node_->set_global_rotation(initial_camera_rotation);

    simtest::init_gravity_test(this, planet_renderers_);

    background_star_node_ = create_scene_node("galactic_background_stars");
    background_galaxy_node_ = create_scene_node("galactic_background_galaxies");
    static MeshData particle_data = create_particle_point_mesh();
    auto* particle_mesh = assets.create_mesh(particle_data);
    particle_mesh->type = MeshType::POINTS;
    auto* particle_shader = assets.create_shader(
        "galactic.background.particles",
        "GravitySimulation/gpu_particle_system.vs.shader",
        "GravitySimulation/gpu_particle_system.fs.shader");
    auto* background_compute_stars = assets.create_compute_shader(
        "galactic.background.compute.stars",
        "GravitySimulation/cosmic_background_particles.glsl");
    auto* background_compute_galaxies = assets.create_compute_shader(
        "galactic.background.compute.galaxies",
        "GravitySimulation/cosmic_background_particles.glsl");

    auto* stars = background_star_node_->add_component<gpu_particle_system_component>(
        background_star_node_,
        background_compute_stars,
        particle_shader,
        particle_mesh,
        get_unit_system(),
        create_background_particles(background_star_count, background_star_min_radius, background_star_max_radius, 0x51A7BEEFu, 0.26f),
        1.45f,
        1.0f);
    stars->set_particle_color(glm::vec3(0.96f, 0.98f, 1.0f));
    stars->set_particle_alpha(0.88f);
    stars->set_particle_glow_strength(1.15f);
    stars->set_particle_size_jitter(1.0f);
    stars->set_particle_visual_mode(1);

    auto* galaxies = background_galaxy_node_->add_component<gpu_particle_system_component>(
        background_galaxy_node_,
        background_compute_galaxies,
        particle_shader,
        particle_mesh,
        get_unit_system(),
        create_background_particles(background_galaxy_count, background_galaxy_min_radius, background_galaxy_max_radius, 0x0A11CE42u, 0.68f),
        18.0f,
        1.0f);
    galaxies->set_particle_color(glm::vec3(0.78f, 0.86f, 1.0f));
    galaxies->set_particle_alpha(0.34f);
    galaxies->set_particle_glow_strength(1.45f);
    galaxies->set_particle_size_jitter(0.35f);
    galaxies->set_particle_visual_mode(2);
}

void galactic_scene::update() {
    if (camera_node_) {
        const glm::vec3 camera_position = camera_node_->get_global_position();
        if (background_star_node_)
            background_star_node_->set_global_position(camera_position);
        if (background_galaxy_node_)
            background_galaxy_node_->set_global_position(camera_position);
    }

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
