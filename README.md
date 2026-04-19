
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

### 1. Creating a renderable node

`g_shape` generates ready-to-upload `MeshData`. Pass it to `Mesh` which uploads it to the GPU (VAO/VBO). Then create a `scene_node`, attach a `renderer` component, and position it in the scene.

```cpp
// Generate a sphere mesh and upload it to the GPU
MeshData sphereData = g_shape::generate_sphere(1.f, 64, 32); // radius, segments, rings
Mesh sphereMesh(sphereData);

// Compile shaders from file
shader myShader("camera.vs.shader", "camera.fs.shader");

// Create node, attach renderer, set transform
scene_node* rock = myScene.create_scene_node("Rock");
rock->add_component<renderer>(rock, &myShader, &sphereMesh);
rock->set_global_position(glm::vec3(100.f, 0.f, 0.f));
rock->set_global_scale(glm::vec3(2.f));   // diameter ≈ 2 units
```

The `renderer` component registers itself with the scene automatically via `attach_to()`, so `scene::get_renderers()` will return it on the next frame.

---

### 2. Adding physics to a node

`physics_data` packs position, velocity, and accumulated force into three `vec4`s. The **`.w` component of every `vec4` stores the body's mass** — this is what the GPU compute shader reads directly via the `std430` SSBO layout.

Use `unit_system` to convert real-world SI values into the simulation's internal scale before storing them:

```cpp
// 1 unit of mass   = 1e24 kg  (~1 Earth mass)
// 1 unit of distance = 1e6 km (~1 million km)
unit_system u_sys(1e24f, 1e6f, 3.872e6f / 3600.f);

float mass_scaled     = u_sys.mass(5.97e24f);       // Earth mass in simulation units
float distance_scaled = u_sys.distance(149.6e6f);   // 1 AU in simulation units

// Circular-orbit tangential velocity: v = sqrt(G_scaled * M_sun / r)
float sun_mass_scaled = u_sys.mass(1.9885e30f);
float v = sqrtf(u_sys.scaled_G() * sun_mass_scaled / distance_scaled);

// physics_data layout: vec4(pos.xyz, mass), vec4(vel.xyz, mass), vec4(force.xyz, mass)
auto* pd = new physics_data(
    glm::vec4(distance_scaled, 0.f, 0.f, mass_scaled),  // start on X-axis
    glm::vec4(0.f, 0.f, v, mass_scaled),                // tangential velocity along Z
    glm::vec4(0.f, 0.f, 0.f, mass_scaled)               // no initial force
);

scene_node* earth = myScene.create_scene_node("Earth");
earth->add_component<rigid_body>(earth, pd);   // registers with physics_system automatically
earth->add_component<renderer>(earth, &myShader, &sphereMesh);
earth->set_global_position(pd->position);      // sync visual position to physics position
earth->set_global_scale(glm::vec3(u_sys.to_renderer_scale(12756.f))); // diameter in km → units
```

The `rigid_body::attach_to()` override calls `physics_system::add()` so the body is automatically picked up in the next physics tick.

---

### 3. Writing a custom component

Inherit from `component`, expose a **static** `type_id()` (used by the hash-map lookup), and override the virtual one. The templated `add_component<T>` / `find_component<T>` / `has_component<T>` methods on `scene_node` will then work for your type with no further registration.

```cpp
// oscillate_component.h
#pragma once
#include "Component.h"
#include <cmath>

class oscillate_component : public component {
    float amplitude_;   // units
    float frequency_;   // Hz
    float time_ = 0.f;
    glm::vec3 origin_;

public:
    static type_id_t type_id() { return ::get_type_id<oscillate_component>(); }
    type_id_t get_type_id() const override { return type_id(); }

    oscillate_component(scene_node* owner, glm::vec3 origin, float amplitude, float freq)
        : component(owner), origin_(origin), amplitude_(amplitude), frequency_(freq) {}

    void update(float dt) {
        time_ += dt;
        glm::vec3 pos = origin_;
        pos.y += amplitude_ * std::sinf(2.f * 3.14159f * frequency_ * time_);
        owner_node_->set_global_position(pos);
    }
};
```

