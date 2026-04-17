# GravitySimulation

A real-time 3D gravitational simulation of the Solar System built with C++ and OpenGL. The simulation models gravitational interactions between the Sun and all eight planets using accurate physical data, and offloads the N-body force calculations to the GPU via an OpenGL compute shader.

---

## Features

- **N-body gravity simulation** — pairwise gravitational forces calculated for all bodies
- **GPU-accelerated physics** — force computation runs on the GPU using an OpenGL 4.6 compute shader (GLSL)
- **Accurate Solar System data** — real masses, diameters, and orbital distances for Mercury through Neptune
- **Scaled unit system** — physical quantities (mass, distance, time) are scaled to keep floating-point precision stable
- **Component-based scene graph** — entities are scene nodes that hold components (Camera, Renderer, RigidBody, …)
- **Free-look camera** — fly around the scene with keyboard (WASD) and mouse
- **Reference grid** — a flat grid rendered as a visual reference plane
- **Phong lighting** — the Sun acts as a point light source illuminating the planets

---

## Architecture

```
GravitySimulation/
├── GravitySimulation.cpp      # Entry point – window creation, main loop
├── Scene / scene_node         # Scene graph: tree of named nodes
├── Component / IComponent     # Base component interface
├── Camera                     # Free-look perspective camera component
├── Renderer                   # Mesh rendering component (supports TRIANGLES & LINES)
├── Mesh / Shape               # GPU mesh wrapper and procedural sphere / grid generators
├── Shader / compute_shader    # GLSL shader wrappers; compute_shader manages SSBO
├── physics_system             # Registers rigid bodies; drives CPU or GPU gravity step
├── rigid_body                 # Physics component: position, velocity, accumulated force
├── physics_data               # Plain data struct shared between CPU and GPU (std430)
├── unit_system                # Scaling helpers (mass / distance / time)
├── Transform / transformable  # 3-D transform hierarchy
├── input_system               # Centralised keyboard + mouse state
├── uuid                       # Lightweight unique-ID type for scene nodes
├── Time                       # Delta-time and fixed-update accumulator
├── galactic_simulation_test   # Spawns the Sun and planets with correct orbital velocities
└── gravity_simulation.glsl    # Compute shader – GPU N-body force pass
```

---

## Requirements

| Dependency | Notes |
|------------|-------|
| **Windows** (x64) | Project is configured for Visual Studio 2022 (MSVC v143) |
| **OpenGL 4.6** | Required for compute shaders (`GL_ARB_compute_shader`) |
| **GLFW 3.4** | Windowing & input – bundled under `glfw-3.4/` |
| **GLAD** | OpenGL loader – headers expected to be available on the include path |
| **GLM** | Mathematics library – header-only, must be on the include path |

---

## Building

1. Open `GravitySimulation.sln` in **Visual Studio 2022**.
2. Ensure the include paths for **GLAD**, **GLM**, and **GLFW** are configured in the project properties (or via `vcpkg`).
3. Select the **Debug | x64** or **Release | x64** configuration and build.

> **Note:** The shader source paths are currently hard-coded with absolute paths in `GravitySimulation.cpp` and `galactic_simulation_test.cpp`. Before building, update those paths to match your local checkout location.

---

## Controls

| Key / Mouse | Action |
|-------------|--------|
| `W` / `S` | Move camera forward / backward |
| `A` / `D` | Strafe camera left / right |
| Mouse move (hold RMB) | Look around |

---

## How It Works

### Unit Scaling

Raw SI values (kilograms, kilometres) are rescaled by `unit_system` before being handed to the physics engine:

| Quantity | Scale |
|----------|-------|
| Mass | 1 × 10²⁴ kg (≈ 1 Earth mass) |
| Distance | 1 × 10⁶ km (1 million kilometres) |
| Time | 1 simulated hour ≈ 3 872 seconds |

The gravitational constant **G** is scaled accordingly so that `F = G * m₁ * m₂ / r²` stays numerically well-behaved.

### Physics Loop

Each frame the engine accumulates a time-step deficit. While there is enough accumulated time, it calls `scene::update()` which ticks the `physics_system`:

