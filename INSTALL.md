# Installation and Build Guide

## System Requirements

### Minimum Requirements
- **Operating System**: Windows 10 (64-bit) or later
- **Processor**: Intel Core i5 or AMD Ryzen 5 (with SSE4.1 support)
- **Memory**: 4 GB RAM
- **Graphics**: DirectX 11 compatible GPU with OpenGL 4.6 support
- **Storage**: 500 MB available space
- **Development Environment**: Visual Studio 2019 or later

### Recommended Requirements
- **Operating System**: Windows 11 (64-bit)
- **Processor**: Intel Core i7 or AMD Ryzen 7
- **Memory**: 8 GB RAM or more
- **Graphics**: Dedicated GPU with compute shader support (NVIDIA GTX 1060 / AMD RX 580 or better)
- **Storage**: 1 GB available space
- **Development Environment**: Visual Studio 2022 with latest updates

### Graphics Card Compatibility
The simulation requires **OpenGL 4.6** with **compute shader support**. Compatible graphics cards include:

#### NVIDIA
- GeForce GTX 400 series and newer (Fermi architecture)
- All GeForce RTX series
- Quadro and Tesla professional cards

#### AMD
- Radeon HD 5000 series and newer (with updated drivers)
- All Radeon RX series
- FirePro and Radeon Pro professional cards

#### Intel
- Intel HD Graphics 4000 and newer (with limitations)
- Intel Iris and Iris Pro graphics
- Intel Arc graphics cards

## Installation Steps

### 1. Install Prerequisites

