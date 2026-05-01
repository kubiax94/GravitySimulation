#pragma once
#include <glad/glad.h>
#include "physics_data.h"
#include "Shader.h"
#include <cstddef>
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
        int pbo_write_index = 0;
        int pbo_read_index = 0;
        size_t pbo_pending_count = 0;
        std::vector<std::byte> latest_completed_result;

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

    static void reset_async_readback_buffers(ssbo_info& info) {
        for (auto& sync : info.pbo_syncs) {
            if (sync) {
                glDeleteSync(sync);
                sync = 0;
            }
        }

        if (!info.pbo_ids.empty()) {
            glDeleteBuffers(static_cast<GLsizei>(info.pbo_ids.size()), info.pbo_ids.data());
            info.pbo_ids.clear();
        }

        info.pbo_syncs.clear();
        info.pbo_write_index = 0;
        info.pbo_read_index = 0;
        info.pbo_pending_count = 0;
        info.latest_completed_result.clear();
    }

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
    GLuint get_ssbo_id(GLuint binding) const;

	template<typename T>
	void add_ssbo(const GLuint& binding, const std::vector<T>& data);
	void bind_ssbos(const std::vector<GLuint>& bindings);

	void dispatch(const glm::uvec3& groups);

	void init();
	void use() override;
    void clear_readback_state(GLuint binding);

	template <typename T>
	void enqueue_readback(GLuint binding);

    template <typename T>
    void enqueue_readback_indices(GLuint binding, const std::vector<size_t>& indices);

	template <typename T>
	void try_readback(GLuint binding, std::vector<T>& out);

    template <typename T>
    bool try_dequeue_readback(GLuint binding, std::vector<T>& out);

    template <typename T>
    bool try_dequeue_readback_indices(GLuint binding, const std::vector<size_t>& indices, std::vector<T>& out);

	template <typename T>
	void get_binding_data(GLuint bind, std::vector<T>& out);

    template <typename T>
    void get_binding_data_indices(GLuint bind, const std::vector<size_t>& indices, std::vector<T>& out);

	template<typename T>
	void update_ssbo(const GLuint& binding, const std::vector<T>& data);

    template<typename T>
    void update_ssbo_indices(const GLuint& binding, const std::vector<size_t>& indices, const std::vector<T>& data);
};

inline GLuint compute_shader::get_ssbo_id(GLuint binding) const {
    auto it = binding_data_.find(binding);
    return it != binding_data_.end() && it->second ? it->second->id : 0;
}

