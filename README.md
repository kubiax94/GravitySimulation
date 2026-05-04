# C++ / OpenGL 3D Engine

<img width="1277" height="717" alt="Solar System" src="https://github.com/user-attachments/assets/fc8ff719-5176-476c-aaf2-32f7470e771d" />

A modular, component-based 3D engine built from scratch in **C++** with an **OpenGL 4.6** rendering backend.
The repository ships several fully-working example scenes that demonstrate physics, fluid dynamics, cloth simulation, and collision resolution.

---

## Gallery

| Scene | Preview |
|-------|---------|
| **Cloth simulation** | <img width="960" alt="Cloth" src="https://github.com/user-attachments/assets/b778e55c-0881-42b1-bdb0-171fffec4a52" /> |
| **Particle system (100k objects)** | <img width="960" alt="Particles" src="https://github.com/user-attachments/assets/1f4af843-c38d-47cc-a4ca-f4f2c34d7bb7" /> |
| **Fluid simulation** | <img width="960" alt="Fluid" src="https://github.com/user-attachments/assets/8942d397-0cee-4c11-89c0-09caa0c5ee4e" /> |

**Video demos:**
- [Cloth simulation](https://github.com/user-attachments/assets/d60c10da-0a40-4f57-ba22-afa23e51e2a7)
- [Particle system](https://github.com/user-attachments/assets/7a4f7264-bc39-4673-a578-41666a6a0235)
- [Fluid simulation](https://github.com/user-attachments/assets/9d2cd7af-e0cb-4d01-bb04-46cdf4d02624)

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
- **Scene loader** — async asset loading with progress feedback UI

### Rendering
- **GLSL shader wrapper** — compile, link, set uniforms (`float`, `vec3`, `mat4`) via a clean C++ API
- **GPU compute shader** — generic `compute_shader` that owns one or more SSBOs, dispatches work groups, and supports async PBO readback
- **Render pipeline** — `render_pipeline` collects `renderer` components each frame and flushes them in batches via `instance_manager` (GPU instanced drawing)
- **Renderer component** — attach a `Mesh` + `shader` to any node; optional per-draw uniform lambda for zero-cost customisation
- **Procedural mesh generation** — `g_shape::generate_sphere()`, `generate_grid()`, `generate_grid_lines()`, `generate_cube()`
- **Phong lighting** — point-light support in the default shaders; dedicated `sun.fs.shader` for emissive bodies
- **Procedural terrain** — height-map terrain meshes via `terrain_mesh_resource`
- **Free-look camera** — perspective camera component, WASD + mouse look
- **Asset manager** — `asset_manager` stores and retrieves `shader` and `Mesh` assets by UUID

### Physics & Simulation
- **`rigid_body` component** — position, velocity, accumulated force; semi-implicit Euler integration
- **`physics_system`** — collects registered `rigid_body` objects and dispatches either a CPU or GPU force pass each fixed step; supports async GPU readback
- **`unit_system`** — scales mass / distance / time so that arbitrary physical scenarios stay numerically stable
- **Collision system** — AABB broadphase (`collision_broadphase`) + narrowphase (`collision_narrowphase`); persistent contact manifolds; collision events (enter / stay / exit); impulse-based solver (`collision_solver`)
- **GPU cloth simulation** — spring-constraint solver on the GPU; wind forces, floor & AABB collisions (`cloth_simulation.glsl`)
- **GPU N-body gravity** — all-pairs force integration on the GPU (`gravity_simulation.glsl`)

### Fluid & Particles
- **`gpu_fluid_system_component`** — SPH-like particle simulation; surface reconstruction with particle surface blur and composite passes
- **`gpu_particle_system_component`** — lightweight GPU particle system (emitter, forces, lifetime)
- **Planetary water** — full render pipeline for spherical water: atlas generation, temporal stabilisation, tidal forces, wave propagation/rendering (`planetary_wave_field`, many dedicated shaders)

### UI
- **Immediate-mode-like UI** — widget tree: `ui_panel`, `ui_label_widget`, `ui_button_widget`, `ui_progress_bar_widget`, `ui_metric_row_widget`, `ui_stack_panel`, `ui_container_widget`
- **Profiler panel** — live frame-time breakdown rendered in-engine
- **Loading feedback** — animated progress panel shown while heavy scenes are initialised asynchronously

---

## Repository Layout

```
GravitySimulation/
│
│  ── Application entry ─────────────────────────────────────────────────────
├── GravitySimulation.cpp          # Entry point: creates engine, pushes simulation_state
├── engine.h / .cpp                # GLFW window, main loop, engine_state machine
├── engine_state.h                 # Abstract state interface (on_enter/update/render/…)
├── simulation_state.h / .cpp      # Concrete state: scene lifecycle, render pipeline, scene switching
│
│  ── Engine core ───────────────────────────────────────────────────────────
├── scene_node.h / .cpp            # Scene graph node; owns children + components
├── Scene.h / .cpp                 # Top-level scene: root node, camera list, systems
├── Component.h / .cpp             # Abstract component base; type-id system
├── Transform.h / .cpp             # Local transform (pos / rot / scale)
├── transformable.h / .cpp         # Interface: get/set global transform
├── i_scene_manager.h / .cpp       # Interface for scene-level registration
├── uuid.h / .cpp                  # Lightweight unique ID
├── sim_time.h / .cpp              # Delta-time + fixed-update accumulator
├── input_system.h / .cpp          # Keyboard & mouse state
├── frame_profiler.h / .cpp        # Scope-based performance profiler
├── scene_loader.h / .cpp          # Async asset loading helper
│
│  ── Rendering ─────────────────────────────────────────────────────────────
├── Renderer.h / .cpp              # Renderer component
├── render_pipeline.h / .cpp       # Per-frame batch collector + instanced flush
├── instance_manager.h / .cpp      # GPU instanced draw calls
├── Shader.h / .cpp                # GLSL program wrapper
├── compute_shader.h / .cpp        # Compute shader + multi-binding SSBO + async PBO readback
├── compute_group.h / .cpp         # Groups compute shaders with data bindings
├── shader_group.h / .cpp          # Shader grouping helper
├── ssbo_manager.h / .cpp          # SSBO lifecycle helper
├── Mesh.h / .cpp                  # VAO/VBO wrapper
├── g_shape.h / .cpp               # Procedural geometry generators (sphere, grid, cube)
├── Camera.h / .cpp                # Perspective camera component
├── texture.h / .cpp               # 2D texture helper
├── procedural_mesh_resource.h     # Runtime-generated meshes
├── terrain_mesh_resource.h        # Height-map terrain mesh
│
│  ── Shaders ──────────────────────────────────────────────────────────────
├── camera.vs/fs.shader            # Default camera shaders (Phong lighting)
├── default.vs/fs.shader           # Minimal pass-through shaders
├── lightsource.vs/fs.shader       # Emissive light-source shaders
├── sun.fs.shader                  # Sun-specific emissive fragment shader
├── gas_giant.fs.shader            # Gas giant surface
├── rocky_planet.vs/fs.shader      # Rocky planet surface + terrain
├── planet_atmosphere.fs.shader    # Atmospheric scattering
├── planet_clouds.fs.shader        # Cloud layer
├── planet_ocean.fs.shader         # Ocean surface
├── gpu_particle_system.vs/fs      # Particle system shaders
├── gpu_fluid_system.vs/fs         # Fluid surface shaders
├── ui_panel.vs/fs.shader          # UI panel shaders
├── ui_text.vs/fs.shader           # UI text shaders
│
│  ── Compute shaders ──────────────────────────────────────────────────────
├── gravity_simulation.glsl        # GPU N-body gravity integration
├── gravity_defor.glsl             # Gravity deformation field
├── cloth_simulation.glsl          # Cloth spring-constraint + wind solver
├── collision_detect.glsl          # Broadphase/narrowphase detection
├── collision_resolve.glsl         # Impulse-based collision resolver
├── collision_debug_gravity.glsl   # Gravity debug visualisation
├── fluid_predict.glsl             # SPH fluid prediction step
├── cosmic_background_particles.glsl
├── solar_halo_particles.glsl
├── planetary_wave_propagation.glsl
├── planetary_wave_render_filter.glsl
├── planetary_tide_field.glsl
│
│  ── Assets ────────────────────────────────────────────────────────────────
├── asset.h / .cpp                 # Base asset type + asset_type enum
├── asset_manager.h / .cpp         # Creates and stores shader / Mesh assets by UUID
├── resource.h                     # Generic resource base
├── base_manager.h / .cpp          # Templated manager base class
│
│  ── Physics ───────────────────────────────────────────────────────────────
├── rigid_body.h / .cpp            # Physics component (implements i_data_provider)
├── physics_data.h                 # std430-compatible data struct (CPU ↔ GPU)
├── physics_buffer.h               # Physics buffer helpers
├── physics_system.h / .cpp        # Force integration orchestrator; async GPU readback
├── unit_system.h / .cpp           # Physical unit scaling
│
│  ── Collision ─────────────────────────────────────────────────────────────
├── collider.h                     # Abstract collider interface
├── aabb_collider.h / .cpp         # Axis-aligned bounding-box collider
├── bounding_box.h                 # AABB data type
├── collision_broadphase.h / .cpp  # Broadphase pair detection
├── collision_narrowphase.h / .cpp # Narrowphase contact generation
├── collision_solver.h / .cpp      # Impulse-based resolution
├── collision_layers.h             # Layer-mask filtering
├── contact_manifold.h             # Persistent contact data
├── collision_event.h              # Enter / stay / exit events
│
│  ── Fluid & Particles ─────────────────────────────────────────────────────
├── gpu_fluid_system_component.h / .cpp
├── gpu_particle_system_component.h / .cpp
├── planetary_ocean_resource.h / .cpp
├── planetary_water_domain.h / .cpp
├── planetary_water_render_resource.h / .cpp
├── planetary_wave_field.h / .cpp
│
│  ── UI ─────────────────────────────────────────────────────────────────────
├── engine_ui.h / .cpp             # Top-level UI manager
├── ui_widget.h / .cpp             # Base widget
├── ui_panel.h / .cpp              # Panel container
├── ui_button_widget.h / .cpp
├── ui_label_widget.h / .cpp
├── ui_progress_bar_widget.h / .cpp
├── ui_stack_panel.h / .cpp
├── ui_container_widget.h / .cpp
├── ui_profiler_panel.h / .cpp     # Live frame-time profiler panel
├── ui_loading_panel.h / .cpp      # Async loading progress panel
├── ui_text_renderer.h / .cpp
├── ui_render_pipeline.h / .cpp
│
│  ── Example scenes ─────────────────────────────────────────────────────────
├── galactic_scene.h / .cpp        # Full solar system: planets, ocean, waves, atmosphere
├── galactic_simulation_test.h / .cpp
├── galactic_stress_scene.h / .cpp # N-body stress test (1 000+ bodies)
├── galactic_stress_test.h / .cpp
├── cloth_scene.h / .cpp           # GPU cloth simulation
├── fluid_scene.h / .cpp           # GPU SPH fluid simulation
└── collision_debug_scene.h / .cpp # AABB collision debug sandbox
```

---

## Requirements

| Dependency | Notes |
|------------|-------|
| **Windows** (x64) | Project is configured for Visual Studio 2022 (MSVC v143) |
| **OpenGL 4.6** | Required for compute shaders (`GL_ARB_compute_shader`) |
| **GLFW 3.4** | Windowing & input — install via `vcpkg` or download separately |
| **GLAD** | OpenGL loader — headers must be on the include path |
| **GLM** | Header-only maths library — must be on the include path |

> GLFW is **not** bundled in this repository. Install it via vcpkg (`vcpkg install glfw3:x64-windows`) or place the headers/libs manually on your include/library paths.

---

## Building

1. Install **GLFW 3.4**, **GLAD**, and **GLM** (e.g. via `vcpkg integrate install`).
2. Open `GravitySimulation.sln` in **Visual Studio 2022**.
3. Verify include paths for GLAD, GLM, and GLFW in project properties.
4. Select **Debug | x64** or **Release | x64** and build.

> Shader source paths are currently resolved relative to the working directory. Update them to match your local checkout if needed.

---

## Controls

| Key / Mouse | Action |
|-------------|--------|
| `W` / `S` | Move camera forward / backward |
| `A` / `D` | Strafe camera left / right |
| `Mouse move` (hold RMB) | Look around |
| `1–5` | Switch between example scenes |
| `Escape` | Exit / cancel camera focus |

---

## Example Scenes

Switch between scenes at runtime with keys **1–5**, or set the starting scene in `GravitySimulation.cpp`:

```cpp
app.change_state(std::make_unique<simulation_state>(
    simulation_state::example_scene_kind::fluid   // fluid / cloth / galactic / galactic_stress / collision_debug
));
```

### 1. Galactic Scene (`galactic_scene`)

Full solar-system simulation with:
- N-body GPU gravity integration (`gravity_simulation.glsl`)
- Planetary spherical water atlas — tidal forces, wave propagation, temporal stabilisation
- Atmospheric scattering, cloud layer, ocean surface shaders
- Background star field and galaxy particles (GPU particle systems)
- Procedural rocky and gas-giant planet surfaces

### 2. Cloth Scene (`cloth_scene`)

GPU cloth simulation:
- Grid of particles connected by spring constraints
- Wind forces with turbulence and pulsing
- AABB collision objects and floor bounce
- Entire solver runs in a single compute dispatch (`cloth_simulation.glsl`)

### 3. Fluid Scene (`fluid_scene`)

GPU SPH-like fluid:
- `gpu_fluid_system_component` — predict, resolve, integrate positions on GPU
- Surface reconstruction using particle surface blur and a composite fullscreen pass
- Real-time debug visualisation modes (pressure, velocity, density)

### 4. Galactic Stress Scene (`galactic_stress_scene`)

Performance benchmark:
- 1 000+ N-body particles in addition to the solar-system planets
- Tests GPU instancing throughput and N-body compute performance simultaneously

### 5. Collision Debug Scene (`collision_debug_scene`)

Interactive AABB physics sandbox:
- Static and dynamic AABB bodies
- Periodic spawn waves to stress-test broadphase + narrowphase
- Visual overlay for contact manifolds and bounding boxes

---

## Engine API — Usage Examples

### Minimal scene (from scratch)

The smallest possible scene that shows a spinning sphere on screen:

**`my_scene.h`**
```cpp
#pragma once
#include "Scene.h"

class my_scene final : public scene {
public:
    explicit my_scene(sim::time_sim* time);
    void update() override;

private:
    scene_node* sphere_node_ = nullptr;
    float angle_ = 0.f;
};
```

**`my_scene.cpp`**
```cpp
#include "my_scene.h"
#include "scene_node.h"
#include "Renderer.h"
#include "g_shape.h"
#include "Mesh.h"
#include "Shader.h"

my_scene::my_scene(sim::time_sim* time) : scene(time) {
    // 1. Build geometry and upload to GPU
    MeshData data = g_shape::generate_sphere(1.f, 32, 16);
    auto* mesh = get_asset_manager().create<Mesh>("sphere", data);

    // 2. Compile shaders
    auto* sh = get_asset_manager().create<shader>("basic",
        "default.vs.shader", "default.fs.shader");

    // 3. Create a scene node, attach renderer
    sphere_node_ = create_scene_node("Sphere");
    sphere_node_->add_component<renderer>(sphere_node_, sh, mesh);
    sphere_node_->set_global_position(glm::vec3(0.f, 0.f, -5.f));

    // 4. Camera
    auto* cam_node = create_scene_node("Camera");
    cam_node->add_component<Camera>(cam_node, 60.f, 1280.f / 720.f, 0.1f, 10000.f);
    cam_node->set_global_position(glm::vec3(0.f, 2.f, 5.f));
}

void my_scene::update() {
    scene::update();
    angle_ += 0.01f;
    sphere_node_->set_global_rotation(glm::vec3(0.f, angle_, 0.f));
}
```

Launch it from `GravitySimulation.cpp`:
```cpp
app.change_state(std::make_unique<simulation_state>(
    std::make_unique<my_scene>(&app.get_time())
));
```

---

### Creating a renderable node

```cpp
// Generate a sphere mesh and upload it to the GPU
MeshData sphereData = g_shape::generate_sphere(1.f, 64, 32);
auto* mesh = scene.get_asset_manager().create<Mesh>("rock_mesh", sphereData);

// Compile shaders from file
auto* sh = scene.get_asset_manager().create<shader>("rock_shader",
    "camera.vs.shader", "camera.fs.shader");

// Create node, attach renderer, set transform
scene_node* rock = myScene.create_scene_node("Rock");
rock->add_component<renderer>(rock, sh, mesh);
rock->set_global_position(glm::vec3(100.f, 0.f, 0.f));
rock->set_global_scale(glm::vec3(2.f));
```

---

### Adding physics (rigid body)

`physics_data` packs position, velocity, and accumulated force into three `vec4`s. The **`.w` component** of every `vec4` stores the body's **mass** — read directly by the GPU compute shader via the `std430` SSBO layout.

Use `unit_system` to convert real-world SI values into stable simulation scale:

```cpp
// 1 mass unit ≈ 1×10²⁴ kg,  1 dist unit ≈ 1×10⁶ km
unit_system u_sys(1e24f, 1e6f, 3.872e6f / 3600.f);

float mass_scaled = u_sys.mass(5.97e24f);          // Earth mass
float dist_scaled = u_sys.distance(149.6e6f);      // 1 AU
float sun_mass    = u_sys.mass(1.9885e30f);
float v           = sqrtf(u_sys.scaled_G() * sun_mass / dist_scaled); // Keplerian speed

auto* pd = new physics_data(
    glm::vec4(dist_scaled, 0.f, 0.f, mass_scaled), // position + mass
    glm::vec4(0.f, 0.f, v,   mass_scaled),         // velocity + mass
    glm::vec4(0.f, 0.f, 0.f, mass_scaled)          // force + mass
);

scene_node* earth = myScene.create_scene_node("Earth");
earth->add_component<rigid_body>(earth, pd);   // auto-registers with physics_system
earth->add_component<renderer>(earth, sh, mesh);
earth->set_global_position(pd->position);
```

---

### Writing a custom component

Inherit from `component`, expose a **static** `type_id()`, and override the virtual one.  
`add_component<T>` / `find_component<T>` / `has_component<T>` on `scene_node` will then work for your type automatically.

```cpp
class oscillate_component : public component {
    float amplitude_, frequency_, time_ = 0.f;
    glm::vec3 origin_;
public:
    static type_id_t type_id() { return ::get_type_id<oscillate_component>(); }
    type_id_t get_type_id() const override { return type_id(); }

    oscillate_component(scene_node* owner, glm::vec3 origin, float amp, float freq)
        : component(owner), origin_(origin), amplitude_(amp), frequency_(freq) {}

    void update(float dt) {
        time_ += dt;
        glm::vec3 pos = origin_;
        pos.y += amplitude_ * std::sinf(2.f * 3.14159f * frequency_ * time_);
        owner_node_->set_global_position(pos);
    }
};

// Usage
scene_node* buoy = myScene.create_scene_node("Buoy");
buoy->add_component<oscillate_component>(buoy, glm::vec3(50.f, 0.f, 0.f), 5.f, 0.5f);
```

---

### Scene graph traversal

```cpp
// Find a node anywhere in the scene by name
scene_node* sun = myScene.find_scene_node("Sun");

// Collect every renderer in the whole sub-tree
auto allRenderers = root->find_component<renderer>(search_options::recursive_down);

// Walk up to find the first physics body on this node or any ancestor
auto* rb = someChildNode->find_component<rigid_body>(search_options::parent_self_first);
if (rb)
    rb->apply_force(glm::vec3(0.f, 1000.f, 0.f));
```

| Flag | Meaning |
|------|---------|
| `include_self` | Check this node before traversing |
| `recursive_down` | Recurse into all children |
| `search_up` | Walk up through parents |
| `first` | Return as soon as one match is found |
| `child_self_first` | Self + all children, stop at first |
| `parent_self_first` | Self + all ancestors, stop at first |

---

### Writing a compute shader

A minimal GPU compute shader that doubles every float in a buffer:

**`double_values.glsl`**
```glsl
#version 460
layout(local_size_x = 64) in;

layout(std430, binding = 0) buffer Data {
    float values[];
};

void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i >= values.length()) return;
    values[i] *= 2.0;
}
```

**C++ side:**
```cpp
// 1. Create the compute shader
auto* cs = get_asset_manager().create<compute_shader>("doubler", "double_values.glsl");

// 2. Upload initial data into SSBO at binding 0
std::vector<float> data = { 1.f, 2.f, 3.f, 4.f };
cs->add_ssbo(0, data);

// 3. Dispatch — ceil(N / local_size_x) work groups
cs->use();
cs->dispatch(glm::uvec3((data.size() + 63) / 64, 1, 1));

// 4. Read results back immediately (blocking)
std::vector<float> result;
cs->get_binding_data(0, result);
// result == { 2, 4, 6, 8 }

// --- OR: async PBO readback (non-blocking, one frame of latency) ---
cs->enqueue_readback<float>(0);       // enqueue this frame
// ... next frame(s) ...
cs->try_readback(0, result);          // returns last completed result
```

To register a compute shader with the physics step so it runs automatically each fixed update:
```cpp
// stage = physics_gpu_stage::pre_integration or post_integration
scene.register_compute_shader(cs, physics_gpu_stage::pre_integration);
```

---

### Custom `unit_system` for physics scenarios

`unit_system` rescales the gravitational constant `G` automatically based on your chosen mass and distance scales:

```cpp
// Solar-system scale
unit_system solar(1e24f, 1e6f, 3.872e6f / 3600.f);

float earth_mass = solar.mass(5.97e24f);
float earth_dist = solar.distance(149.6e6f);
float sun_mass   = solar.mass(1.9885e30f);
float v_earth    = sqrtf(solar.scaled_G() * sun_mass / earth_dist);  // orbital speed

// Stellar-neighbourhood scale — just change the numbers
unit_system stellar(1.989e30f, 9.461e12f, 3.156e13f);
```

---

## Solar System — Planet Data

| Planet  | Mass (kg)      | Diameter (km) | Distance to Sun (km) |
|---------|---------------|---------------|----------------------|
| Mercury | 0.330×10²⁴    | 4 879         | 57.9×10⁶             |
| Venus   | 4.87×10²⁴     | 12 104        | 108.2×10⁶            |
| Earth   | 5.97×10²⁴     | 12 756        | 149.6×10⁶            |
| Mars    | 0.642×10²⁴    | 6 792         | 227.9×10⁶            |
| Jupiter | 1 898×10²⁴    | 142 984       | 778.6×10⁶            |
| Saturn  | 568×10²⁴      | 120 536       | 1 433.5×10⁶          |
| Uranus  | 86.8×10²⁴     | 51 118        | 2 872.5×10⁶          |
| Neptune | 102×10²⁴      | 49 528        | 4 495.1×10⁶          |

---

## GPU N-body Shader (`gravity_simulation.glsl`)

Each fixed-update tick `physics_system` uploads all `physics_data` structs into a single SSBO (binding 0) and dispatches `⌈N/64⌉` work groups:

```glsl
// Simplified kernel (see gravity_simulation.glsl for full source)
vec3 acceleration = vec3(0.0);
for (uint j = 0u; j < bodies.length(); ++j) {
    if (j == i) continue;
    vec3 dir   = bodies[j].position.xyz - bodies[i].position.xyz;
    float r2   = dot(dir, dir) + softening * softening;
    float inv3 = inversesqrt(r2) * (1.0 / r2);
    acceleration += G * bodies[j].position.w * dir * inv3;
}
bodies[i].velocity.xyz += acceleration * dt;
bodies[i].position.xyz += bodies[i].velocity.xyz * dt;
```

The `.w` component of `position` and `velocity` stores the body's mass (matches the `std430` layout of `physics_data`).

---

## Boot-up Sequence

```
main()
 └─ engine::init()            — creates GLFW window, initialises GLAD
     └─ engine::change_state(simulation_state)
         └─ simulation_state::on_enter()
             ├─ scene constructed (e.g. galactic_scene, fluid_scene, …)
             ├─ camera node positioned
             ├─ scene content initialised (nodes, renderers, physics, compute shaders)
             └─ render_pipeline ready

Per-frame loop:
 handle_input → fixed_update (physics tick) → update (sync render) → render (submit + flush)
```

---

## License

See [LICENSE.txt](LICENSE.txt).
