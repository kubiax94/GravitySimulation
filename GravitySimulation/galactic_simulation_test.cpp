#include "galactic_simulation_test.h"

#include <cmath>
#include <random>

#include "Mesh.h"
#include "compute_shader.h"
#include "asset_manager.h"
#include "fluid_bounds.h"
#include "fluid_particle.h"
#include "g_shape.h"
#include "gpu_fluid_system_component.h"
#include "gpu_particle_system_component.h"
#include "planet_terrain.h"
#include "renderer.h"

struct planet_data
{
	std::string name;
	float mass = 0.f;
	float diameter = 0.f;
	float distance_to_sun = 0.f;
};
namespace simtest {

constexpr float sun_visual_radius_scale = 0.42f;
constexpr float sun_halo_scale_multiplier = 1.13f;
constexpr int sun_halo_particle_count = 960;
constexpr int planetary_ocean_particle_count = 24576;
constexpr float earth_ocean_base_radius = 1.015f;
constexpr float earth_ocean_shell_thickness = 0.075f;

float get_visual_orbit_offset(const planet_data& planet, float dia_scale) {
 const float sun_visual_radius = sun_visual_radius_scale * (1391000.f / dia_scale);
	const float sun_halo_radius = sun_visual_radius * sun_halo_scale_multiplier;
	const float planet_visual_radius = planet.diameter / dia_scale;
	return sun_halo_radius + planet_visual_radius + 12.0f;
}

float get_visual_orbital_radius(const planet_data& planet, unit_system& u_sys, float dia_scale) {
    const float scaled_distance = planet.distance_to_sun / u_sys.distance_scale;
	const float compressed_orbit_radius = 12.0f * std::sqrt(glm::max(scaled_distance, 0.0f));
	return compressed_orbit_radius + get_visual_orbit_offset(planet, dia_scale);
}

float get_initial_orbit_angle(size_t index) {
	constexpr float degrees_to_radians = 0.01745329252f;
	constexpr float start_angles_deg[] = {
		0.f,
		42.f,
		96.f,
		151.f,
		208.f,
		257.f,
		311.f,
		342.f
	};
	return start_angles_deg[index % std::size(start_angles_deg)] * degrees_to_radians;
}

MeshData create_particle_point_mesh() {
	MeshData data;
	Vertex vertex{};
	vertex.Position = glm::vec3(0.f);
	vertex.Normal = glm::vec3(0.f, 1.f, 0.f);
	data.vertecies.push_back(vertex);
	data.indices = { 0u };
	return data;
}

shader* create_rocky_planet_shader(asset_manager& assets, const planet_data& planet, bool disable_static_ocean_tint = false) {
	shader* rocky_shader = assets.create_shader(
		"galactic.planets." + planet.name,
       "GravitySimulation/rocky_planet.vs.shader",
		"GravitySimulation/rocky_planet.fs.shader");
  auto terrain_profile = planet_terrain::make_rocky_planet_profile(planet.name);
	terrain_profile.static_ocean_tint_enabled = !disable_static_ocean_tint;
	planet_terrain::apply_rocky_planet_profile(*rocky_shader, terrain_profile);
	return rocky_shader;
}

shader* create_planet_cloud_shader(
	asset_manager& assets,
	const std::string& shader_name,
	float coverage,
	float softness,
	float opacity,
	float speed,
	const glm::vec3& color,
	const glm::vec3& shadow_color) {
	shader* cloud_shader = assets.create_shader(
		shader_name,
		"GravitySimulation/lightsource.vs.shader",
		"GravitySimulation/planet_clouds.fs.shader");
	cloud_shader->use();
	cloud_shader->set_uni_float("cloudCoverage", coverage);
	cloud_shader->set_uni_float("cloudSoftness", softness);
	cloud_shader->set_uni_float("cloudOpacity", opacity);
	cloud_shader->set_uni_float("cloudSpeed", speed);
	cloud_shader->set_uni_vec3("cloudColor", color);
	cloud_shader->set_uni_vec3("cloudShadowColor", shadow_color);
	return cloud_shader;
}

std::vector<fluid_particle> create_planetary_shell_particles(int count, float base_radius, float shell_thickness) {
	std::vector<fluid_particle> particles;
	particles.reserve(static_cast<size_t>(count));
	constexpr float golden_angle = 2.39996323f;

	for (int i = 0; i < count; ++i) {
		const float t = (static_cast<float>(i) + 0.5f) / static_cast<float>(count);
		const float y = 1.0f - 2.0f * t;
		const float radial = std::sqrt(glm::max(0.0f, 1.0f - y * y));
		const float angle = golden_angle * static_cast<float>(i);
		const glm::vec3 normal(std::cos(angle) * radial, y, std::sin(angle) * radial);
		const float layer = static_cast<float>(i % 5) / 4.0f;
		const float radius = base_radius + shell_thickness * layer;

		fluid_particle particle;
		particle.position = glm::vec4(normal * radius, 1.0f);
		particle.predicted_position = particle.position;
		const glm::vec3 tangent = glm::normalize(glm::cross(normal, glm::vec3(0.f, 1.f, 0.f) + glm::vec3(0.13f, 0.f, 0.07f)));
		particle.velocity = glm::vec4(tangent * ((layer - 0.5f) * 0.05f), 0.0f);
		particles.push_back(particle);
	}

	return particles;
}

std::vector<physics_data> create_solar_halo_particles(int count) {
	std::vector<physics_data> particles;
	particles.reserve(static_cast<size_t>(count));
	std::mt19937 rng(0x5A17CAFEu);
	std::uniform_real_distribution<float> unit_dist(0.f, 1.f);
	std::uniform_real_distribution<float> phase_dist(0.f, glm::two_pi<float>());
	constexpr float golden_angle = 2.39996323f;

	for (int i = 0; i < count; ++i) {
		const float t = (static_cast<float>(i) + 0.5f) / static_cast<float>(count);
		const float y = 1.0f - 2.0f * t;
		const float radial = std::sqrt(glm::max(0.0f, 1.0f - y * y));
		const float angle = golden_angle * static_cast<float>(i);
		const glm::vec3 normal(std::cos(angle) * radial, y, std::sin(angle) * radial);
		const float shell_radius = glm::mix(1.02f, 1.16f, std::pow(unit_dist(rng), 1.6f));
		const glm::vec3 base_position = normal * shell_radius;
		const glm::vec3 helper = std::abs(normal.y) > 0.85f ? glm::vec3(1.f, 0.f, 0.f) : glm::vec3(0.f, 1.f, 0.f);
		const glm::vec3 tangent = glm::normalize(glm::cross(helper, normal));
		const glm::vec3 bitangent = glm::normalize(glm::cross(normal, tangent));
		const float tangent_mix = unit_dist(rng) * 2.0f - 1.0f;
		const glm::vec3 tangent_seed = glm::normalize(glm::mix(tangent, bitangent, 0.5f + 0.5f * tangent_mix));

		physics_data particle;
		particle.position = glm::vec4(base_position, 1.0f);
		particle.velocity = glm::vec4(tangent_seed, phase_dist(rng));
		particle.accumulated_force = glm::vec4(base_position, glm::mix(0.02f, 0.085f, unit_dist(rng)));
		particles.push_back(particle);
	}

	return particles;
}

bool has_planet_atmosphere(const planet_data& planet) {
	return planet.name != "Mercury";
}

bool is_gas_giant(const planet_data& planet) {
	return planet.name == "Jupiter"
		|| planet.name == "Saturn"
		|| planet.name == "Uranus"
		|| planet.name == "Neptune";
}

float get_planet_atmosphere_scale(const planet_data& planet) {
	if (planet.diameter >= 45000.f)
       return 1.12f;
	if (planet.name == "Venus")
       return 1.09f;
	if (planet.name == "Mars")
      return 1.055f;
	return 1.075f;
}

