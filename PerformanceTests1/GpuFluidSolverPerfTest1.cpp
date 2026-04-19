#include "pch.h"
#include "CppUnitTest.h"
#include <cmath>
#include <filesystem>
#include <memory>
#include <vector>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "../GravitySimulation/compute_shader.h"
#include "../GravitySimulation/fluid_bounds.h"
#include "../GravitySimulation/fluid_particle.h"
#include "../GravitySimulation/gpu_fluid_system_component.h"
#include "../GravitySimulation/scene_node.h"
#define GLFW_INCLUDE_NONE







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
			window = glfwCreateWindow(64, 64, "GpuFluidSolverPerfTest", nullptr, nullptr);
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

	std::vector<fluid_particle> create_box_particles(int count_x, int count_y, int count_z, float spacing) {
		std::vector<fluid_particle> particles;
		particles.reserve(static_cast<size_t>(count_x) * static_cast<size_t>(count_y) * static_cast<size_t>(count_z));

		const glm::vec3 base(-3.0f, 1.0f, -3.0f);
		for (int z = 0; z < count_z; ++z) {
			for (int y = 0; y < count_y; ++y) {
				for (int x = 0; x < count_x; ++x) {
					fluid_particle particle;
					particle.position = glm::vec4(base + glm::vec3(x, y, z) * spacing, 1.0f);
					particle.velocity = glm::vec4(0.0f);
					particle.predicted_position = particle.position;
					particles.push_back(particle);
				}
			}
		}

		return particles;
	}

	float run_solver_case(unsigned int solver_substeps, unsigned int constraint_iterations, int frame_count) {
		gl_test_context context;
		const auto solution_root = std::filesystem::path(__FILE__).parent_path().parent_path();
		const auto compute_path = solution_root / "GravitySimulation" / "fluid_predict.glsl";
		auto compute = std::make_unique<compute_shader>(compute_path.string().c_str());
		Assert::IsTrue(compute->is_vaild(), L"compute shader failed to load");

		fluid_bounds bounds;
		bounds.min = glm::vec3(-6.f, 0.25f, -6.f);
		bounds.max = glm::vec3(6.f, 14.f, 6.f);
		bounds.restitution = 0.18f;
		bounds.damping = 0.98f;

		scene_node node("fluid_perf", static_cast<scene_node*>(nullptr), static_cast<i_scene_manager*>(nullptr));
		gpu_fluid_system_component system(
			&node,
			compute.get(),
			nullptr,
			nullptr,
			create_box_particles(16, 16, 16, 0.38f),
			bounds,
			glm::vec3(0.f, -18.f, 0.f),
			7.5f,
			0.52f,
			0.17f,
			0.32f,
			0.18f,
			0.45f,
			0.22f,
			6.0f,
			solver_substeps,
			constraint_iterations);

		for (int frame = 0; frame < frame_count; ++frame) {
			system.fixed_update(1.0f / 60.0f);
			glFinish();
		}

		std::vector<fluid_particle> gpu_result;
		compute->get_binding_data(0, gpu_result);
		Assert::AreEqual(static_cast<size_t>(16 * 16 * 16), gpu_result.size(), L"unexpected particle count");
		compute->cleanup();

		float sink = 0.0f;
		for (size_t i = 0; i < gpu_result.size(); i += 257)
			sink += gpu_result[i].position.x + gpu_result[i].position.y + gpu_result[i].position.z;

		Assert::IsTrue(std::isfinite(sink), L"sink must be finite");
		return sink;
	}
}

namespace PerformanceTests1
{
	TEST_CLASS(GpuFluidSolverPerfTest)
	{
	public:
		TEST_METHOD(FluidSolverLowIterationProfile)
		{
			const float sink = run_solver_case(1u, 1u, 12);
			Assert::IsTrue(std::isfinite(sink), L"sink must be finite");
		}

		TEST_METHOD(FluidSolverHighIterationProfile)
		{
			const float sink = run_solver_case(3u, 5u, 12);
			Assert::IsTrue(std::isfinite(sink), L"sink must be finite");
		}
	};
}
