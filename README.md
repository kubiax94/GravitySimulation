# Gravity Simulation

**3D Orbital Mechanics Simulation using OpenGL and C++**

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-Windows-lightgrey.svg)
![C++](https://img.shields.io/badge/C%2B%2B-17-blue.svg)
![OpenGL](https://img.shields.io/badge/OpenGL-4.6-red.svg)

A real-time 3D gravity simulation that models planetary motion in our solar system. Features both CPU and GPU-accelerated physics calculations with realistic orbital mechanics, accurate planetary data, and interactive 3D visualization.

## 🌟 Features

- **Realistic Solar System Simulation**: All 8 planets with accurate masses, sizes, and orbital distances
- **Dual Physics Engine**: Choose between CPU and GPU-accelerated gravity calculations
- **3D Visualization**: OpenGL-based rendering with realistic planetary scales
- **Interactive Camera**: Free-look camera with WASD movement and mouse controls
- **Real Physics**: Newton's law of universal gravitation with proper orbital mechanics
- **Compute Shaders**: GPU-accelerated N-body physics simulation for performance
- **Component System**: Modular ECS-like architecture for game objects
- **Unit Scaling**: Proper unit conversion system for astronomical scales

## 🚀 Quick Start

### Prerequisites

- **Visual Studio 2022** (with C++ development tools)
- **Windows 10/11** (x64)
- **Graphics Card** supporting OpenGL 4.6+ and compute shaders
- **Git** for cloning the repository

### Building

1. **Clone the repository**:
   ```bash
   git clone https://github.com/kubiax94/GravitySimulation.git
   cd GravitySimulation
   ```

2. **Open in Visual Studio**:
   - Open `GravitySimulation.sln` in Visual Studio 2022
   - Set build configuration to `Debug` or `Release`
   - Set platform to `x64`

3. **Build the project**:
   - Press `Ctrl+Shift+B` or go to Build → Build Solution
   - The executable will be created in the `Debug/` or `Release/` folder

4. **Run the simulation**:
   - Press `F5` to run with debugging or `Ctrl+F5` without debugging
   - A window will open showing the 3D solar system simulation

### Controls

| Key/Action | Function |
|------------|----------|
| **W/A/S/D** | Move camera forward/left/backward/right |
| **Mouse Move** | Look around (first-person camera) |
| **Mouse Buttons** | Camera interaction |
| **Space/Shift** | Move camera up/down |

## 🏗️ Architecture

### Core Components

#### Physics System (`physics_system.h/.cpp`)
- Manages all rigid bodies in the simulation
- Handles both CPU and GPU physics calculations
- N-body gravity simulation using Newton's law of universal gravitation
- Efficient compute shader implementation for GPU acceleration

#### Rendering Engine
- **Scene Management** (`Scene.h/.cpp`): Hierarchical scene graph
- **Renderer** (`Renderer.h/.cpp`): OpenGL rendering pipeline
- **Shader System** (`Shader.h/.cpp`): GLSL shader management
- **Mesh System** (`Mesh.h/.cpp`): 3D geometry handling
- **Camera** (`Camera.h/.cpp`): 3D camera with perspective projection

#### Component System
- **scene_node** (`scene_node.h/.cpp`): Base entity in the scene hierarchy
- **transformable** (`transformable.h/.cpp`): Position, rotation, scale operations
- **rigid_body** (`rigid_body.h/.cpp`): Physics properties and integration
- **Component** (`Component.h/.cpp`): Base component interface

#### Support Systems
- **Unit System** (`unit_system.h/.cpp`): Converts real-world units to simulation scale
- **Input System** (`input_system.h/.cpp`): Keyboard and mouse input handling
- **Time System** (`Time.h/.cpp`): Fixed timestep physics updates

### Physics Implementation

The simulation uses Newton's law of universal gravitation:

```
F = G * (m1 * m2) / r²
```

Where:
- `F` = gravitational force between two objects
- `G` = gravitational constant (6.674 × 10⁻¹¹ m³/kg⋅s²)
- `m1, m2` = masses of the two objects
- `r` = distance between object centers

#### GPU Compute Shader (`gravity_simulation.glsl`)
- Parallel calculation of gravitational forces
- Each thread processes one celestial body
- Shared memory for efficient data access
- Real-time N-body simulation at 60+ FPS

#### Unit Scaling
Real astronomical values are scaled for stable simulation:
- **Mass Scale**: 1×10²⁴ kg (Earth masses)
- **Distance Scale**: 1×10⁶ km (millions of kilometers)  
- **Time Scale**: ~1 hour = 3,872 km/h conversion

### Planetary Data

The simulation includes accurate data for all planets:

| Planet | Mass (×10²⁴ kg) | Diameter (km) | Distance from Sun (×10⁶ km) |
|--------|------------------|---------------|------------------------------|
| Mercury | 0.330 | 4,879 | 57.9 |
| Venus | 4.87 | 12,104 | 108.2 |
| Earth | 5.97 | 12,756 | 149.6 |
| Mars | 0.642 | 6,792 | 227.9 |
| Jupiter | 1,898 | 142,984 | 778.6 |
| Saturn | 568 | 120,536 | 1,433.5 |
| Uranus | 86.8 | 51,118 | 2,872.5 |
| Neptune | 102 | 49,528 | 4,495.1 |

## 🎮 Usage Examples

### Basic Simulation
The main simulation loop in `GravitySimulation.cpp` demonstrates:
```cpp
// Initialize scene and physics
scene cscene(time1);
simtest::init_gravity_test(&cscene, planet_renders);

// Main loop
while (!glfwWindowShouldClose(window)) {
    // Update physics at fixed timestep
    while (time1->should_fixed_update()) {
        cscene.update();
        time1->reduce_accumulator();
    }
    
    // Render all objects
    for (auto* render : planet_renders) {
        render->draw(cam, shader_callback);
    }
}
```

### Adding Custom Objects
```cpp
// Create a new celestial body
auto* asteroid_node = scene.create_scene_node("Asteroid");

// Set up physics data (position, velocity, mass)
physics_data asteroid_physics(
    glm::vec4(distance, 0, 0, mass),      // position + mass
    glm::vec4(0, 0, orbital_velocity, 0), // velocity
    glm::vec4(0, 0, 0, 0)                 // accumulated force
);

// Add components
asteroid_node->add_component<rigid_body>(asteroid_node, asteroid_physics);
asteroid_node->add_component<renderer>(asteroid_node, shader, mesh);
```

## 🛠️ Development

### Project Structure
```
GravitySimulation/
├── GravitySimulation/          # Main source directory
│   ├── *.cpp, *.h             # C++ source and header files
│   ├── *.shader               # GLSL shader files
│   ├── *.glsl                 # Compute shader files
│   └── glfw-3.4/              # GLFW library
├── Debug.zip                   # Debug build artifacts
├── GravitySimulation.sln       # Visual Studio solution
└── README.md                   # This file
```

### Key Files
- **GravitySimulation.cpp**: Main application entry point
- **galactic_simulation_test.cpp**: Solar system setup and configuration
- **physics_system.cpp**: Core physics calculations
- **gravity_simulation.glsl**: GPU compute shader for physics
- **Scene.cpp**: Scene management and update loop

### Building from Source
1. Ensure all dependencies are installed (see Prerequisites)
2. Open the Visual Studio solution file
3. Set the target platform to x64
4. Build and run the project

### Dependencies
- **OpenGL 4.6+**: For graphics rendering and compute shaders
- **GLFW 3.4**: For window management and input handling
- **GLM**: For mathematical operations (vectors, matrices)
- **GLAD**: For OpenGL function loading

## 🐛 Troubleshooting

### Common Issues

**Black screen or no rendering**:
- Ensure your graphics card supports OpenGL 4.6
- Check that compute shaders are supported
- Verify shader files are in the correct directory

**Compilation errors**:
- Make sure you're using Visual Studio 2022 with C++17 support
- Check that all paths in the project are correct
- Ensure GLFW library is properly linked

**Performance issues**:
- Try switching from GPU to CPU physics calculation
- Reduce the number of celestial bodies in the simulation
- Check that you're running in Release mode for better performance

**Camera not responding**:
- Make sure the window has focus
- Check that mouse and keyboard callbacks are properly set up
- Verify input system initialization

### Performance Tips
- Use Release build configuration for optimal performance
- Enable GPU physics computation for better performance with many objects
- Consider reducing visual quality settings if running on older hardware

## 📄 License

This project is licensed under the MIT License - see the [LICENSE.txt](LICENSE.txt) file for details.

## 🤝 Contributing

Contributions are welcome! Please feel free to submit a Pull Request. For major changes, please open an issue first to discuss what you would like to change.

### Development Guidelines
1. Follow the existing code style and structure
2. Add comments for complex physics calculations
3. Test your changes with both CPU and GPU physics modes
4. Update documentation if you add new features

## 🙏 Acknowledgments

- Real planetary data from NASA
- OpenGL and compute shader implementation techniques
- GLFW for cross-platform window management
- GLM for mathematical operations

---

**Happy simulating! 🌍🪐✨**