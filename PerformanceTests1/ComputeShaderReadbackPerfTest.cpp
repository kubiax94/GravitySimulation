#include "pch.h"
#include "CppUnitTest.h"
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "../GravitySimulation/compute_shader.h"
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
			window = glfwCreateWindow(64, 64, "ComputeShaderReadbackPerfTest", nullptr, nullptr);
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

	std::filesystem::path ensure_shader_file(const std::filesystem::path& path, const char* contents) {
		std::filesystem::create_directories(path.parent_path());
		std::ofstream file(path, std::ios::binary | std::ios::trunc);
		file << contents;
		return path;
	}
}

namespace PerformanceTests1
{
	TEST_CLASS(ComputeShaderReadbackPerfTest)
	{
	public:
		TEST_METHOD(ComputeShaderBlockingReadback)
		{
			constexpr size_t ParticleCount = 8192;
			constexpr GLuint LocalSizeX = 64;
			constexpr int IterationCount = 128;

			gl_test_context context;
			const auto shader_dir = std::filesystem::temp_directory_path() / "GravitySimulation" / "ComputeShaderReadbackPerfTest";
			const auto compute_path = ensure_shader_file(shader_dir / "perf_test.comp", R"(#version 430 core
layout(local_size_x = 64) in;

struct PhysicsData {
    vec4 position;
    vec4 velocity;
    vec4 accumulated_force;
};

layout(std430, binding = 0) buffer PhysicsBuffer {
    PhysicsData data[];
};

uniform float frameBias;

void main()
{
    uint index = gl_GlobalInvocationID.x;
    if (index >= data.length())
        return;

    data[index].position.x += frameBias;
}
)");
			const auto compute_path_string = compute_path.string();
			auto compute = std::make_unique<compute_shader>(compute_path_string.c_str());
			Assert::IsTrue(compute->is_vaild(), L"compute shader failed to load");

			std::vector<physics_data> initial_data(ParticleCount);
			for (size_t i = 0; i < ParticleCount; ++i)
			{
				initial_data[i].position = glm::vec4(static_cast<float>(i), 0.f, 0.f, 1.f);
				initial_data[i].velocity = glm::vec4(0.f);
				initial_data[i].accumulated_force = glm::vec4(0.f);
			}

			compute->add_ssbo(0, initial_data);
			const GLuint groups_x = static_cast<GLuint>((ParticleCount + LocalSizeX - 1) / LocalSizeX);

			std::vector<physics_data> gpu_result;
			gpu_result.reserve(ParticleCount);
			volatile float sink = 0.f;
			for (int iteration = 0; iteration < IterationCount; ++iteration)
			{
				compute->use();
				compute->set_uni_float("frameBias", 0.001f * static_cast<float>(iteration + 1));
				compute->dispatch({ groups_x, 1, 1 });
				compute->get_binding_data(0, gpu_result);
				Assert::AreEqual(ParticleCount, gpu_result.size(), L"unexpected readback size");
				sink += gpu_result[iteration % ParticleCount].position.x;
			}

			compute->cleanup();
			Assert::IsTrue(sink > 0.f, L"sink should consume readback data");
		}
	};
}
