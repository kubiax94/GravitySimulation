# Contributing Guide

Thank you for your interest in contributing to the Gravity Simulation project! This guide will help you get started with development and contribution best practices.

## 🤝 How to Contribute

### Types of Contributions Welcome

- **Bug Reports**: Found a problem? Let us know!
- **Feature Requests**: Have ideas for improvements?
- **Code Contributions**: Fix bugs or implement new features
- **Documentation**: Improve or add documentation
- **Performance Improvements**: Optimize existing code
- **Examples**: Add new simulation scenarios

### Getting Started

1. **Fork the Repository**
   ```bash
   # Click "Fork" on GitHub, then clone your fork
   git clone https://github.com/YOUR_USERNAME/GravitySimulation.git
   cd GravitySimulation
   git remote add upstream https://github.com/kubiax94/GravitySimulation.git
   ```

2. **Set Up Development Environment**
   - Follow the [Installation Guide](INSTALL.md)
   - Ensure you can build and run the project
   - Set up Visual Studio with recommended extensions

3. **Create a Feature Branch**
   ```bash
   git checkout -b feature/your-feature-name
   # or
   git checkout -b bugfix/issue-description
   ```

## 📋 Development Guidelines

### Code Style

#### C++ Style Guidelines
- **Naming Conventions**:
  ```cpp
  // Classes: PascalCase
  class PhysicsSystem { };
  
  // Functions and variables: snake_case
  void calculate_gravity();
  float orbital_velocity;
  
  // Constants: UPPER_SNAKE_CASE
  const float GRAVITATIONAL_CONSTANT = 6.674e-11f;
  
  // Private members: trailing underscore
  private:
      float mass_;
      glm::vec3 position_;
  ```

- **File Organization**:
  - Header files (`.h`) contain declarations
  - Implementation files (`.cpp`) contain definitions
  - One class per header/implementation pair
  - Include guards or `#pragma once` in headers

#### Formatting
- **Indentation**: 4 spaces (no tabs)
- **Line Length**: Maximum 120 characters
- **Braces**: Opening brace on same line
  ```cpp
  if (condition) {
      // code here
  }
  ```

#### Comments
- Use clear, descriptive comments for complex physics calculations
- Document public API functions
- Explain "why" not "what" in comments
  ```cpp
  // Calculate orbital velocity using vis-viva equation
  // v = sqrt(G * M * (2/r - 1/a)) where a is semi-major axis
  float orbital_velocity = sqrt(gravitational_parameter * (2.0f / distance - 1.0f / semi_major_axis));
  ```

### Architecture Principles

1. **Component-Based Design**
   - Follow the existing component system
   - Each component should have a single responsibility
   - Use composition over inheritance

2. **Performance Considerations**
   - Prefer GPU computation for physics when possible
   - Minimize memory allocations in main loop
   - Use const-correctness and move semantics where appropriate

3. **Resource Management**
   - Follow RAII principles
   - Properly manage OpenGL resources
   - Use smart pointers for shared ownership

## 🧪 Testing Guidelines

### Manual Testing
Before submitting a pull request:

1. **Build Testing**:
   ```bash
   # Test both Debug and Release builds
   msbuild GravitySimulation.sln /p:Configuration=Debug /p:Platform=x64
   msbuild GravitySimulation.sln /p:Configuration=Release /p:Platform=x64
   ```

2. **Functional Testing**:
   - Run the simulation and verify basic functionality
   - Test camera movement and controls
   - Observe planetary motion for accuracy
   - Check performance (frame rate should be 60+ FPS on modern hardware)

3. **Edge Case Testing**:
   - Test with different numbers of celestial bodies
   - Verify stability with extreme masses or distances
   - Test window resizing and focus changes

### Performance Testing
- Monitor frame rate during testing
- Check GPU/CPU usage balance
- Test memory usage over extended periods

### Graphics Testing
- Test on different graphics cards if possible
- Verify shader compilation on different drivers
- Check for OpenGL errors using debug output

## 🚀 Pull Request Process

### Before Submitting

1. **Update Documentation**:
   - Update README.md if you added features
   - Add technical details to DOCS.md if relevant
   - Update installation guide if build process changed

2. **Code Quality Check**:
   - Ensure code follows style guidelines
   - Remove debugging code and comments
   - Check for memory leaks in debug builds

3. **Test Thoroughly**:
   - Test on your target platform
   - Verify existing functionality still works
   - Document any breaking changes

### Submitting a Pull Request

1. **Prepare Your Changes**:
   ```bash
   git add .
   git commit -m "Brief description of changes"
   git push origin feature/your-feature-name
   ```

2. **Create Pull Request**:
   - Go to GitHub and create a pull request
   - Use a clear, descriptive title
   - Fill out the pull request template
   - Reference any related issues

