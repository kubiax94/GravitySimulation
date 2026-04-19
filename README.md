
# C++ / OpenGL 3D Engine

A modular, component-based 3D engine built from scratch in C++ with an OpenGL 4.6 rendering backend.  
The repository includes a **Solar System gravity simulation** as a fully-working example scene that demonstrates how the engine's parts fit together.
<img width="1708" height="757" alt="image" src="https://github.com/user-attachments/assets/9eddf905-d09b-423b-84e9-62cce289404a" />
<img width="1285" height="753" alt="image" src="https://github.com/user-attachments/assets/05807d5b-f4df-43c8-bb7c-a2fc2120c118" />

https://github.com/user-attachments/assets/9e9ff822-b78c-46fe-a886-e41e9fe412d9
https://github.com/user-attachments/assets/86716b93-f516-44f6-a268-134ce153d508

---

## Engine Features

### Core
- **Engine & state machine** — `engine` owns the GLFW window and the main loop; behaviour is swapped by pushing `engine_state` objects
- **Component-based scene graph** — entities are `scene_node` objects; behaviour is added by attaching typed `component` instances
- **UUID node identity** — every node gets a unique ID at creation, enabling safe cross-frame references
- **Flexible transform hierarchy** — local / global position, rotation, scale; dirty-flag propagation; `forward` / `right` / `up` vectors
- **Fixed-update loop** — accumulator-based time-stepping with a configurable delta time (`sim::time`)
- **Centralised input system** — keyboard and mouse state queried from anywhere via `input_system`
- **Frame profiler** — scope-based timer (`frame_profiler`) that prints average timings every N frames

### Rendering
- **GLSL shader wrapper** — compile, link, set uniforms (`float`, `vec3`, `mat4`) via a clean C++ API
- **GPU compute shader** — generic `compute_shader` that owns an SSBO, dispatches work groups, and reads results back
- **Render pipeline** — `render_pipeline` collects `renderer` components each frame and flushes them in batches via `instance_manager` (GPU instanced drawing)
- **Renderer component** — attach a `Mesh` + `shader` to any node; optional per-draw uniform lambda for zero-cost customisation
- **Procedural mesh generation** — `g_shape::generate_sphere()`, `generate_grid()`, `generate_grid_lines()`, `generate_cube()`
- **Phong lighting** — point-light support in the default shaders; dedicated `sun.fs.shader` for the light source
- **Free-look camera** — perspective camera component, WASD + mouse look
- **Asset manager** — `asset_manager` stores and retrieves `shader` and `Mesh` assets by UUID

### Physics
- **`rigid_body` component** — position, velocity, accumulated force; semi-implicit Euler integration
- **`physics_system`** — collects registered `rigid_body` objects and dispatches either a CPU or GPU force pass each fixed step; supports async GPU readback
- **`unit_system`** — scales mass / distance / time so that arbitrary physical scenarios stay numerically stable

---

## Repository Layout

```
GravitySimulation/
│
│  ── Application entry ─────────────────────────────────────────────────────
├── GravitySimulation.cpp      # Entry point: creates engine, pushes simulation_state
├── engine.h / .cpp            # GLFW window, main loop, engine_state machine
├── engine_state.h             # Abstract state interface (on_enter/update/render/…)
├── simulation_state.h / .cpp  # Concrete state: sets up scene, drives render pipeline
│
│  ── Engine core ───────────────────────────────────────────────────────────
├── scene_node.h / .cpp        # Scene graph node; owns children + components
├── Scene.h / .cpp             # Top-level scene: root node, camera list, systems
├── Component.h / .cpp         # Abstract component base; type-id system
├── Transform.h / .cpp         # Local transform (pos / rot / scale)
├── transformable.h / .cpp     # Interface: get/set global transform
├── i_scene_manager.h / .cpp   # Interface for scene-level registration (cameras, renderers…)
├── uuid.h / .cpp              # Lightweight unique ID
├── Time.h / .cpp              # Delta-time + fixed-update accumulator
├── input_system.h / .cpp      # Keyboard & mouse state
├── frame_profiler.h / .cpp    # Scope-based performance profiler
│
│  ── Rendering ─────────────────────────────────────────────────────────────
├── Renderer.h / .cpp          # Renderer component
├── render_pipeline.h / .cpp   # Per-frame batch collector + instanced flush
├── instance_manager.h / .cpp  # GPU instanced draw calls
├── Shader.h / .cpp            # GLSL program wrapper
├── compute_shader.h / .cpp    # Compute shader + SSBO helper
├── compute_group.h / .cpp     # Groups compute shaders with data bindings
├── shader_group.h / .cpp      # Shader grouping helper
├── ssbo_manager.h / .cpp      # SSBO lifecycle helper
├── Mesh.h / .cpp              # VAO/VBO wrapper
├── g_shape.h / .cpp           # Procedural geometry generators (sphere, grid, cube)
├── Camera.h / .cpp            # Perspective camera component
├── camera.vs.shader           # Camera vertex shader
├── camera.fs.shader           # Camera fragment shader
├── default.vs.shader          # Default vertex shader
├── default.fs.shader          # Default fragment shader
├── lightsource.vs.shader      # Light-source (emissive) vertex shader
├── lightsource.fs.shader      # Light-source (emissive) fragment shader
├── sun.fs.shader              # Sun-specific fragment shader
│
│  ── Assets ────────────────────────────────────────────────────────────────
├── asset.h / .cpp             # Base asset type + asset_type enum
├── asset_manager.h / .cpp     # Creates and stores shader / Mesh assets by UUID
├── resource.h / .cpp          # Generic resource base
├── base_manager.h / .cpp      # Templated manager base class
│
│  ── Physics ───────────────────────────────────────────────────────────────
├── rigid_body.h / .cpp        # Physics component (implements i_data_provider)
├── physics_data.h             # std430-compatible data struct (CPU ↔ GPU)
├── physics_buffer.h           # Physics buffer helpers
├── physics_system.h / .cpp    # Force integration orchestrator; async GPU readback
├── unit_system.h / .cpp       # Physical unit scaling
├── i_data_provider.h / .cpp   # Interface for GPU-uploadable data
│
│  ── Example scene ─────────────────────────────────────────────────────────
├── galactic_simulation_test.h / .cpp   # Solar System + stress-test scene setup
├── gravity_simulation.glsl             # GPU N-body compute shader
└── gravity_defor.glsl                  # Gravity deformation compute shader
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

> **Note:** Shader source paths are currently hard-coded relative to the working directory in `simulation_state.cpp` and `galactic_simulation_test.cpp`. Update them to match your local checkout before building.

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
auto meshData = g_shape::generate_sphere();
Mesh sphereMesh(meshData);
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

`render_pipeline::flush()` accepts an optional lambda that is called before each draw call, so you can push per-frame globals (lighting, view position, …) without subclassing the renderer.

```cpp
render_pipeline_.flush(cam_, [&](shader& s) {
    s.set_uni_vec3("objectColor", glm::vec3(1.f, 0.4f, 0.1f));
    s.set_uni_vec3("lightPos",    sunNode->get_global_position());
    s.set_uni_float("intensity",  0.75f);
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
- `stress_test()` helper adds hundreds of additional random bodies for GPU performance testing

This scene is set up in `galactic_simulation_test.cpp` and booted from `simulation_state`, so it can be replaced or extended without touching the engine core.