inline void compute_shader::clear_readback_state(GLuint binding) {
    auto it = binding_data_.find(binding);
    if (it == binding_data_.end() || !it->second)
        return;

    reset_async_readback_buffers(*it->second);
}

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
void compute_shader::get_binding_data_indices(GLuint bind, const std::vector<size_t>& indices, std::vector<T>& out) {
    if (!binding_data_.contains(bind) || indices.empty()) {
        out.clear();
        return;
    }

    auto* info = binding_data_.at(bind);
    out.resize(indices.size());

    use();
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, info->id);

    size_t range_start = 0u;
    while (range_start < indices.size()) {
        size_t range_end = range_start + 1u;
        while (range_end < indices.size() && indices[range_end] == indices[range_end - 1u] + 1u)
            ++range_end;

        const size_t first_index = indices[range_start];
        const size_t range_count = range_end - range_start;
        glGetBufferSubData(
            GL_SHADER_STORAGE_BUFFER,
            static_cast<GLintptr>(sizeof(T) * first_index),
            static_cast<GLsizeiptr>(sizeof(T) * range_count),
            out.data() + range_start);

        range_start = range_end;
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

template <typename T>
void compute_shader::enqueue_readback(GLuint binding) {
    if (!binding_data_.contains(binding)) return;
    auto* info = binding_data_.at(binding);

    if (info->pbo_ids.empty()) {
        const int pbo_count = 4;
        info->pbo_ids.resize(pbo_count);
        info->pbo_syncs.resize(pbo_count, 0);
        glGenBuffers(pbo_count, info->pbo_ids.data());
        for (int i = 0; i < pbo_count; ++i) {
            glBindBuffer(GL_PIXEL_PACK_BUFFER, info->pbo_ids[i]);
            glBufferData(GL_PIXEL_PACK_BUFFER, sizeof(T) * info->size, nullptr, GL_STREAM_READ);
        }
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        info->pbo_write_index = 0;
        info->pbo_read_index = 0;
        info->pbo_pending_count = 0;
    }

    if (info->pbo_pending_count == info->pbo_ids.size()) {
        const int drop_index = info->pbo_read_index;
        if (info->pbo_syncs[drop_index]) {
            glDeleteSync(info->pbo_syncs[drop_index]);
            info->pbo_syncs[drop_index] = 0;
        }

        info->pbo_read_index = (info->pbo_read_index + 1) % static_cast<int>(info->pbo_ids.size());
        --info->pbo_pending_count;
    }

    GLuint ssbo = info->id;
    const int write_index = info->pbo_write_index;
    GLuint pbo = info->pbo_ids[write_index];

    glBindBuffer(GL_COPY_READ_BUFFER, ssbo);
    glBindBuffer(GL_COPY_WRITE_BUFFER, pbo);
    glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, sizeof(T) * info->size);

    if (info->pbo_syncs[write_index]) {
        glDeleteSync(info->pbo_syncs[write_index]);
        info->pbo_syncs[write_index] = 0;
    }
    info->pbo_syncs[write_index] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

    info->pbo_write_index = (info->pbo_write_index + 1) % static_cast<int>(info->pbo_ids.size());
    ++info->pbo_pending_count;

    glBindBuffer(GL_COPY_READ_BUFFER, 0);
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
}

template <typename T>
void compute_shader::enqueue_readback_indices(GLuint binding, const std::vector<size_t>& indices) {
    if (!binding_data_.contains(binding) || indices.empty())
        return;

    auto* info = binding_data_.at(binding);

    if (info->pbo_ids.empty()) {
        const int pbo_count = 4;
        info->pbo_ids.resize(pbo_count);
        info->pbo_syncs.resize(pbo_count, 0);
        glGenBuffers(pbo_count, info->pbo_ids.data());
        for (int i = 0; i < pbo_count; ++i) {
            glBindBuffer(GL_PIXEL_PACK_BUFFER, info->pbo_ids[i]);
            glBufferData(GL_PIXEL_PACK_BUFFER, sizeof(T) * info->size, nullptr, GL_STREAM_READ);
        }
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        info->pbo_write_index = 0;
        info->pbo_read_index = 0;
        info->pbo_pending_count = 0;
    }

    if (info->pbo_pending_count == info->pbo_ids.size()) {
        const int drop_index = info->pbo_read_index;
        if (info->pbo_syncs[drop_index]) {
            glDeleteSync(info->pbo_syncs[drop_index]);
            info->pbo_syncs[drop_index] = 0;
        }

        info->pbo_read_index = (info->pbo_read_index + 1) % static_cast<int>(info->pbo_ids.size());
        --info->pbo_pending_count;
    }

    GLuint ssbo = info->id;
    const int write_index = info->pbo_write_index;
    GLuint pbo = info->pbo_ids[write_index];

    glBindBuffer(GL_COPY_READ_BUFFER, ssbo);
    glBindBuffer(GL_COPY_WRITE_BUFFER, pbo);

    size_t range_start = 0u;
    while (range_start < indices.size()) {
        size_t range_end = range_start + 1u;
        while (range_end < indices.size() && indices[range_end] == indices[range_end - 1u] + 1u)
            ++range_end;

        const size_t first_index = indices[range_start];
        const size_t range_count = range_end - range_start;
        glCopyBufferSubData(
            GL_COPY_READ_BUFFER,
            GL_COPY_WRITE_BUFFER,
            static_cast<GLintptr>(sizeof(T) * first_index),
            static_cast<GLintptr>(sizeof(T) * first_index),
            static_cast<GLsizeiptr>(sizeof(T) * range_count));

        range_start = range_end;
    }

    if (info->pbo_syncs[write_index]) {
        glDeleteSync(info->pbo_syncs[write_index]);
        info->pbo_syncs[write_index] = 0;
    }
    info->pbo_syncs[write_index] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

    info->pbo_write_index = (info->pbo_write_index + 1) % static_cast<int>(info->pbo_ids.size());
    ++info->pbo_pending_count;

    glBindBuffer(GL_COPY_READ_BUFFER, 0);
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
}

