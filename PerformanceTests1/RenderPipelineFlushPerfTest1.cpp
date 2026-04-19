#include "pch.h"
#include "CppUnitTest.h"
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "../GravitySimulation/render_pipeline.h"
#include "..\GravitySimulation\Camera.h"
#include "..\GravitySimulation\Mesh.h"
#include "..\GravitySimulation\Renderer.h"
#include "..\GravitySimulation\Shader.h"
#include "..\GravitySimulation\scene_node.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


namespace {
	struct gl_test_context {
		GLFWwindow* window = nullptr;
		bool initialized = false;

		gl_test_context() {
			initialized = glfwInit() == GLFW_TRUE;
            Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue(initialized, L"glfwInit failed");
			glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
			glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
			glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
			glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
			window = glfwCreateWindow(64, 64, "RenderPipelinePerfTest", nullptr, nullptr);
          Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsNotNull(window, L"glfwCreateWindow failed");
			glfwMakeContextCurrent(window);
            Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue(gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) != 0, L"gladLoadGLLoader failed");
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

	MeshData create_mesh_data() {
		MeshData data;
		data.vertecies = {
			{{-0.5f, -0.5f, 0.0f}, {0.f, 0.f, 1.f}, {0.f, 0.f}},
			{{0.5f, -0.5f, 0.0f}, {0.f, 0.f, 1.f}, {1.f, 0.f}},
			{{0.0f, 0.5f, 0.0f}, {0.f, 0.f, 1.f}, {0.5f, 1.f}}
		};
		data.indices = { 0u, 1u, 2u };
		return data;
	}
}

namespace PerformanceTests1
{
	TEST_CLASS(RenderPipelinePerfTest)
	{
	public:
		TEST_METHOD(RenderPipelineFlushInstanced)
		{
            constexpr int RendererCount = 25000;
			constexpr int FrameCount = 192;

			gl_test_context context;
			const auto shader_dir = std::filesystem::temp_directory_path() / "GravitySimulation" / "RenderPipelinePerfTest";
			const auto vertex_path = ensure_shader_file(shader_dir / "perf_test.vs", R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 2) in mat4 instanceModel;
uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
uniform int useInstancing;
void main()
{
    mat4 finalModel = useInstancing != 0 ? instanceModel : model;
    gl_Position = projection * view * finalModel * vec4(aPos, 1.0);
}
)");
			const auto fragment_path = ensure_shader_file(shader_dir / "perf_test.fs", R"(#version 330 core
out vec4 FragColor;
void main()
{
    FragColor = vec4(1.0, 1.0, 1.0, 1.0);
}
)");
			const auto vertex_path_string = vertex_path.string();
			const auto fragment_path_string = fragment_path.string();
			auto batch_shader = std::make_unique<shader>(vertex_path_string.c_str(), fragment_path_string.c_str());
			Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue(batch_shader->is_vaild(), L"shader failed to load");

			MeshData mesh_data = create_mesh_data();
			auto batch_mesh = std::make_unique<Mesh>(mesh_data);
			Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue(batch_mesh->is_vaild(), L"mesh failed to initialize");

			scene_node camera_node("camera", static_cast<scene_node*>(nullptr), static_cast<i_scene_manager*>(nullptr));
			camera_node.set_position(0.0f, 0.0f, 10.0f);
			Camera camera(&camera_node);

			render_pipeline pipeline;
			std::vector<std::unique_ptr<scene_node>> nodes;
			std::vector<std::unique_ptr<renderer>> renderers;
			nodes.reserve(RendererCount);
			renderers.reserve(RendererCount);

			for (int i = 0; i < RendererCount; ++i)
			{
				auto node = std::make_unique<scene_node>("node_" + std::to_string(i), static_cast<scene_node*>(nullptr), static_cast<i_scene_manager*>(nullptr));
				node->set_position(static_cast<float>(i % 100), static_cast<float>((i / 100) % 100), static_cast<float>(i % 17));
				node->set_rotation(static_cast<float>(i % 360), static_cast<float>((i * 3) % 360), static_cast<float>((i * 7) % 360));
				node->set_scale(1.0f + static_cast<float>(i % 5) * 0.05f);
				auto render = std::make_unique<renderer>(node.get(), batch_shader.get(), batch_mesh.get());
				render->set_visual_scale(glm::vec3(1.0f + static_cast<float>(i % 3) * 0.1f));
				renderers.push_back(std::move(render));
				nodes.push_back(std::move(node));
			}

			volatile float sink = 0.0f;
			for (int frame = 0; frame < FrameCount; ++frame)
			{
				for (int i = 0; i < RendererCount; ++i)
				{
					nodes[i]->set_position(static_cast<float>((i + frame) % 100), static_cast<float>(((i / 100) + frame) % 100), static_cast<float>((i + frame * 3) % 19));
				}
				pipeline.begin_frame();
				for (const auto& render : renderers)
					pipeline.submit(render.get());
				pipeline.flush(&camera, nullptr);
				sink += nodes[frame % RendererCount]->get_global_position().x;
			}

			batch_mesh->cleanup();
			batch_shader->cleanup();
			Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue(sink >= 0.0f || renderers.empty());
		}
	};
}

#include "..\GravitySimulation\render_pipeline.cpp"