1. All `physics_data` structs are copied into an SSBO (Shader Storage Buffer Object).
2. The compute shader (`gravity_simulation.glsl`) runs — each invocation handles one body and sums forces from all bodies with a lower index (Newton's third law is applied symmetrically).
3. Results are read back, positions/velocities are updated with semi-implicit Euler integration (`rigid_body::integrate`).
4. Each `rigid_body` applies its new position to the owning `scene_node`'s transform.

### Scene Graph

`scene` owns a flat list of `scene_node` objects identified by UUID. Each node holds a list of `component` pointers (Camera, Renderer, RigidBody, …). Components are queried by their static `type_id`.

---

## Implementation Examples

The engine is designed around three independent axes of extensibility: **components**, the **scene graph**, and the **unit/physics systems**. Below are copy-paste-ready snippets that show how each piece fits together.

---

### 1. Creating a scene node and attaching built-in components

```cpp
// 1a. A simple renderable sphere (no physics)
auto sphereVerts = Shape::GenerateSphere();
Mesh sphereMesh(sphereVerts);
shader myShader("camera.vs.shader", "camera.fs.shader");

scene_node* asteroid = cscene.create_scene_node("Asteroid");
asteroid->add_component<renderer>(asteroid, &myShader, &sphereMesh);
asteroid->set_global_position(glm::vec3(200.f, 0.f, 0.f));
asteroid->set_global_scale(glm::vec3(0.5f));
```

```cpp
// 1b. The same node, now also affected by gravity
unit_system u(1e24f, 1e6f, 3.872e6f / 3600.f);

physics_data pd(
    glm::vec4(200.f, 0.f, 0.f, u.mass(7.35e22f)),  // position + mass (Moon ~)
    glm::vec4(0.f, 0.f, 1.02f,  u.mass(7.35e22f)),  // velocity (orbital)
    glm::vec4(0.f, 0.f, 0.f,    u.mass(7.35e22f))   // accumulated force
);
asteroid->add_component<rigid_body>(asteroid, pd);
// rigid_body::attach_to() automatically registers the body with physics_system
```

---

### 2. Writing a custom component

Any class that inherits from `component` and implements `get_type_id()` becomes a first-class engine citizen. The node's template methods (`add_component<T>`, `find_component<T>`, `has_component<T>`) handle lifetime and lookup automatically.

```cpp
// my_spin_component.h
#pragma once
#include "Component.h"

class my_spin_component : public component {
    float speed_;
public:
    static type_id_t type_id() { return ::get_type_id<my_spin_component>(); }
    type_id_t get_type_id() const override { return type_id(); }

    my_spin_component(scene_node* owner, float speed)
        : component(owner), speed_(speed) {}

    // Called manually from your game-loop or scene::update()
    void update(float dt) {
        glm::vec3 rot = owner_node_->get_rotation();
        rot.y += speed_ * dt;
        owner_node_->set_rotation(rot);
    }
};
```

```cpp
// Usage
scene_node* moon = cscene.create_scene_node("Moon");
moon->add_component<my_spin_component>(moon, 45.f); // 45 deg/s

// Later, retrieve it from anywhere in the code:
auto* spin = moon->find_component<my_spin_component>();
if (spin) spin->update(deltaTime);
```

---

### 3. Querying the scene graph

`scene_node` provides flexible traversal via `search_options` flags.

```cpp
// Find a single node by name (registered in scene)
scene_node* earth = cscene.find_scene_node("Earth");

// Find all renderers anywhere below a root node
auto renderers = rootNode->find_component<renderer>(search_options::recursive_down);

// Check whether a node has a physics body before touching it
if (earth->has_component<rigid_body>()) {
    auto* rb = earth->find_component<rigid_body>();
    std::cout << "Earth mass: " << rb->get_mass() << "\n";
}
```

---

### 4. Custom unit scale for a different simulation

`unit_system` is a plain struct – swap the three scale factors to switch from a Solar-System scenario to, for example, a binary-star or a galaxy-scale simulation without touching any physics code.

```cpp
// Solar system (default)
unit_system solar(1e24f,   // mass scale  : ~1 Earth mass
                  1e6f,    // dist scale  : 1 million km
                  3.872e6f / 3600.f); // time scale : ~1 hour

// Galaxy-scale: mass in solar masses, distance in light-years, time in Myr
unit_system galactic(1.989e30f,   // 1 solar mass
                     9.461e12f,   // 1 light-year in km
                     3.156e13f);  // 1 million years in seconds
```

`physics_system` accepts a `unit_system*` pointer in its constructor, so a fresh `scene` with a different `unit_system` is all that is needed.

---

### 5. Custom draw uniforms per renderer

`renderer::draw()` accepts an optional `std::function<void(shader&)>` that is called right before the draw call, letting you push any per-object uniforms without subclassing the renderer.

```cpp
myRenderer.draw(cam, [&](shader& s) {
    s.set_uni_vec3("objectColor",  glm::vec3(0.2f, 0.6f, 1.0f)); // icy blue
    s.set_uni_vec3("lightPos",     sunNode->get_global_position());
    s.set_uni_float("emissive",    0.0f);
});
```

---

## License

See [LICENSE.txt](LICENSE.txt).