```cpp
// Attach in on_enter / scene setup
scene_node* buoy = myScene.create_scene_node("Buoy");
buoy->add_component<oscillate_component>(buoy,
    glm::vec3(50.f, 0.f, 0.f), // pivot point
    5.f,                        // ±5 units amplitude
    0.5f);                      // 0.5 Hz

// Call in your fixed_update or update loop
if (auto* osc = buoy->find_component<oscillate_component>())
    osc->update(dt);
```

---

### 4. Scene graph traversal

`find_component<T>` and `find_node` take `search_options` bit-flags that control direction (up the parent chain vs. down through children) and whether to stop at the first match.

```cpp
// Find a node anywhere in the scene by name
scene_node* sun = myScene.find_scene_node("Sun");

// Collect every renderer in the whole sub-tree (returns std::vector<renderer*>)
auto allRenderers = root->find_component<renderer>(search_options::recursive_down);

// Walk up the hierarchy to find the first physics body on this node or any ancestor
auto* rb = someChildNode->find_component<rigid_body>(search_options::parent_self_first);
if (rb)
    rb->apply_force(glm::vec3(0.f, 1000.f, 0.f));

// Safe single-component access on the node itself (includes self + full tree, stops at first)
if (myNode->has_component<Camera>()) {
    Camera* cam = myNode->find_component<Camera>();
    // use cam …
}
```

Available flags (combinable with `|`):

| Flag | Meaning |
|------|---------|
| `include_self` | Check this node before traversing |
| `recursive_down` | Recurse into all children |
| `search_up` | Walk up through parents |
| `first` | Return as soon as one match is found |
| `child_self_first` | Self + all children, stop at first |
| `parent_self_first` | Self + all ancestors, stop at first |

---

### 5. Full render loop

The render pipeline is driven manually each frame from your `engine_state::render()` override:

```cpp
void my_state::render(engine& eng) {
    // 1. Clear the framebuffer
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 2. Begin a new frame (clears the internal draw list)
    render_pipeline_.begin_frame();

    // 3. Submit all renderers registered in the scene
    for (auto* r : scene_->get_renderers())
        render_pipeline_.submit(r);

    // 4. Flush: sorts by shader/mesh, issues instanced draw calls.
    //    The lambda runs once per batch and lets you push per-frame uniforms.
    render_pipeline_.flush(cam_, [&](shader& s) {
        s.set_uni_vec3("objectColor", glm::vec3(1.0f, 0.5f, 0.31f));
        s.set_uni_vec3("lightColor",  glm::vec3(1.0f, 0.8f,  0.3f));
        s.set_uni_vec3("viewPos",     cam_->get_transform()->get_global_position());
        s.set_uni_vec3("lightPos",    sun_node_->get_global_position());
        s.set_uni_float("intensity",  0.75f);
    });
}
```

> **`sync_render()`** must be called from `update()` (every display frame) to interpolate visual positions between the last two fixed-update steps, keeping the rendering smooth even when the physics tick rate is lower than the frame rate.

---

### 6. Custom `unit_system` for any physics scenario

`unit_system` converts real SI values into a numerically stable simulation space. The gravitational constant `G = 6.674×10⁻¹¹` is automatically rescaled by `scaled_G()` based on your chosen mass and distance scales, so you never touch the physics code to change the scenario.

```cpp
// Solar-system scale
// 1 mass unit   ≈ 1×10²⁴ kg  (roughly 1 Earth mass)
// 1 dist unit   ≈ 1×10⁶  km  (1 million km)
unit_system solar(1e24f, 1e6f, 3.872e6f / 3600.f);

// Convert real data before storing in physics_data
float earth_mass = solar.mass(5.97e24f);          // → ~5.97 simulation mass units
float earth_dist = solar.distance(149.6e6f);      // → ~149.6 simulation distance units
float sun_mass   = solar.mass(1.9885e30f);

// Keplerian orbital speed: v = sqrt(G' * M / r)
float v_earth = sqrtf(solar.scaled_G() * sun_mass / earth_dist);

// Diameter in km → renderer scale (Earth diameter / 12742 km baseline)
float render_scale = solar.to_renderer_scale(12756.f); // ≈ 1.0 for Earth
```

For a completely different scenario, just swap the numbers — everything else adapts automatically:

```cpp
// Stellar neighbourhood scale
// 1 mass unit = 1 solar mass,  1 dist unit = 1 light-year,  1 time unit = 1 million years
unit_system stellar(1.989e30f, 9.461e12f, 3.156e13f);
```

---

## Example Scene — Solar System Gravity Simulation

