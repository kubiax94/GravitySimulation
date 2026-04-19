#include "galactic_simulation_test.h"

#include <cmath>
#include <random>

#include "Mesh.h"
#include "asset_manager.h"
#include "g_shape.h"
#include "renderer.h"

struct planet_data
{
	std::string name;
	float mass = 0.f;
	float diameter = 0.f;
	float distance_to_sun = 0.f;
};
namespace simtest {

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
        shader* planet_shader = assets.create_shader("galactic.planets", "GravitySimulation/camera.vs.shader", "GravitySimulation/camera.fs.shader");
		shader* sun_shader = assets.create_shader("galactic.sun", "GravitySimulation/lightsource.vs.shader", "GravitySimulation/sun.fs.shader");

		auto tmp = g_shape::generate_sphere();

		MeshData* sphere_mesh_data = new MeshData();
		*sphere_mesh_data = tmp;

        auto* sphere_mesh = assets.create_mesh(*sphere_mesh_data);
		
		auto* sun_render = sun_node->add_component<renderer>(sun_node, sun_shader, sphere_mesh);

		float sun_mass = u_sys.mass(1.9885e30f);
		float dia_scale = 12756.f;

		auto* p_data = new physics_data(
			glm::vec4(0, 0, 0, sun_mass),
			glm::vec4(0, 0, 0, sun_mass),
			{ 0, 0, 0, sun_mass });

		auto* s_rigid = sun_node->add_component<rigid_body>(sun_node, p_data);
		sun_node->set_global_scale(glm::vec3(1391000/dia_scale));

		//s_rigid->add_compute_buffor();

		for (auto planet : data)
		{
			float v = sqrt(u_sys.scaled_G() * sun_mass / u_sys.distance(planet.distance_to_sun));
            // allocate physics data on heap so rigid_body stores a valid pointer
            auto* p_physics_data = new physics_data(
                glm::vec4(u_sys.distance(planet.distance_to_sun), 0, 0, u_sys.mass(planet.mass)),
                glm::vec4(0, 0, v, u_sys.mass(planet.mass)),
                { 0, 0, 0, u_sys.mass(planet.mass) });

            auto* planet_node = s_to_init->create_scene_node(planet.name);
            planet_node->add_component<rigid_body>(planet_node, p_physics_data);
            planets_renders.push_back(planet_node->add_component<renderer>(planet_node, planet_shader, sphere_mesh));

            planet_node->set_global_position(p_physics_data->position);
			planet_node->set_global_scale(glm::vec3(planet.diameter/dia_scale));

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