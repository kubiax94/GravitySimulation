#pragma once
#include <glad/glad.h>
#include "physics_data.h"
#include "Shader.h"
#include <vector>
#include <cstring>

class compute_shader : public shader
{
private:
	GLuint ssbo_{};
	size_t current_buffor_size{};

	struct ssbo_info
	{
		GLuint id = 0;
		size_t size = 0;

		std::vector<GLuint> pbo_ids;
		std::vector<GLsync> pbo_syncs;
		int pbo_next = 0;

		virtual ~ssbo_info() {
			for (auto s : pbo_syncs) {
				if (s) glDeleteSync(s);
			}
			if (!pbo_ids.empty()) {
				glDeleteBuffers(static_cast<GLsizei>(pbo_ids.size()), pbo_ids.data());
			}
			if (id) glDeleteBuffers(1, &id);
		}
	};

	template<typename T>
	struct ssbo_data : ssbo_info
	{
		std::vector<T> data;
	}; 

	std::unordered_map<GLuint, ssbo_info*> binding_data_;
	void change_buffor_size(const size_t& n_size);

public:
	compute_shader(const char* compute_source);
	~compute_shader() override;

	template<typename T>
	void add_ssbo(const GLuint& binding, const std::vector<T>& data);
	void bind_ssbos(const std::vector<GLuint>& bindings);

	void dispatch(const glm::uvec3& groups);

	void init();
	void use() override;

	template <typename T>
	void enqueue_readback(GLuint binding);

	template <typename T>
	void try_readback(GLuint binding, std::vector<T>& out);

	template <typename T>
	void get_binding_data(GLuint bind, std::vector<T>& out);

	template<typename T>
	void update_ssbo(const GLuint& binding, const std::vector<T>& data);
};

template <typename T>
void compute_shader::add_ssbo(const GLuint& binding, const std::vector<T>& data) {
	use();

	if (binding_data_.contains(binding)) {
		delete binding_data_[binding];
		binding_data_.erase(binding);
	}

	ssbo_data<T>* n_data = new ssbo_data<T>();
	n_data->size = data.size();
	n_data->data = data;

	glGenBuffers(1, &n_data->id);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, n_data->id);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(T) * data.size(), data.data(), GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, n_data->id);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	binding_data_[binding] = n_data;
}

template <typename T>
void compute_shader::get_binding_data(GLuint bind, std::vector<T>& out) {
    if (!binding_data_.contains(bind)) {
        out.clear();
        return;
    }

    auto* info = binding_data_.at(bind);
    const size_t count = info->size;

    use();
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, info->id);
    out.resize(count);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(T) * count, out.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

template <typename T>
void compute_shader::enqueue_readback(GLuint binding) {
    if (!binding_data_.contains(binding)) return;
    auto* info = binding_data_.at(binding);

    if (info->pbo_ids.empty()) {
        const int pbo_count = 2;
        info->pbo_ids.resize(pbo_count);
        info->pbo_syncs.resize(pbo_count, 0);
        glGenBuffers(pbo_count, info->pbo_ids.data());
        for (int i = 0; i < pbo_count; ++i) {
            glBindBuffer(GL_PIXEL_PACK_BUFFER, info->pbo_ids[i]);
            glBufferData(GL_PIXEL_PACK_BUFFER, sizeof(T) * info->size, nullptr, GL_STREAM_READ);
        }
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        info->pbo_next = 0;
    }

    GLuint ssbo = info->id;
    GLuint pbo = info->pbo_ids[info->pbo_next];

    glBindBuffer(GL_COPY_READ_BUFFER, ssbo);
    glBindBuffer(GL_COPY_WRITE_BUFFER, pbo);
    glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, sizeof(T) * info->size);

    if (info->pbo_syncs[info->pbo_next]) {
        glDeleteSync(info->pbo_syncs[info->pbo_next]);
        info->pbo_syncs[info->pbo_next] = 0;
    }
    info->pbo_syncs[info->pbo_next] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

    info->pbo_next = (info->pbo_next + 1) % static_cast<int>(info->pbo_ids.size());

    glBindBuffer(GL_COPY_READ_BUFFER, 0);
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
}

template <typename T>
void compute_shader::try_readback(GLuint binding, std::vector<T>& out) {
    if (!binding_data_.contains(binding)) { out.clear(); return; }
    auto* info = binding_data_.at(binding);
    if (info->pbo_ids.empty()) { out.clear(); return; }

    const int ready_index = info->pbo_next;
    GLsync s = info->pbo_syncs[ready_index];
    if (!s) { out.clear(); return; }

    GLenum res = glClientWaitSync(s, 0, 0);
    if (res == GL_ALREADY_SIGNALED || res == GL_CONDITION_SATISFIED) {
        GLuint pbo = info->pbo_ids[ready_index];
        glBindBuffer(GL_COPY_WRITE_BUFFER, pbo);
        void* ptr = glMapBufferRange(GL_COPY_WRITE_BUFFER, 0, sizeof(T) * info->size, GL_MAP_READ_BIT);
        if (ptr) {
            out.resize(info->size);
            memcpy(out.data(), ptr, sizeof(T) * info->size);
            glUnmapBuffer(GL_COPY_WRITE_BUFFER);
        } else {
            out.clear();
        }
        glBindBuffer(GL_COPY_WRITE_BUFFER, 0);

        glDeleteSync(s);
        info->pbo_syncs[ready_index] = 0;
    } else {
        out.clear();
    }
}

template <typename T>
void compute_shader::update_ssbo(const GLuint& binding, const std::vector<T>& data) {
    if (!binding_data_.contains(binding)) {
        add_ssbo(binding, data);
        return;
    }

    use();

    auto* old_data = static_cast<ssbo_data<T>*>(binding_data_[binding]);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, old_data->id);

    if (old_data->size < data.size())
    {
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(T) * data.size(), data.data(), GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, old_data->id);
        old_data->size = data.size();
    }
    else
    {
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(T) * data.size(), data.data());
    }

    old_data->data = data;
    old_data->size = data.size();
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}