`galactic_simulation_test` bootstraps a live N-body simulation of the Solar System in around 100 lines of code. Here is how it all fits together.

### Planet data

All eight planets are described by a simple struct:

```cpp
struct planet_data { std::string name; float mass; float diameter; float distance_to_sun; };
```

| Planet | Mass (kg) | Diameter (km) | Distance to Sun (km) |
|--------|-----------|---------------|----------------------|
| Mercury | 0.330×10²⁴ | 4 879 | 57.9×10⁶ |
| Venus | 4.87×10²⁴ | 12 104 | 108.2×10⁶ |
| Earth | 5.97×10²⁴ | 12 756 | 149.6×10⁶ |
| Mars | 0.642×10²⁴ | 6 792 | 227.9×10⁶ |
| Jupiter | 1 898×10²⁴ | 142 984 | 778.6×10⁶ |
| Saturn | 568×10²⁴ | 120 536 | 1 433.5×10⁶ |
| Uranus | 86.8×10²⁴ | 51 118 | 2 872.5×10⁶ |
| Neptune | 102×10²⁴ | 49 528 | 4 495.1×10⁶ |

### Scene setup flow (`init_gravity_test`)

1. A `unit_system` is created with Solar System scaling.
2. The Sun node is created, given a `rigid_body` (stationary, mass ≈ 1.99×10³⁰ kg scaled), and a `renderer` using `lightsource.vs.shader` + `sun.fs.shader`.
3. For each planet the Keplerian circular-orbit velocity is derived: `v = sqrt(G' * M_sun / r)`, then a `physics_data` struct is filled with the scaled mass, starting position on the X-axis, and the tangential velocity along Z. Each planet gets a `rigid_body` (auto-registered with `physics_system`) and a `renderer`.
4. Visual scale is set from real diameter: `planet.diameter / 12756` so Earth = 1 renderer unit.

### GPU N-body compute shader (`gravity_simulation.glsl`)

Every fixed-update tick `physics_system::compute_gpu()` uploads all `physics_data` structs into a single SSBO (binding 0) and dispatches `ceil(N / 64)` work groups. Inside the shader:

- Each thread `i` iterates over all other bodies `j` and accumulates the gravitational force: `F = G * m_i * m_j / r²` directed along the unit vector `(pos_j - pos_i)`.
- Acceleration, velocity and position are integrated in-place on the GPU (semi-implicit Euler), so no readback is needed for the physics step — only the final positions are read back asynchronously to drive the visual transforms.

```glsl
// Simplified kernel (see gravity_simulation.glsl for full source)
vec3 force = vec3(0);
for (int j = 0; j < bodies.length(); ++j) {
    if (int(i) == j) continue;
    vec3 dir  = bodies[j].position.xyz - bodies[i].position.xyz;
    float r   = length(dir);
    force    += normalize(dir) * (G * bodies[i].position.w * bodies[j].position.w / (r * r));
}
vec3 accel = force / bodies[i].position.w;
bodies[i].velocity.xyz += accel * dt;
bodies[i].position.xyz += bodies[i].velocity.xyz * dt;
```

### Stress test

`simtest::stress_test(scene, renders, count)` adds `count` additional bodies (default **1 000**) with random positions (±1 000 units) and random masses, all sharing the same sphere mesh. This tests GPU instancing throughput and N-body compute performance simultaneously. The total body count including planets is **1 009**.

### Boot-up sequence

```
main()
 └─ engine::init()          — creates GLFW window, initialises GLAD
     └─ engine::change_state(simulation_state)
         └─ simulation_state::on_enter()
             ├─ scene created, camera node added at (0, 280, 400) angled -45°
             ├─ grid mesh (64×64 lines, 50-unit tiles) added for visual reference
             ├─ simtest::init_gravity_test()   — Sun + 8 planets
             └─ simtest::stress_test(…, 1000) — 1 000 random bodies
```

Every frame thereafter follows the pattern: `handle_input → fixed_update (physics tick) → update (sync render positions) → render (submit + flush pipeline)`.

### Lighting

The Sun node is located via `find_scene_node("Sun")` and its world position is passed as `lightPos` to the Phong fragment shader on every draw call. Planets use `camera.fs.shader` (Phong diffuse + specular); the Sun itself uses `sun.fs.shader` which renders it as a fully emissive body regardless of the light position.