template <typename T>
void compute_shader::try_readback(GLuint binding, std::vector<T>& out) {
    if (!binding_data_.contains(binding)) { out.clear(); return; }
    auto* info = binding_data_.at(binding);
    const auto copy_latest_completed = [&]() {
        if (info->latest_completed_result.size() != sizeof(T) * info->size) {
            out.clear();
            return;
        }

        out.resize(info->size);
        memcpy(out.data(), info->latest_completed_result.data(), info->latest_completed_result.size());
    };

    if (info->pbo_ids.empty() || info->pbo_pending_count == 0) {
        copy_latest_completed();
        return;
    }

    const int ready_index = info->pbo_read_index;
    GLsync s = info->pbo_syncs[ready_index];
    if (!s) {
        info->pbo_read_index = (info->pbo_read_index + 1) % static_cast<int>(info->pbo_ids.size());
        --info->pbo_pending_count;
        copy_latest_completed();
        return;
    }

    GLenum res = glClientWaitSync(s, 0, 0);
    if (res == GL_ALREADY_SIGNALED || res == GL_CONDITION_SATISFIED) {
        GLuint pbo = info->pbo_ids[ready_index];
        glBindBuffer(GL_COPY_WRITE_BUFFER, pbo);
        void* ptr = glMapBufferRange(GL_COPY_WRITE_BUFFER, 0, sizeof(T) * info->size, GL_MAP_READ_BIT);
        if (ptr) {
            out.resize(info->size);
            memcpy(out.data(), ptr, sizeof(T) * info->size);
            info->latest_completed_result.resize(sizeof(T) * info->size);
            memcpy(info->latest_completed_result.data(), ptr, info->latest_completed_result.size());
            glUnmapBuffer(GL_COPY_WRITE_BUFFER);
        } else {
            copy_latest_completed();
        }
        glBindBuffer(GL_COPY_WRITE_BUFFER, 0);

        glDeleteSync(s);
        info->pbo_syncs[ready_index] = 0;
        info->pbo_read_index = (info->pbo_read_index + 1) % static_cast<int>(info->pbo_ids.size());
        --info->pbo_pending_count;
    } else {
        copy_latest_completed();
    }
}

template <typename T>
bool compute_shader::try_dequeue_readback(GLuint binding, std::vector<T>& out) {
    out.clear();

    if (!binding_data_.contains(binding))
        return false;

    auto* info = binding_data_.at(binding);
    if (info->pbo_ids.empty() || info->pbo_pending_count == 0)
        return false;

    const int ready_index = info->pbo_read_index;
    GLsync s = info->pbo_syncs[ready_index];
    if (!s) {
        info->pbo_read_index = (info->pbo_read_index + 1) % static_cast<int>(info->pbo_ids.size());
        --info->pbo_pending_count;
        return false;
    }

    GLenum res = glClientWaitSync(s, 0, 0);
    if (res != GL_ALREADY_SIGNALED && res != GL_CONDITION_SATISFIED)
        return false;

    GLuint pbo = info->pbo_ids[ready_index];
    glBindBuffer(GL_COPY_WRITE_BUFFER, pbo);
    void* ptr = glMapBufferRange(GL_COPY_WRITE_BUFFER, 0, sizeof(T) * info->size, GL_MAP_READ_BIT);
    if (ptr) {
        out.resize(info->size);
        memcpy(out.data(), ptr, sizeof(T) * info->size);
        info->latest_completed_result.resize(sizeof(T) * info->size);
        memcpy(info->latest_completed_result.data(), ptr, info->latest_completed_result.size());
        glUnmapBuffer(GL_COPY_WRITE_BUFFER);
    }
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);

    glDeleteSync(s);
    info->pbo_syncs[ready_index] = 0;
    info->pbo_read_index = (info->pbo_read_index + 1) % static_cast<int>(info->pbo_ids.size());
    --info->pbo_pending_count;

    return !out.empty();
}

