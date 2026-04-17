# C++ / OpenGL 3D Engine

A modular, component-based 3D engine built from scratch in C++ with an OpenGL 4.6 rendering backend.  
The repository includes a **Solar System gravity simulation** as a fully-working example scene that demonstrates how the engine's parts fit together.
<img width="1708" height="757" alt="image" src="https://github.com/user-attachments/assets/9eddf905-d09b-423b-84e9-62cce289404a" />
<img width="1285" height="753" alt="image" src="https://github.com/user-attachments/assets/05807d5b-f4df-43c8-bb7c-a2fc2120c118" />

---

## Engine Features

### Core
- **Component-based scene graph** — entities are `scene_node` objects; behaviour is added by attaching typed `component` instances
- **UUID node identity** — every node gets a unique ID at creation, enabling safe cross-frame references
- **Flexible transform hierarchy** — local / global position, rotation, scale; dirty-flag propagation; `forward` / `right` / `up` vectors
- **Fixed-update loop** — accumulator-based time-stepping with a configurable delta time (`sim::time`)
- **Centralised input system** — keyboard and mouse state queried from anywhere via `input_system`

### Rendering
- **GLSL shader wrapper** — compile, link, set uniforms (`float`, `vec3`, `mat4`) via a clean C++ API
- **GPU compute shader** — generic `compute_shader` that owns an SSBO, dispatches work groups, and reads results back
- **Renderer component** — attach a `Mesh` + `shader` to any node; optional per-draw uniform lambda for zero-cost customisation
- **Procedural mesh generation** — `Shape::GenerateSphere()`, grid, and other helpers
- **Phong lighting** — point-light support in the default shaders
- **Free-look camera** — perspective camera component, WASD + mouse look

### Physics
- **`rigid_body` component** — position, velocity, accumulated force; semi-implicit Euler integration
- **`physics_system`** — collects registered `rigid_body` objects and dispatches either a CPU or GPU force pass each fixed step
- **`unit_system`** — scales mass / distance / time so that arbitrary physical scenarios stay numerically stable

---

## Repository Layout

```
GravitySimulation/
│
│  ── Engine core ───────────────────────────────────────────────────────────
├── scene_node.h / .cpp        # Scene graph node; owns children + components
├── Scene.h / .cpp             # Top-level scene: root node, camera list, systems
├── Component.h / .cpp         # Abstract component base; type-id system
├── Transform.h / .cpp         # Local transform (pos / rot / scale)
├── transformable.h / .cpp     # Interface: get/set global transform
├── uuid.h / .cpp              # Lightweight unique ID
├── Time.h / .cpp              # Delta-time + fixed-update accumulator
├── input_system.h / .cpp      # Keyboard & mouse state
│
│  ── Rendering ─────────────────────────────────────────────────────────────
├── Renderer.h / .cpp          # Renderer component
├── Shader.h / .cpp            # GLSL program wrapper
├── compute_shader.h / .cpp    # Compute shader + SSBO helper
├── Mesh.h / .cpp              # VAO/VBO wrapper
├── Shape.h / .cpp             # Procedural geometry generators
├── Camera.h / .cpp            # Perspective camera component
├── camera.vs.shader           # Default vertex shader
├── camera.fs.shader           # Default fragment shader
├── default.vs/fs.shader       # Alternate shaders
├── lightsource.vs/fs.shader   # Light-source (emissive) shaders
│
│  ── Physics ───────────────────────────────────────────────────────────────
├── rigid_body.h / .cpp        # Physics component
├── physics_data.h             # std430-compatible data struct (CPU ↔ GPU)
├── physics_system.h / .cpp    # Force integration orchestrator
├── unit_system.h / .cpp       # Physical unit scaling
│
│  ── Example scene ─────────────────────────────────────────────────────────
├── galactic_simulation_test.h / .cpp   # Solar System scene setup
├── gravity_simulation.glsl             # GPU N-body compute shader
│
└── GravitySimulation.cpp      # Entry point: window, main loop, scene boot
```

---

## Requirements

| Dependency | Notes |
|------------|-------|
| **Windows** (x64) | Project is configured for Visual Studio 2022 (MSVC v143) |
| **OpenGL 4.6** | Required for compute shaders (`GL_ARB_compute_shader`) |
| **GLFW 3.4** | Windowing & input — bundled under `glfw-3.4/` |
| **GLAD** | OpenGL loader — headers must be on the include path |
| **GLM** | Header-only maths library — must be on the include path |

---

## Building

1. Open `GravitySimulation.sln` in **Visual Studio 2022**.
2. Ensure include paths for **GLAD**, **GLM**, and **GLFW** are set in project properties (or via `vcpkg`).
3. Select **Debug | x64** or **Release | x64** and build.

