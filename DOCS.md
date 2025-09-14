# Technical Documentation

## Architecture Overview

### System Design
The Gravity Simulation is built using a component-based architecture with clear separation between physics, rendering, and input systems. The design follows modern C++ practices with RAII, smart pointers where appropriate, and modular components.

## Core Systems

### 1. Physics System (`physics_system.h/.cpp`)

The physics system is the heart of the simulation, managing all gravitational calculations and object interactions.

#### Key Methods:
```cpp
class physics_system {
public:
    physics_system(unit_system* u_sys);
    
    // Entity management
    bool add(rigid_body* r_body);
    bool remove(rigid_body* rigid_body);
    
    // Physics computation
    void gravity_simulation_cpu();    // CPU-based calculations
    void gravity_simulation_gpu();    // GPU-based calculations
    
    // Main update loop
    void update(const float& dt);
};
```

#### Implementation Details:
- **N-body Problem**: Implements O(n²) gravitational force calculations
- **Numerical Integration**: Uses Verlet integration for stable physics
- **GPU Acceleration**: Compute shader implementation for parallel processing
- **Unit Scaling**: Proper scaling system for astronomical distances and masses

### 2. Scene Management (`Scene.h/.cpp`, `scene_node.h/.cpp`)

Hierarchical scene graph system for managing all objects in the simulation.

#### Scene Node System:
```cpp
class scene_node : public transformable {
public:
    // Component management
    template<typename T, typename... Args>
    T* add_component(Args&&... args);
    
    template<typename T>
    T* find_component();
    
    // Hierarchy management
    void add_child(scene_node* child);
    scene_node* find_child(const std::string& name);
    
    // Transform operations
    void set_global_position(const glm::vec3& position);
    void set_global_rotation(const glm::vec3& rotation);
    void set_global_scale(const glm::vec3& scale);
};
```

#### Features:
- **Component System**: ECS-like architecture for modular functionality
- **Hierarchy Support**: Parent-child relationships with transform inheritance
- **UUID System**: Unique identification for all scene nodes
- **Type Safety**: Template-based component management

### 3. Rendering System (`Renderer.h/.cpp`)

OpenGL-based rendering pipeline with modern shader support.

#### Renderer Component:
```cpp
class renderer : public component {
public:
    renderer(scene_node* owner, shader* shader, Mesh* mesh);
    
    // Rendering methods
    void draw(Camera* camera);
    void draw(Camera* camera, std::function<void(shader&)> uniform_callback);
    
    // Resource management
    void set_shader(shader* new_shader);
    void set_mesh(Mesh* new_mesh);
};
```

#### Shader System:
- **Vertex Shaders**: Transform vertices to screen space
- **Fragment Shaders**: Calculate pixel colors with lighting
- **Compute Shaders**: GPU physics calculations
- **Uniform Management**: Easy parameter passing to shaders

### 4. Physics Data Structure (`physics_data.h`)

Core data structure for physics simulation:

```cpp
struct physics_data {
    glm::vec4 position;           // xyz: position, w: mass
    glm::vec4 velocity;           // xyz: velocity, w: mass (duplicate)
    glm::vec4 accumulated_force;  // xyz: force, w: mass (duplicate)
    
    physics_data(const glm::vec4& pos, const glm::vec4& vel, const glm::vec4& force);
};
```

#### GPU Buffer Layout:
The physics data is structured for efficient GPU processing with proper alignment for compute shaders.

### 5. Unit System (`unit_system.h/.cpp`)

Scaling system to handle astronomical units in a stable simulation:

```cpp
struct unit_system {
    static const float const_G;  // Gravitational constant
    
    float mass_scale;           // Mass scaling factor (kg)
    float distance_scale;       // Distance scaling factor (km)  
    float scale_time;           // Time scaling factor (s)
    
    // Conversion methods
    float mass(float kg);                    // Convert kg to simulation units
    float distance(float km);                // Convert km to simulation units
    float time(float seconds);               // Convert seconds to simulation units
    float to_renderer_scale(float realm_km); // Convert to rendering scale
    
    const float scaled_G() const;            // Get scaled gravitational constant
};
```

#### Scaling Rationale:
- **Mass**: Scaled by 10²⁴ kg to avoid floating-point precision issues
- **Distance**: Scaled by 10⁶ km for manageable coordinate ranges
- **Time**: Scaled for real-time simulation with proper orbital mechanics

## GPU Compute Shader Implementation

### Gravity Simulation Shader (`gravity_simulation.glsl`)

```glsl
#version 460
layout(local_size_x = 64) in;

uniform float G;  // Gravitational constant

struct PhysicsBody {
    vec4 position;           // xyz: position, w: mass
    vec4 velocity;           // xyz: velocity, w: mass
    vec4 accumulated_force;  // xyz: force accumulator, w: mass
};

layout(std430, binding = 0) buffer PhysicsData {
    PhysicsBody bodies[];
};

void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i >= bodies.length()) return;
    
    vec3 pos_i = bodies[i].position.xyz;
    float mass_i = bodies[i].position.w;
    vec3 force = vec3(0);
    
    // Calculate gravitational forces from all other bodies
    for(int j = 0; j < bodies.length(); j++) {
        if (i == j) continue;  // Skip self-interaction
        
        vec3 pos_j = bodies[j].position.xyz;
        float mass_j = bodies[j].position.w;
        
        vec3 direction = pos_j - pos_i;
        float distance = length(direction);
        
        // Avoid division by zero and singularities
        if (distance < 0.01) continue;
        
        // Newton's law of universal gravitation: F = G * m1 * m2 / r²
        float force_magnitude = G * mass_i * mass_j / (distance * distance);
        force += normalize(direction) * force_magnitude;
    }
    
    // Store accumulated force
    bodies[i].accumulated_force.xyz = force;
}
```

