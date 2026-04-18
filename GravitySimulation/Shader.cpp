#include "Shader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <filesystem>

unsigned int shader::current_shader_id_ = 0;

static GLuint create_fallback_program() {
    const char* vs_src =
        "#version 330 core\n"
        "layout(location = 0) in vec3 aPos;\n"
        "uniform mat4 projection; uniform mat4 view; uniform mat4 model;\n"
        "void main() { gl_Position = projection * view * model * vec4(aPos,1.0); }\n";

    const char* fs_src =
        "#version 330 core\n"
        "out vec4 FragColor;\n"
        "void main() { FragColor = vec4(1.0,0.5,0.0,1.0); }\n";

    GLuint v = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(v, 1, &vs_src, nullptr);
    glCompileShader(v);
    GLint ok = 0; glGetShaderiv(v, GL_COMPILE_STATUS, &ok);
    if (!ok) { GLchar info[512]; glGetShaderInfoLog(v, 512, NULL, info); std::cerr << "FALLBACK VS COMPILE ERROR: " << info << std::endl; glDeleteShader(v); return 0; }

    GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(f, 1, &fs_src, nullptr);
    glCompileShader(f);
    glGetShaderiv(f, GL_COMPILE_STATUS, &ok);
    if (!ok) { GLchar info[512]; glGetShaderInfoLog(f, 512, NULL, info); std::cerr << "FALLBACK FS COMPILE ERROR: " << info << std::endl; glDeleteShader(v); glDeleteShader(f); return 0; }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, v);
    glAttachShader(prog, f);
    glLinkProgram(prog);
    GLint linked = 0; glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (!linked) { GLchar info[512]; glGetProgramInfoLog(prog, 512, NULL, info); std::cerr << "FALLBACK LINK ERROR: " << info << std::endl; glDeleteShader(v); glDeleteShader(f); glDeleteProgram(prog); return 0; }

    glDeleteShader(v); glDeleteShader(f);
    return prog;
}

static std::string read_file(const char* path) {
    if (!path) return {};
    std::filesystem::path p(path);
    std::ostringstream debug_paths;

    // Log current working directory for debugging
    try {
        debug_paths << "CWD=" << std::filesystem::current_path().string() << "\n";
    }
    catch (...) {}

    // Try the given path first
    if (std::filesystem::exists(p)) {
        std::ifstream f(p, std::ios::binary);
        if (f.is_open()) {
            std::ostringstream ss;
            ss << f.rdbuf();
            return ss.str();
        }
    }

    // If not found, try searching up the directory chain for the file
    std::filesystem::path cwd = std::filesystem::current_path();
    for (int i = 0; i < 6; ++i) {
        std::filesystem::path try_path = cwd / p;
        debug_paths << "try: " << try_path.string() << "\n";
        if (std::filesystem::exists(try_path)) {
            std::ifstream f(try_path, std::ios::binary);
            if (f.is_open()) {
                std::ostringstream ss;
                ss << f.rdbuf();
                return ss.str();
            }
        }
        if (cwd.has_parent_path()) cwd = cwd.parent_path(); else break;
    }

    // Last resort: try opening path as given (may fail and return empty)
    std::ifstream f(path, std::ios::binary);
    if (f.is_open()) {
        std::ostringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }

    // Print debug info to stderr to help diagnose missing file in Release
    std::cerr << "ERROR::READ_FILE failed for '" << path << "'\n" << debug_paths.str();
    return {};
}

std::string shader::load_shader_file(const char* shader_source) {
    return read_file(shader_source);
}

shader::shader() : asset(asset_type::SHADER) {
    this->id = 0;
    registered_uniforms_.reserve(16);
    sub_shader_type_ = shader_type::unknown;
    type_ = asset_type::SHADER;
    status_ = asset_status::UNLOADED;
}