template <typename T>
bool compute_shader::try_dequeue_readback_indices(GLuint binding, const std::vector<size_t>& indices, std::vector<T>& out) {
    out.clear();

    if (!binding_data_.contains(binding) || indices.empty())
        return false;

    auto* info = binding_data_.at(binding);
    if (info->pbo_ids.empty() || info->pbo_pending_count == 0)
        return false;

    const int ready_index = info->pbo_read_index;
    GLsync s = info->pbo_syncs[ready_index];
    if (!s) {
        info->pbo_read_index = (info->pbo_read_index + 1) % static_cast<int>(info->pbo_ids.size());
        --info->pbo_pending_count;
        return false;
    }

    GLenum res = glClientWaitSync(s, 0, 0);
    if (res != GL_ALREADY_SIGNALED && res != GL_CONDITION_SATISFIED)
        return false;

    GLuint pbo = info->pbo_ids[ready_index];
    glBindBuffer(GL_COPY_WRITE_BUFFER, pbo);
    out.resize(indices.size());

    size_t range_start = 0u;
    while (range_start < indices.size()) {
        size_t range_end = range_start + 1u;
        while (range_end < indices.size() && indices[range_end] == indices[range_end - 1u] + 1u)
            ++range_end;

        const size_t first_index = indices[range_start];
        const size_t range_count = range_end - range_start;
        void* ptr = glMapBufferRange(
            GL_COPY_WRITE_BUFFER,
            static_cast<GLintptr>(sizeof(T) * first_index),
            static_cast<GLsizeiptr>(sizeof(T) * range_count),
            GL_MAP_READ_BIT);

        if (!ptr) {
            out.clear();
            glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
            glDeleteSync(s);
            info->pbo_syncs[ready_index] = 0;
            info->pbo_read_index = (info->pbo_read_index + 1) % static_cast<int>(info->pbo_ids.size());
            --info->pbo_pending_count;
            return false;
        }

        memcpy(out.data() + range_start, ptr, sizeof(T) * range_count);
        glUnmapBuffer(GL_COPY_WRITE_BUFFER);
        range_start = range_end;
    }

    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
    glDeleteSync(s);
    info->pbo_syncs[ready_index] = 0;
    info->pbo_read_index = (info->pbo_read_index + 1) % static_cast<int>(info->pbo_ids.size());
    --info->pbo_pending_count;

    return !out.empty();
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

   if (old_data->size != data.size())
        reset_async_readback_buffers(*old_data);

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

template<typename T>
void compute_shader::update_ssbo_indices(const GLuint& binding, const std::vector<size_t>& indices, const std::vector<T>& data) {
    if (!binding_data_.contains(binding) || indices.empty() || indices.size() != data.size())
        return;

    use();

    auto* old_data = static_cast<ssbo_data<T>*>(binding_data_[binding]);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, old_data->id);

    size_t range_start = 0u;
    while (range_start < indices.size()) {
        size_t range_end = range_start + 1u;
        while (range_end < indices.size() && indices[range_end] == indices[range_end - 1u] + 1u)
            ++range_end;

        const size_t first_index = indices[range_start];
        const size_t range_count = range_end - range_start;
        glBufferSubData(
            GL_SHADER_STORAGE_BUFFER,
            static_cast<GLintptr>(sizeof(T) * first_index),
            static_cast<GLsizeiptr>(sizeof(T) * range_count),
            data.data() + range_start);

        range_start = range_end;
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}
