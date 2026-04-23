#include "pch.h"
#include "CppUnitTest.h"
#include <cmath>
#include <filesystem>
#include <memory>
#include <vector>
#define GLFW_INCLUDE_NONE
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "../GravitySimulation/compute_shader.h"
#include "../GravitySimulation/fluid_bounds.h"
#include "../GravitySimulation/fluid_particle.h"
#include "../GravitySimulation/gpu_fluid_system_component.h"
#include "../GravitySimulation/planet_terrain.h"
#include "../GravitySimulation/scene_node.h"








using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace {
	struct gl_test_context {
		GLFWwindow* window = nullptr;
		bool initialized = false;

		gl_test_context() {
			initialized = glfwInit() == GLFW_TRUE;
			Assert::IsTrue(initialized, L"glfwInit failed");
			glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
			glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
			glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
			glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
			window = glfwCreateWindow(64, 64, "GpuFluidPlanetaryRespawnPerfTest", nullptr, nullptr);
			Assert::IsNotNull(window, L"glfwCreateWindow failed");
			glfwMakeContextCurrent(window);
			Assert::IsTrue(gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) != 0, L"gladLoadGLLoader failed");
			glViewport(0, 0, 64, 64);
		}

		~gl_test_context() {
			if (window)
				glfwDestroyWindow(window);
			if (initialized)
				glfwTerminate();
		}
	};

	std::vector<fluid_particle> create_planetary_particles(int latitude_count, int longitude_count, int radial_layers, float base_radius, float radial_step) {
		std::vector<fluid_particle> particles;
		particles.reserve(static_cast<size_t>(latitude_count) * static_cast<size_t>(longitude_count) * static_cast<size_t>(radial_layers));

		for (int layer = 0; layer < radial_layers; ++layer) {
			const float radius = base_radius + radial_step * static_cast<float>(layer);
			for (int lat = 0; lat < latitude_count; ++lat) {
				const float v = (static_cast<float>(lat) + 0.5f) / static_cast<float>(latitude_count);
				const float phi = v * 3.1415926535f;
				const float sin_phi = std::sin(phi);
				const float cos_phi = std::cos(phi);
				for (int lon = 0; lon < longitude_count; ++lon) {
					const float u = static_cast<float>(lon) / static_cast<float>(longitude_count);
					const float theta = u * 6.283185307f;
					const glm::vec3 normal(
						std::cos(theta) * sin_phi,
						cos_phi,
						std::sin(theta) * sin_phi);

					fluid_particle particle;
					particle.position = glm::vec4(normal * radius, 1.0f);
					particle.velocity = glm::vec4(0.0f);
					particle.predicted_position = particle.position;
					particle.delta_position = glm::vec4(0.0f);
					particle.solver_data = glm::vec4(0.0f);
					particles.push_back(particle);
				}
			}
		}

		return particles;
	}

	float run_planetary_respawn_case(int frame_count) {
		gl_test_context context;
		const auto solution_root = std::filesystem::path(__FILE__).parent_path().parent_path();
		const auto compute_path = solution_root / "GravitySimulation" / "fluid_predict.glsl";
		auto compute = std::make_unique<compute_shader>(compute_path.string().c_str());
		Assert::IsTrue(compute->is_vaild(), L"compute shader failed to load");

		fluid_bounds bounds;
		bounds.min = glm::vec3(-2.5f);
		bounds.max = glm::vec3(2.5f);
		bounds.restitution = 0.02f;
		bounds.damping = 0.995f;

		scene_node node("planetary_fluid_perf", static_cast<scene_node*>(nullptr), static_cast<i_scene_manager*>(nullptr));
		gpu_fluid_system_component system(
			&node,
			compute.get(),
			nullptr,
			nullptr,
			create_planetary_particles(32, 32, 4, 1.01f, 0.01f),
			bounds,
			glm::vec3(0.0f),
			7.5f,
			0.52f,
			0.02f,
			0.32f,
			0.18f,
			0.45f,
			0.22f,
			6.0f,
			2u,
			2u);

		auto terrain_profile = planet_terrain::make_rocky_planet_profile("Earth");
		system.set_planetary_surface(glm::vec3(0.0f), 1.0f, 0.08f, 9.81f);
		system.set_planetary_terrain_profile(terrain_profile);
		system.set_planetary_water_coverage(0.8f);
		system.set_planetary_respawn_management(true, 1u);

		for (int frame = 0; frame < frame_count; ++frame) {
			system.fixed_update(1.0f / 60.0f);
			glFinish();
		}

		std::vector<fluid_particle> gpu_result;
		compute->get_binding_data(0, gpu_result);
		Assert::AreEqual(static_cast<size_t>(32 * 32 * 4), gpu_result.size(), L"unexpected particle count");
		compute->cleanup();

		float sink = 0.0f;
		for (size_t i = 0; i < gpu_result.size(); i += 131)
			sink += gpu_result[i].position.x + gpu_result[i].position.y + gpu_result[i].position.z;

		Assert::IsTrue(std::isfinite(sink), L"sink must be finite");
		return sink;
	}
}

namespace PerformanceTests1
{
	TEST_CLASS(GpuFluidPlanetaryRespawnPerfTest)
	{
	public:
		TEST_METHOD(PlanetaryRespawnHydrologyProfile)
		{
			const float sink = run_planetary_respawn_case(16);
			Assert::IsTrue(std::isfinite(sink), L"sink must be finite");
		}
	};
}