#### Visual Studio Community 2022 (Free)
1. Download from [Visual Studio official site](https://visualstudio.microsoft.com/vs/community/)
2. During installation, select:
   - **Desktop development with C++** workload
   - **Windows 10/11 SDK** (latest version)
   - **CMake tools for C++** (optional, for future enhancements)

#### Git for Windows
1. Download from [git-scm.com](https://git-scm.com/download/win)
2. Install with default settings
3. Choose "Git from the command line and also from 3rd-party software"

#### Graphics Drivers
Ensure you have the latest graphics drivers installed:
- **NVIDIA**: Download from [NVIDIA Driver Downloads](https://www.nvidia.com/drivers/)
- **AMD**: Download from [AMD Support](https://www.amd.com/support)
- **Intel**: Download from [Intel Graphics Driver Downloads](https://www.intel.com/content/www/us/en/support/graphics.html)

### 2. Clone the Repository

```bash
# Open Command Prompt or PowerShell
git clone https://github.com/kubiax94/GravitySimulation.git
cd GravitySimulation
```

### 3. Project Structure Verification

After cloning, your directory structure should look like:
```
GravitySimulation/
├── GravitySimulation/               # Source code directory
│   ├── GravitySimulation.cpp       # Main application file
│   ├── *.h, *.cpp                  # Source files
│   ├── *.shader, *.glsl           # Shader files
│   ├── glfw-3.4/                  # GLFW library (included)
│   └── GravitySimulation.vcxproj   # Visual Studio project file
├── GravitySimulation.sln           # Visual Studio solution file
├── README.md                       # Main documentation
├── DOCS.md                         # Technical documentation
├── INSTALL.md                      # This file
└── LICENSE.txt                     # License information
```

## Building the Project

### Option 1: Using Visual Studio IDE (Recommended)

1. **Open the Solution**:
   - Double-click `GravitySimulation.sln`
   - Or open Visual Studio and use File → Open → Project/Solution

2. **Configure Build Settings**:
   - Set Solution Configuration to **Release** (for best performance)
   - Set Solution Platform to **x64**
   - Both dropdowns are in the toolbar below the menu bar

3. **Build the Project**:
   - Press `Ctrl+Shift+B` or use Build → Build Solution
   - Wait for compilation to complete (should take 1-2 minutes)

4. **Run the Application**:
   - Press `F5` to run with debugging
   - Or press `Ctrl+F5` to run without debugging
   - The simulation window should open

### Option 2: Using MSBuild Command Line

```cmd
# Open Developer Command Prompt for VS 2022
cd GravitySimulation

# Build Release configuration
msbuild GravitySimulation.sln /p:Configuration=Release /p:Platform=x64

# Run the executable
cd x64\Release
GravitySimulation.exe
```

### Option 3: Using Visual Studio Code (Advanced)

1. **Install Extensions**:
   - C/C++ Extension Pack
   - CMake Tools (if using CMake)

2. **Open Folder**:
   - Open the `GravitySimulation` folder in VS Code

3. **Configure C++ Extension**:
   - Press `Ctrl+Shift+P` and select "C/C++: Edit Configurations (UI)"
   - Set Compiler path to your Visual Studio installation
   - Set IntelliSense mode to "windows-msvc-x64"

4. **Build**:
   - Use Terminal → New Terminal
   - Run MSBuild commands as shown in Option 2

## Troubleshooting

### Common Build Errors

#### Error: "Cannot open include file 'GL/glew.h'"
**Solution**: The project uses GLAD instead of GLEW. Check that:
- `glad.c` and `glad.h` are included in the project
- Include directories are properly set in project properties

#### Error: "LNK2019: unresolved external symbol"
**Solution**: 
- Ensure all `.cpp` files are included in the project
- Check that OpenGL libraries are linked: `opengl32.lib`
- Verify that GLFW is properly linked

#### Error: "This application has failed to start because no Qt platform plugin could be loaded"
**Solution**: This is not a Qt application. If you see this error:
- Clean and rebuild the solution
- Check that you're running the correct executable
- Verify Windows SDK is properly installed

### Runtime Errors

#### Black Screen or Window Not Opening
**Possible Causes & Solutions**:

1. **Graphics Driver Issues**:
   ```
   - Update graphics drivers to latest version
   - Check Windows Update for driver updates
   - Restart computer after driver installation
   ```

2. **OpenGL Version Issues**:
   ```
   - Use GPU-Z or similar tool to check OpenGL support
   - Ensure OpenGL 4.6 is supported
   - Try running on integrated graphics if available
   ```

3. **Shader Compilation Errors**:
   ```
   - Check that shader files (.shader, .glsl) are in the correct directory
   - Verify shader paths in source code match actual file locations
   - Enable OpenGL debug context for detailed error messages
   ```

#### Performance Issues

1. **Low Frame Rate**:
   ```
   - Switch to Release build configuration
   - Update graphics drivers
   - Close unnecessary background applications
   - Try reducing number of celestial bodies in galactic_simulation_test.cpp
   ```

2. **High CPU Usage**:
   ```
   - Ensure GPU physics is enabled (check physics_system implementation)
   - Monitor GPU usage with Task Manager or GPU-Z
   - Consider reducing physics update frequency
   ```

### System-Specific Issues

#### Windows 11 with Secure Boot
- Some older graphics drivers may have compatibility issues
- Try installing drivers in compatibility mode
- Disable Windows Defender Real-time Protection temporarily during build

#### High DPI Displays
- Windows may scale the application incorrectly
- Right-click executable → Properties → Compatibility
- Check "Override high DPI scaling behavior"
- Set scaling to "Application"

#### Integrated Graphics (Intel/AMD APU)
- Performance will be limited compared to dedicated GPUs
- Ensure GPU physics is working correctly
- May need to reduce simulation complexity
- Update integrated graphics drivers

## Performance Optimization

### Build Optimizations

1. **Release Configuration**:
   ```
   - Always use Release build for final testing
   - Enables compiler optimizations
   - Removes debug symbols and assertions
   ```

2. **Link-Time Optimization**:
   ```
   - In Project Properties → C/C++ → Optimization
   - Set "Whole Program Optimization" to "Yes"
   - In Linker → Optimization, enable "Link Time Code Generation"
   ```

3. **CPU-Specific Optimizations**:
   ```
   - In C/C++ → Code Generation
   - Set "Enable Enhanced Instruction Set" to "Advanced Vector Extensions 2"
   - Only if your CPU supports AVX2
   ```

### Runtime Optimizations

1. **Graphics Settings**:
   ```cpp
   // In main(), consider reducing MSAA samples
   glfwWindowHint(GLFW_SAMPLES, 4);  // Instead of 8
   
   // Enable V-Sync for smoother motion
   glfwSwapInterval(1);
   ```

2. **Physics Calculations**:
   ```cpp
   // In galactic_simulation_test.cpp, reduce planet count for testing
   // Comment out some planets if needed for performance testing
   ```

3. **Window Size**:
   ```cpp
   // Smaller window = better performance
   GLFWwindow* window = glfwCreateWindow(1024, 576, "Test", NULL, NULL);
   ```

## Advanced Configuration

### Custom Shader Paths

If you need to change shader file locations, update paths in:
- `GravitySimulation.cpp` (lines 89-91)
- `galactic_simulation_test.cpp` (lines 41-42)

### Physics Parameters

Modify simulation parameters in:
- `unit_system.cpp`: Scaling factors
- `galactic_simulation_test.cpp`: Planetary data and initial conditions
- `physics_system.cpp`: Integration timestep and method

### Memory Optimization

For systems with limited RAM:
1. Reduce texture sizes in future implementations
2. Limit number of celestial bodies
3. Use lower precision physics calculations if needed

## Development Environment Setup

### Code Style and Formatting

1. **Install ClangFormat** (optional):
   - Available through Visual Studio installer
   - Helps maintain consistent code formatting

2. **Enable Static Analysis**:
   - In Project Properties → Code Analysis
   - Enable "Microsoft Native Analysis Tools"
   - Helps catch potential bugs

### Debugging Configuration

1. **Enable Graphics Debugging**:
   ```cpp
   // Add to main() after OpenGL context creation
   glEnable(GL_DEBUG_OUTPUT);
   glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
   ```

2. **Memory Leak Detection**:
   ```cpp
   // Add to main() for debug builds
   #ifdef _DEBUG
   _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
   #endif
   ```

## Next Steps

After successful installation and build:

1. **Explore the Simulation**:
   - Run the application and use WASD + mouse to navigate
   - Observe planetary motion and orbital mechanics

2. **Read the Documentation**:
   - Check `DOCS.md` for technical details
   - Understand the physics implementation
   - Learn about the component system

3. **Experiment**:
   - Modify planetary parameters in `galactic_simulation_test.cpp`
   - Try adding new celestial bodies
   - Experiment with different camera positions

4. **Contribute**:
   - Report bugs or issues on GitHub
   - Suggest improvements or new features
   - Submit pull requests with enhancements

For additional help, please check the project's GitHub Issues page or create a new issue with your specific problem and system configuration.