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

## License

See [LICENSE.txt](LICENSE.txt).