	std::vector<planet_data> data = {
	{"Mercury", 0.330e24f, 4879, 57.9e6f},
	{"Venus", 4.87e24f, 12104, 108.2e6f},
	{"Earth", 5.97e24f, 12756, 149.6e6f},
	{"Mars", 0.642e24f, 6792, 227.9e6f},
	{"Jupiter", 1898e24f, 142984, 778.6e6f},
	{"Saturn", 568e24f, 120536, 1433.5e6f},
	{"Uranus", 86.8e24f, 51118, 2872.5e6f},
	{"Neptune", 102e24f, 49528, 4495.1e6f}
	};

  float random_float(float min, float max) {
		static std::mt19937 gen(std::random_device{}()); // generator (zainicjalizowany raz)
		std::uniform_real_distribution<float> dist(min, max);
		return dist(gen);
	}

std::vector<physics_data> simtest::create_stress_particles(int count) {
	unit_system u_sys(1e24f, 1e6f, 3.872e6f / 3600.f);
	std::vector<physics_data> particles;
	particles.reserve(static_cast<size_t>(count) + 1);
    std::mt19937 gen(std::random_device{}());
    std::uniform_real_distribution<float> dist_unit(0.f, 1.f);
	std::uniform_real_distribution<float> dist_arm_offset(-0.22f, 0.22f);
	std::uniform_real_distribution<float> dist_thickness(-55.f, 55.f);
	std::uniform_real_distribution<float> dist_mass_e24(0.05f, 9.0f);
	std::uniform_real_distribution<float> dist_speed_scale(0.93f, 1.07f);
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

void simtest::stress_test(scene* s_to_init, std::vector<renderer*>& planets_renders, int count) {
 auto& assets = s_to_init->get_asset_manager();
	shader* planet_shader = assets.create_shader("stress.cpu.planets", "GravitySimulation/camera.vs.shader", "GravitySimulation/camera.fs.shader");

	auto tmp = g_shape::generate_sphere();
	MeshData* sphere_mesh_data = new MeshData();
	*sphere_mesh_data = tmp;
    auto* sphere_mesh = assets.create_mesh(*sphere_mesh_data);

	auto particles = create_stress_particles(count);
	for (size_t i = 0; i < particles.size(); ++i) {
		auto* node = s_to_init->create_scene_node("stress_" + std::to_string(i));
		auto* p = new physics_data(particles[i]);
		node->add_component<rigid_body>(node, p);
		planets_renders.push_back(node->add_component<renderer>(node, planet_shader, sphere_mesh));
		node->set_global_position(p->position);
		node->set_global_scale(i == 0 ? glm::vec3(14.f) : glm::vec3(0.55f));
	}
}


	void simtest::init_gravity_test(scene* s_to_init, std::vector<renderer*>& planets_renders) {

		unit_system u_sys(1e24f, 1e6f, 3.872e6f / 3600.f);
		auto& assets = s_to_init->get_asset_manager();

       auto* sun_node = s_to_init->create_scene_node("Sun");
		shader* mercury_shader = create_rocky_planet_shader(assets, data[0]);
		shader* venus_shader = create_rocky_planet_shader(assets, data[1]);
     shader* earth_shader = create_rocky_planet_shader(assets, data[2], true);
		shader* mars_shader = create_rocky_planet_shader(assets, data[3]);
     shader* earth_cloud_shader = create_planet_cloud_shader(assets, "galactic.planets.earth.clouds", 0.66f, 0.12f, 0.26f, 0.014f, glm::vec3(0.95f, 0.98f, 1.0f), glm::vec3(0.42f, 0.52f, 0.62f));
		shader* venus_cloud_shader = create_planet_cloud_shader(assets, "galactic.planets.venus.clouds", 0.48f, 0.20f, 0.62f, 0.011f, glm::vec3(0.98f, 0.90f, 0.70f), glm::vec3(0.48f, 0.34f, 0.18f));
		shader* gas_giant_shader = assets.create_shader("galactic.planets.gas_giant", "GravitySimulation/lightsource.vs.shader", "GravitySimulation/gas_giant.fs.shader");
        shader* planet_atmosphere_shader = assets.create_shader("galactic.planets.atmosphere", "GravitySimulation/lightsource.vs.shader", "GravitySimulation/planet_atmosphere.fs.shader");
		shader* sun_shader = assets.create_shader("galactic.sun", "GravitySimulation/lightsource.vs.shader", "GravitySimulation/sun.fs.shader");
		shader* sun_halo_shader = assets.create_shader("galactic.sun.halo", "GravitySimulation/lightsource.vs.shader", "GravitySimulation/sun_halo.fs.shader");
        shader* sun_halo_particle_shader = assets.create_shader("galactic.sun.halo.particles", "GravitySimulation/gpu_particle_system.vs.shader", "GravitySimulation/gpu_particle_system.fs.shader");
      shader* ocean_render_shader = assets.create_shader("galactic.earth.ocean.points", "GravitySimulation/gpu_fluid_system.vs.shader", "GravitySimulation/gpu_fluid_system_surface.fs.shader");
		compute_shader* ocean_compute_shader = assets.create_compute_shader("galactic.earth.ocean.compute", "GravitySimulation/fluid_predict.glsl");
		compute_shader* sun_halo_particle_compute = assets.create_compute_shader("galactic.sun.halo.particles.compute", "GravitySimulation/solar_halo_particles.glsl");

		auto tmp = g_shape::generate_sphere();

		MeshData* sphere_mesh_data = new MeshData();
		*sphere_mesh_data = tmp;

        auto* sphere_mesh = assets.create_mesh(*sphere_mesh_data);
		
        auto* sun_render = sun_node->add_component<renderer>(sun_node, sun_shader, sphere_mesh);
		sun_render->set_visual_scale(glm::vec3(1.f));
		auto* sun_halo_node = s_to_init->create_scene_node("SunHalo");
		sun_halo_node->set_parent(sun_node, false);
		auto* sun_halo_render = sun_halo_node->add_component<renderer>(sun_halo_node, sun_halo_shader, sphere_mesh);
     sun_halo_render->set_visual_scale(glm::vec3(1.f));
		auto* sun_halo_particle_node = s_to_init->create_scene_node("SunHaloParticles");
		sun_halo_particle_node->set_parent(sun_node, false);
		static MeshData halo_particle_data = create_particle_point_mesh();
		auto* halo_particle_mesh = assets.create_mesh(halo_particle_data);
		halo_particle_mesh->type = MeshType::POINTS;
		auto* sun_halo_particles = sun_halo_particle_node->add_component<gpu_particle_system_component>(
			sun_halo_particle_node,
			sun_halo_particle_compute,
			sun_halo_particle_shader,
			halo_particle_mesh,
			s_to_init->get_unit_system(),
			create_solar_halo_particles(sun_halo_particle_count),
			12.5f,
			1.0f);
		sun_halo_particles->set_particle_color(glm::vec3(1.0f, 0.84f, 0.30f));
      sun_halo_particles->set_particle_alpha(0.065f);
		sun_halo_particles->set_particle_glow_strength(1.18f);
		sun_halo_particles->set_particle_size_jitter(1.0f);
		sun_halo_particles->set_particle_visual_mode(3);
		sun_halo_particles->set_additive_blend_enabled(true);
		sun_halo_particles->set_depth_write_enabled(false);
        static MeshData ocean_particle_data = create_particle_point_mesh();
		auto* ocean_particle_mesh = assets.create_mesh(ocean_particle_data);
     ocean_particle_mesh->type = MeshType::POINTS;

		float sun_mass = u_sys.mass(1.9885e30f);
		float dia_scale = 12756.f;

		auto* p_data = new physics_data(
			glm::vec4(0, 0, 0, sun_mass),
			glm::vec4(0, 0, 0, sun_mass),
			{ 0, 0, 0, sun_mass });

		auto* s_rigid = sun_node->add_component<rigid_body>(sun_node, p_data);
      const float sun_visual_scale = (1391000 / dia_scale) * sun_visual_radius_scale;
		sun_node->set_global_scale(glm::vec3(sun_visual_scale));
		sun_halo_node->set_scale(glm::vec3(sun_halo_scale_multiplier));
     sun_halo_render->set_blend_mode(renderer_blend_mode::alpha);
		sun_halo_render->set_depth_write_enabled(false);
       sun_halo_render->set_cull_mode(renderer_cull_mode::front);

		//s_rigid->add_compute_buffor();

        for (size_t planet_index = 0; planet_index < data.size(); ++planet_index)
		{
           auto planet = data[planet_index];
           shader* current_planet_shader = nullptr;
			if (planet.name == "Mercury")
				current_planet_shader = mercury_shader;
			else if (planet.name == "Venus")
				current_planet_shader = venus_shader;
			else if (planet.name == "Earth")
				current_planet_shader = earth_shader;
			else if (planet.name == "Mars")
				current_planet_shader = mars_shader;
			else if (is_gas_giant(planet))
				current_planet_shader = gas_giant_shader;

            const float orbital_radius = get_visual_orbital_radius(planet, u_sys, dia_scale);
           const float orbit_angle = get_initial_orbit_angle(planet_index);
			const glm::vec3 radial_direction(std::cos(orbit_angle), 0.f, std::sin(orbit_angle));
			const glm::vec3 tangent_direction(-radial_direction.z, 0.f, radial_direction.x);
			float v = sqrt(u_sys.scaled_G() * sun_mass / orbital_radius);
            // allocate physics data on heap so rigid_body stores a valid pointer
            auto* p_physics_data = new physics_data(
               glm::vec4(radial_direction * orbital_radius, u_sys.mass(planet.mass)),
				glm::vec4(tangent_direction * v, u_sys.mass(planet.mass)),
                { 0, 0, 0, u_sys.mass(planet.mass) });

            auto* planet_node = s_to_init->create_scene_node(planet.name);
			auto* planet_visual_spin_node = s_to_init->create_scene_node(planet.name + "_visual_spin");
			planet_visual_spin_node->set_parent(planet_node, false);
			planet_node->add_component<rigid_body>(planet_node, p_physics_data);
			auto* planet_render = planet_visual_spin_node->add_component<renderer>(planet_visual_spin_node, current_planet_shader, sphere_mesh);
			planet_render->set_visual_scale(glm::vec3(1.f));
			planets_renders.push_back(planet_render);

            planet_node->set_global_position(p_physics_data->position);
            planet_visual_spin_node->set_scale(glm::vec3(planet.diameter/dia_scale));

			if (planet.name == "Earth") {
               auto* cloud_node = s_to_init->create_scene_node("Earth_clouds");
             cloud_node->set_parent(planet_visual_spin_node, false);
				auto* cloud_render = cloud_node->add_component<renderer>(cloud_node, earth_cloud_shader, sphere_mesh);
				cloud_render->set_visual_scale(glm::vec3(1.f));
				cloud_node->set_scale(glm::vec3(1.032f));
				cloud_render->set_blend_mode(renderer_blend_mode::alpha);
				cloud_render->set_depth_write_enabled(false);

				auto* ocean_node = s_to_init->create_scene_node("Earth_ocean_fluid");
                ocean_node->set_parent(planet_visual_spin_node, false);

				fluid_bounds ocean_bounds;
				const float ocean_outer_radius = earth_ocean_base_radius + earth_ocean_shell_thickness;
				ocean_bounds.min = glm::vec3(-ocean_outer_radius - 0.15f);
				ocean_bounds.max = glm::vec3(ocean_outer_radius + 0.15f);
				ocean_bounds.restitution = 0.02f;
				ocean_bounds.damping = 0.995f;
              const auto terrain_profile = planet_terrain::make_rocky_planet_profile(planet.name);
				auto ocean_particles = planet_terrain::create_ocean_seed_particles(planetary_ocean_particle_count, earth_ocean_base_radius, earth_ocean_shell_thickness, terrain_profile);
				auto* ocean_system = new gpu_fluid_system_component(
					ocean_node,
					ocean_compute_shader,
					ocean_render_shader,
					ocean_particle_mesh,
					std::move(ocean_particles),
					ocean_bounds,
					glm::vec3(0.f),
					1.5f,
                  0.06f,
					0.026f,
                    0.3f,
					0.016f,
					0.62f,
					0.34f,
					6.2f,
					3u,
					5u);
				ocean_node->add_component(ocean_system);
				ocean_system->set_planetary_surface(glm::vec3(0.f), earth_ocean_base_radius, earth_ocean_shell_thickness, 4.2f);
              ocean_system->set_planetary_flow_tuning(0.18f, 0.34f, 0.18f, 0.56f, 0.72f);
				ocean_system->set_planetary_flood_guidance_strength(0.22f);
				ocean_system->set_planetary_surface_layer_tuning(0.56f, 0.3f, 0.68f);
              ocean_system->set_planetary_rotation_tuning(1.35f, 1.0f, 1.0f);
              ocean_system->set_planetary_respawn_management(true, 12u);
             ocean_system->set_planetary_water_coverage(terrain_profile.ocean_coverage);
             ocean_system->set_planetary_surface_frame_node(ocean_node);
				ocean_system->set_planetary_terrain_profile(terrain_profile);
                ocean_system->set_debug_visualization_mode(fluid_debug_visualization_mode::none);
				ocean_system->set_debug_readback_enabled(true, 20u);
			}

			if (planet.name == "Venus") {
				auto* cloud_node = s_to_init->create_scene_node("Venus_clouds");
             cloud_node->set_parent(planet_visual_spin_node, false);
				auto* cloud_render = cloud_node->add_component<renderer>(cloud_node, venus_cloud_shader, sphere_mesh);
				cloud_render->set_visual_scale(glm::vec3(1.f));
				cloud_node->set_scale(glm::vec3(1.024f));
				cloud_render->set_blend_mode(renderer_blend_mode::alpha);
				cloud_render->set_depth_write_enabled(false);
			}

			if (has_planet_atmosphere(planet)) {
				auto* atmosphere_node = s_to_init->create_scene_node(planet.name + "_atmosphere");
                atmosphere_node->set_parent(planet_visual_spin_node, false);
				auto* atmosphere_render = atmosphere_node->add_component<renderer>(atmosphere_node, planet_atmosphere_shader, sphere_mesh);
             atmosphere_render->set_visual_scale(glm::vec3(1.f));
				atmosphere_node->set_scale(glm::vec3(get_planet_atmosphere_scale(planet)));
				atmosphere_render->set_blend_mode(renderer_blend_mode::additive);
				atmosphere_render->set_depth_write_enabled(false);
				atmosphere_render->set_cull_mode(renderer_cull_mode::front);
			}

		}

		//for (int i = 0; i < 1000; i++)
		//{
		//	float distance_to_sun = random_float(1.f, 300.f);
		//	float mass = random_float(1, 1000);

		//	float v = sqrt(u_sys.scaled_G() * sun_mass / distance_to_sun);
		//	auto p_physics_data = physics_data(
		//		glm::vec4(distance_to_sun*-1*(i%2), 0, distance_to_sun, mass),
		//		glm::vec4(0, 0, v, mass),
		//		{ 0, 0, 0, mass });

		//	auto* planet_node = s_to_init->create_scene_node("test"+std::to_string(i));
		//	planet_node->add_component<rigid_body>(planet_node, p_physics_data);
		//	planets_renders.push_back(planet_node->add_component<renderer>(planet_node, planet_shader, sphere_mesh));

		//	planet_node->set_global_position(p_physics_data.position);
		//	planet_node->set_global_scale(glm::vec3(5));
		//}
	}
}