> **Note:** Shader source paths are currently hard-coded in `GravitySimulation.cpp` and `galactic_simulation_test.cpp`. Update them to match your local checkout before building.

---

## Controls (example scene)

| Key / Mouse | Action |
|-------------|--------|
| `W` / `S` | Move camera forward / backward |
| `A` / `D` | Strafe camera left / right |
| Mouse move (hold RMB) | Look around |

---

## Engine API — Usage Examples

The sections below show how to use the engine independently of the Solar System scene.

---

### 1. Creating a node and attaching built-in components

```cpp
// Renderable sphere — no physics
auto verts = Shape::GenerateSphere();
Mesh sphereMesh(verts);
shader myShader("camera.vs.shader", "camera.fs.shader");

scene_node* rock = myScene.create_scene_node("Rock");
rock->add_component<renderer>(rock, &myShader, &sphereMesh);
rock->set_global_position(glm::vec3(10.f, 0.f, 0.f));
rock->set_global_scale(glm::vec3(0.5f));
```

```cpp
// Add a physics body to the same node
physics_data pd(
    glm::vec4(10.f, 0.f, 0.f, 100.f),   // position.xyz + mass
    glm::vec4(0.f, 0.f, 1.5f, 100.f),   // velocity.xyz + mass
    glm::vec4(0.f, 0.f, 0.f,  100.f)    // accumulated force + mass
);
rock->add_component<rigid_body>(rock, pd);
// attach_to() is called internally; the body registers itself with physics_system
```

---

### 2. Writing a custom component

Inherit from `component`, expose a static `type_id()`, and override the virtual one.  
`add_component<T>`, `find_component<T>`, and `has_component<T>` on the node will work automatically.

```cpp
// rotate_component.h
#pragma once
#include "Component.h"

class rotate_component : public component {
    float speed_deg_s_;
public:
    static type_id_t type_id() { return ::get_type_id<rotate_component>(); }
    type_id_t get_type_id() const override { return type_id(); }

    rotate_component(scene_node* owner, float speed)
        : component(owner), speed_deg_s_(speed) {}

    void update(float dt) {
        glm::vec3 rot = owner_node_->get_rotation();
        rot.y += speed_deg_s_ * dt;
        owner_node_->set_rotation(rot);
    }
};
```

```cpp
// Attach and use
scene_node* cube = myScene.create_scene_node("Cube");
cube->add_component<rotate_component>(cube, 90.f); // 90 deg/s

// Retrieve anywhere — returns nullptr if not present
if (auto* r = cube->find_component<rotate_component>())
    r->update(deltaTime);
```

---

### 3. Traversing the scene graph

`find_component<T>` and `find_node` accept `search_options` bit-flags to control the direction and depth of the search.

```cpp
// Look up a node by name
scene_node* player = myScene.find_scene_node("Player");

// All renderer components in the entire sub-tree below root
auto allRenderers = root->find_component<renderer>(search_options::recursive_down);

// Check before accessing
if (player->has_component<rigid_body>()) {
    auto* rb = player->find_component<rigid_body>();
    rb->apply_force(glm::vec3(0.f, 500.f, 0.f)); // jump impulse
}
```

---

### 4. Per-draw shader uniforms

`renderer::draw()` takes an optional lambda that is called right before the GL draw call, so you can push per-object data without subclassing the renderer.

```cpp
myRenderer.draw(cam, [&](shader& s) {
    s.set_uni_vec3("objectColor", glm::vec3(1.f, 0.4f, 0.1f));
    s.set_uni_vec3("lightPos",    lightNode->get_global_position());
    s.set_uni_float("emissive",   0.0f);
});
```

---

### 5. Custom `unit_system` for any physics scenario

`unit_system` is a plain struct that scales mass, distance, and time.  
Swap it out to run a completely different physical scenario without touching physics code.

```cpp
// Default (Solar System scale)
unit_system solar(1e24f,            // mass   : ~1 Earth mass per unit
                  1e6f,             // dist   : 1 million km per unit
                  3.872e6f / 3600.f); // time : ~1 hour per unit

// Galactic scale
unit_system galactic(1.989e30f,     // 1 solar mass
                     9.461e12f,     // 1 light-year in km
                     3.156e13f);    // 1 million years in seconds
```

---

## Example Scene — Solar System Gravity Simulation

`galactic_simulation_test` uses the engine to run a real-time N-body simulation of the Solar System:

- Real masses and orbital distances (Mercury → Neptune)
- GPU-accelerated pairwise force calculation via `gravity_simulation.glsl` (OpenGL compute shader, SSBO)
- Semi-implicit Euler integration in `rigid_body::integrate()`
- The Sun provides Phong point-lighting for all planets

This scene is intentionally kept in a single file (`galactic_simulation_test.cpp`) so it can be replaced or extended without touching the engine.

---

## License

See [LICENSE.txt](LICENSE.txt).