shader::shader(const char* vertex_source, const char* fragment_source) : asset(asset_type::SHADER) {
    std::string vertexCode = load_shader_file(vertex_source);
    std::string fragmentCode = load_shader_file(fragment_source);

    if (vertexCode.empty() || fragmentCode.empty()) {
        const char* vs = vertex_source ? vertex_source : "(null)";
        const char* fs = fragment_source ? fragment_source : "(null)";
        std::cerr << "ERROR::SHADER::FILES_EMPTY: vertex='" << vs << "' fragment='" << fs << "'" << std::endl;
        status_ = asset_status::FAILED;
        return;
    }

    const char* vCode = vertexCode.c_str();
    const char* fCode = fragmentCode.c_str();

    GLuint vert = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert, 1, &vCode, nullptr);
    glCompileShader(vert);
    check_compiler_error(vert, "VERTEX");
    GLint ok = 0; glGetShaderiv(vert, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLchar infoLog[1024] = {0};
        glGetShaderInfoLog(vert, sizeof(infoLog), NULL, infoLog);
        std::cout << "ERROR: Vertex shader compilation failed:\n" << infoLog << std::endl;
        glDeleteShader(vert);
        status_ = asset_status::FAILED;
        return;
    }

    GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag, 1, &fCode, nullptr);
    glCompileShader(frag);
    check_compiler_error(frag, "FRAGMENT");
    glGetShaderiv(frag, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLchar infoLog[1024] = {0};
        glGetShaderInfoLog(frag, sizeof(infoLog), NULL, infoLog);
        std::cout << "ERROR: Fragment shader compilation failed:\n" << infoLog << std::endl;
        glDeleteShader(vert);
        glDeleteShader(frag);
        status_ = asset_status::FAILED;
        return;
    }

    this->id = glCreateProgram();
    glAttachShader(this->id, vert);
    glAttachShader(this->id, frag);
    glLinkProgram(this->id);
    check_compiler_error(this->id, "PROGRAM");
    GLint linked = 0; glGetProgramiv(this->id, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLchar infoLog[1024] = {0};
        glGetProgramInfoLog(this->id, sizeof(infoLog), NULL, infoLog);
        std::cout << "ERROR: Program linking failed:\n" << infoLog << std::endl;
        glDeleteShader(vert);
        glDeleteShader(frag);
        status_ = asset_status::FAILED;
        return;
    }

    glDeleteShader(vert);
    glDeleteShader(frag);

    registered_uniforms_.reserve(16);
    sub_shader_type_ = shader_type::visual_shader;
    type_ = asset_type::SHADER;
    status_ = asset_status::LOADED;

    // Debug: report successful load and associated sources
    try {
        std::cerr << "SHADER::LOADED vertex='" << (vertex_source?vertex_source:"(null)") << "' fragment='" << (fragment_source?fragment_source:"(null)") << "' program_id=" << this->id << "\n";
    }
    catch(...){}
}

void shader::use() {
    if (shader::current_shader_id_ != id) {
        shader::current_shader_id_ = id;
        glUseProgram(this->id);
    }
}

void shader::check_compiler_error(GLuint shader_id, const std::string& type) {
    GLint success;
    GLchar infoLog[1024];

    if (type != "PROGRAM") {
        glGetShaderiv(shader_id, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader_id, 1024, NULL, infoLog);
            std::cout << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n--------------------------------------------------" << std::endl;
            status_ = asset_status::FAILED;
        }

    }
    else {
        glGetProgramiv(shader_id, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shader_id, 1024, NULL, infoLog);
            std::cout << "ERROR::SHADER_LINKING_ERROR of type: " << type << "\n" << infoLog << "\n--------------------------------------------------" << std::endl;
            status_ = asset_status::FAILED;
        }
    }
}

void shader::set_uni_float(const std::string& u_name, const float& x) {
    if (!registered_uniforms_.contains(u_name)) register_uniform(u_name);
    glUniform1f(registered_uniforms_[u_name], x);
}
void shader::set_uni_int(const std::string& u_name, const int& x) {
    if (!registered_uniforms_.contains(u_name)) register_uniform(u_name);
    glUniform1i(registered_uniforms_[u_name], x);
}
void shader::set_uni_vec2(const std::string& u_name, const glm::vec2& n_vec2) {
    if (!registered_uniforms_.contains(u_name)) register_uniform(u_name);
    glUniform2fv(registered_uniforms_[u_name], 1, &n_vec2[0]);
}
void shader::set_uni_vec2(const std::string& u_name, const float& x, const float& y) {
    if (!registered_uniforms_.contains(u_name)) register_uniform(u_name);
    glUniform2f(registered_uniforms_[u_name], x, y);
}
void shader::set_uni_vec3(const std::string& u_name, const glm::vec3& n_vec3) {
    if (!registered_uniforms_.contains(u_name)) register_uniform(u_name);
    glUniform3fv(registered_uniforms_[u_name], 1, &n_vec3[0]);
}
void shader::set_uni_vec3(const std::string& u_name, const float& x, const float& y, const float& z) {
    if (!registered_uniforms_.contains(u_name)) register_uniform(u_name);
    glUniform3f(registered_uniforms_[u_name], x, y, z);
}
void shader::set_uni_vec4(const std::string& u_name, const glm::vec4& n_vec4) {
    if (!registered_uniforms_.contains(u_name)) register_uniform(u_name);
    glUniform4fv(registered_uniforms_[u_name], 1, &n_vec4[0]);
}
void shader::set_uni_vec4(const std::string& u_name, const float& x, const float& y, const float& z, const float& a) {
    if (!registered_uniforms_.contains(u_name)) register_uniform(u_name);
    glUniform4f(registered_uniforms_[u_name], x, y, z, a);
}

void shader::set_uniform_mat4(const std::string &u_name, const glm::mat4& n_mat4) {
    if (!registered_uniforms_.contains(u_name)) register_uniform(u_name);
    glUniformMatrix4fv(registered_uniforms_.at(u_name), 1, GL_FALSE, &n_mat4[0][0]);
}

bool shader::is_vaild() { return status_ == asset_status::LOADED; }

void shader::cleanup() { if (is_vaild()) { glDeleteProgram(this->id); status_ = asset_status::UNLOADED; } }

void shader::register_uniform(const std::string& u_name) { registered_uniforms_[u_name] = glGetUniformLocation(this->id, u_name.c_str()); }

