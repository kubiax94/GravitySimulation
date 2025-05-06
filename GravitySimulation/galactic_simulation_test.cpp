#include "galactic_simulation_test.h"

#include <random>

#include "Mesh.h"
#include "Shape.h"
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


	void simtest::init_gravity_test(scene* s_to_init, std::vector<renderer*>& planets_renders) {

		unit_system u_sys(1e24f, 1e6f, 3.872e6f / 3600.f);

		auto* sun_node = s_to_init->create_scene_node("Sun");
		shader* planet_shader = new shader("C:/Users/Kubiaxx/source/repos/GravitySimulation/GravitySimulation/camera.vs.shader", "C:/Users/Kubiaxx/source/repos/GravitySimulation/GravitySimulation/camera.fs.shader");
		shader* sun_shader = new shader("C:/Users/Kubiaxx/source/repos/GravitySimulation/GravitySimulation/lightsource.vs.shader", "C:/Users/Kubiaxx/source/repos/GravitySimulation/GravitySimulation/sun.fs.shader");

		auto tmp = Shape::GenerateSphere();

		MeshData* sphere_mesh_data = new MeshData();
		*sphere_mesh_data = tmp;

		auto* sphere_mesh = new Mesh(*sphere_mesh_data);
		
		auto* sun_render = sun_node->add_component<renderer>(sun_node, sun_shader, sphere_mesh);

		float sun_mass = u_sys.mass(1.9885e30f);
		float dia_scale = 12756.f;

		auto* p_data = new physics_data(
			glm::vec4(0, 0, 0, sun_mass),
			glm::vec4(0, 0, 0, sun_mass),
			{ 0, 0, 0, sun_mass });

		sun_node->add_component<rigid_body>(sun_node, p_data);
		sun_node->set_global_scale(glm::vec3(1391000/dia_scale));

		for (auto planet : data)
		{
			float v = sqrt(u_sys.scaled_G() * sun_mass / u_sys.distance(planet.distance_to_sun));
			auto p_physics_data = physics_data(
				glm::vec4(u_sys.distance(planet.distance_to_sun) + 109.f, 0, 0, u_sys.mass(planet.mass)),
				glm::vec4(0, 0, v, u_sys.mass(planet.mass)),
				{ 0, 0, 0, u_sys.mass(planet.mass) });

			auto* planet_node = s_to_init->create_scene_node(planet.name);
			planet_node->add_component<rigid_body>(planet_node, p_physics_data);
			planets_renders.push_back(planet_node->add_component<renderer>(planet_node, planet_shader, sphere_mesh));

			planet_node->set_global_position(p_physics_data.position);
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