3. **Pull Request Template**:
   ```markdown
   ## Description
   Brief description of changes and motivation.
   
   ## Type of Change
   - [ ] Bug fix
   - [ ] New feature
   - [ ] Performance improvement
   - [ ] Documentation update
   - [ ] Other (please describe):
   
   ## Testing
   - [ ] Tested on Windows 10/11
   - [ ] Verified with both Debug and Release builds
   - [ ] No performance regression
   - [ ] Documentation updated if needed
   
   ## Screenshots (if applicable)
   Add screenshots or videos showing the changes.
   ```

### Review Process

1. **Automated Checks**: GitHub Actions will run basic checks
2. **Code Review**: Maintainers will review your code
3. **Testing**: Changes will be tested on different configurations
4. **Feedback**: Address any requested changes
5. **Merge**: Approved changes will be merged

## 🐛 Bug Reports

### Before Reporting
1. Check existing issues to avoid duplicates
2. Test with latest version from main branch
3. Gather system information

### Bug Report Template
```markdown
## Bug Description
Clear description of what the bug is.

## To Reproduce
Steps to reproduce the behavior:
1. Open application
2. Perform specific action
3. Observe error

## Expected Behavior
What you expected to happen.

## Screenshots/Videos
If applicable, add visual evidence.

## System Information
- OS: Windows 10/11
- Graphics Card: NVIDIA RTX 3070
- Graphics Driver Version: 
- Visual Studio Version: 2022
- Build Configuration: Debug/Release

## Additional Context
Any other context about the problem.
```

## 💡 Feature Requests

### Feature Request Template
```markdown
## Feature Description
Clear description of the proposed feature.

## Motivation
Why is this feature useful? What problem does it solve?

## Proposed Implementation
How do you envision this being implemented?

## Alternatives Considered
Other approaches you've considered.

## Additional Context
Any other relevant information.
```

## 🚧 Development Areas

### High Priority Areas
1. **Performance Optimization**:
   - Improve GPU physics calculations
   - Optimize rendering pipeline
   - Memory usage improvements

2. **User Experience**:
   - Better camera controls
   - UI for simulation parameters
   - Save/load simulation states

3. **Visual Enhancements**:
   - Improved lighting and shading
   - Particle effects (comet tails, etc.)
   - Orbital trail rendering

### Technical Improvements
1. **Code Quality**:
   - Unit tests for physics calculations
   - Better error handling
   - Memory leak detection

2. **Platform Support**:
   - Linux support (long-term goal)
   - macOS support (long-term goal)
   - Different graphics API support

3. **Documentation**:
   - API documentation generation
   - Tutorial videos
   - Example projects

## 🔧 Development Tools

### Recommended Tools
- **Visual Studio 2022**: Primary IDE
- **Git**: Version control
- **GPU-Z**: Graphics card information
- **RenderDoc**: Graphics debugging
- **Visual Studio Diagnostic Tools**: Memory profiling

### Optional Tools
- **Clang-Format**: Code formatting
- **Doxygen**: Documentation generation
- **CMake**: Alternative build system (future)
- **vcpkg**: Package management (future)

## 📚 Learning Resources

### Graphics Programming
- [LearnOpenGL](https://learnopengl.com/): Excellent OpenGL tutorial
- [OpenGL Reference Pages](https://www.khronos.org/registry/OpenGL-Refpages/): Official documentation
- [GPU Gems](https://developer.nvidia.com/gpugems/gpugems/contributors): Advanced techniques

### Physics Simulation
- [Real-Time Rendering](http://www.realtimerendering.com/): Graphics and physics
- [Game Physics Engine Development](https://www.amazon.com/dp/0123819768): Physics fundamentals
- [Numerical Recipes](http://numerical.recipes/): Mathematical algorithms

### C++ Development
- [Effective Modern C++](https://www.oreilly.com/library/view/effective-modern-c/9781491908419/): Modern C++ practices
- [C++ Core Guidelines](https://github.com/isocpp/CppCoreGuidelines): Best practices
- [CPP Reference](https://en.cppreference.com/): Language reference

## 🆘 Getting Help

### Communication Channels
- **GitHub Issues**: For bug reports and feature requests
- **GitHub Discussions**: For questions and general discussion
- **Code Comments**: For specific implementation questions

### Response Times
- Issues and PRs are typically reviewed within 1-2 weeks
- Complex features may require more discussion time
- Bug fixes are prioritized over new features

### Mentorship
New contributors are welcome! If you're new to:
- **C++ development**: Start with small documentation improvements
- **Graphics programming**: Begin with simple rendering enhancements
- **Physics simulation**: Focus on parameter tuning and testing

## 🏆 Recognition

### Contributors
All contributors are recognized in:
- GitHub contributor list
- CONTRIBUTORS.md file (future addition)
- Release notes for significant contributions

### Types of Recognition
- **Code Contributors**: Direct code contributions
- **Documentation Contributors**: Improve project documentation
- **Testing Contributors**: Comprehensive testing and bug reports
- **Community Contributors**: Help other users and contributors

## 📄 License

By contributing to this project, you agree that your contributions will be licensed under the same MIT License that covers the project. See [LICENSE.txt](LICENSE.txt) for details.

---

Thank you for contributing to the Gravity Simulation project! Your contributions help make this educational physics simulation better for everyone. 🌟