### Performance Characteristics:
- **Parallel Processing**: Each celestial body processed in parallel
- **Memory Efficiency**: Structured buffer storage for optimal GPU access
- **Work Group Size**: 64 threads per work group for optimal occupancy
- **Synchronization**: Proper barrier usage for data consistency

## Input System (`input_system.h/.cpp`)

Static input management system for keyboard and mouse handling:

```cpp
class input_system {
private:
    static std::unordered_map<int, bool> current_keys_;
    static std::unordered_map<int, bool> current_mouse_buttons_;
    static std::unordered_map<int, bool> previous_keys_;
    
    static glm::vec3 mouse_pos_;
    static glm::vec3 mouse_previous_pos_;
    static glm::vec3 mouse_move_;

public:
    // State queries
    static bool is_key_down(int key);
    static bool is_key_pressed(int key);    // Single frame press
    static bool is_key_released(int key);   // Single frame release
    static bool is_button_down(int button);
    
    // Mouse handling
    static const glm::vec3& get_mouse_pos();
    static const glm::vec3& get_mouse_move();
    
    // Callbacks (called by GLFW)
    static void on_key_press(int key);
    static void on_key_release(int key);
    static void on_mouse_button_press(int button);
    static void on_mouse_button_release(int button);
    static void on_mouse_move(const double x, const double y);
};
```

## Camera System (`Camera.h/.cpp`)

First-person camera with perspective projection:

```cpp
class Camera : public transformable {
private:
    float aspect_ratio_;
    mutable bool dirty_projection_;
    mutable bool dirty_view_;
    
    glm::mat4 projectionMatrix;
    glm::mat4 viewMatrix;
    
    float yaw_;      // Horizontal rotation
    float pitch_;    // Vertical rotation
    float fov_;      // Field of view
    float near_;     // Near clipping plane
    float far_;      // Far clipping plane

public:
    Camera(scene_node* owner);
    
    // Matrix calculations
    const glm::mat4& get_projection_matrix() const;
    const glm::mat4& get_view_matrix() const;
    
    // Input processing
    void process_input(float deltaTime);
    void process_mouse_movement(float xoffset, float yoffset);
    
    // Configuration
    void set_aspect_ratio(float aspect);
    void set_fov(float fov);
    void set_clipping_planes(float near, float far);
};
```

### Camera Controls:
- **WASD**: Forward/backward and left/right movement
- **Mouse**: Look around (yaw and pitch rotation)
- **Space/Shift**: Up/down movement
- **Smooth Movement**: Delta-time based for frame-rate independence

## Memory Management

### Resource Lifecycle:
1. **Scene Creation**: Scene nodes created through scene factory methods
2. **Component Addition**: Components added via template methods with proper ownership
3. **Automatic Cleanup**: RAII-based resource management with proper destructors
4. **GPU Resources**: Proper OpenGL object cleanup in destructors

### Performance Considerations:
- **Object Pooling**: Reuse of mesh and shader objects across multiple entities
- **Batch Rendering**: Similar objects rendered together to minimize state changes
- **Frustum Culling**: (Not implemented) Could be added for performance with many objects
- **Level of Detail**: (Not implemented) Could be added for distant objects

## Extension Points

### Adding New Components:
```cpp
class my_component : public component {
public:
    static type_id_t type_id() { 
        static type_id_t id = generate_type_id();
        return id;
    }
    
    my_component(scene_node* owner) : component(owner) {}
    
    void attach_to(scene_node* node) override {
        // Component-specific initialization
    }
    
    type_id_t get_type_id() const override {
        return type_id();
    }
};
```

### Adding New Physics Algorithms:
1. Extend `physics_system` with new calculation methods
2. Implement compute shaders for GPU acceleration
3. Add appropriate unit scaling for new physical quantities
4. Update integration methods if needed

### Adding New Rendering Features:
1. Create new shader programs for different effects
2. Extend renderer component with new uniform parameters
3. Add new mesh generation functions in `Shape` class
4. Implement post-processing pipeline if needed

## Debugging and Profiling

### Debug Features:
- **Grid Rendering**: Visual reference for scale and positioning
- **Console Output**: Physics state debugging through standard output
- **OpenGL Debug Context**: Enable for detailed GPU debugging
- **Visual Studio Debugging**: Full C++ debugging support

### Performance Monitoring:
- **Frame Time**: Monitor via `Time` class delta measurements  
- **GPU Profiling**: Use graphics debuggers like RenderDoc or NSight
- **Physics Timing**: Separate timing for CPU vs GPU physics calculations
- **Memory Usage**: Monitor via Visual Studio diagnostic tools

## Future Enhancements

### Potential Improvements:
1. **Collision Detection**: Add sphere-sphere collision handling
2. **Trails/Orbits**: Render orbital paths for better visualization
3. **Time Controls**: Add pause, speed up, slow down functionality
4. **Save/Load**: Serialize simulation state to files
5. **Configuration**: External configuration files for simulation parameters
6. **Multiple Scenarios**: Support for different astronomical scenarios
7. **Improved Lighting**: Better planet shading and shadows
8. **Particle Systems**: Add comet tails, asteroid fields
9. **Sound**: Audio feedback for collisions and events
10. **UI**: ImGui integration for real-time parameter adjustment