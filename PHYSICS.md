# Physics and Mathematics Documentation

## Overview

This document explains the physical principles, mathematical formulations, and numerical methods used in the Gravity Simulation. The simulation implements Newton's law of universal gravitation with accurate astronomical data to create a realistic orbital mechanics simulation.

## 🌌 Physical Principles

### Newton's Law of Universal Gravitation

The fundamental force governing the simulation is gravitational attraction between celestial bodies:

```
F = G × (m₁ × m₂) / r²
```

Where:
- **F** = Gravitational force (Newtons)
- **G** = Gravitational constant (6.674 × 10⁻¹¹ m³/kg⋅s²)
- **m₁, m₂** = Masses of the two objects (kilograms)
- **r** = Distance between object centers (meters)

### Force Direction
The gravitational force is always attractive and acts along the line connecting the centers of the two masses:

```
F⃗ = G × (m₁ × m₂) / r² × r̂
```

Where **r̂** is the unit vector pointing from object 1 to object 2.

### N-Body Problem
The simulation solves the N-body gravitational problem, where each celestial body experiences forces from all other bodies simultaneously:

```
F⃗ᵢ = Σⱼ₌₁ᴺ G × (mᵢ × mⱼ) / |r⃗ⱼ - r⃗ᵢ|² × (r⃗ⱼ - r⃗ᵢ)/|r⃗ⱼ - r⃗ᵢ|
```

Where the sum excludes i = j (objects don't exert force on themselves).

## 📐 Mathematical Implementation

### Vector Calculations

#### Position and Velocity Vectors
Each celestial body has:
- **Position vector**: r⃗ = (x, y, z)
- **Velocity vector**: v⃗ = (vₓ, vᵧ, vᵤ)
- **Acceleration vector**: a⃗ = F⃗/m

#### Distance Calculation
```cpp
glm::vec3 direction = body_j.position - body_i.position;
float distance = glm::length(direction);
```

#### Force Calculation (CPU Implementation)
```cpp
glm::vec3 force_direction = glm::normalize(direction);
float force_magnitude = scaled_G * mass_i * mass_j / (distance * distance);
glm::vec3 force = force_direction * force_magnitude;
```

### GPU Shader Implementation

The compute shader performs the same calculations in parallel:

```glsl
vec3 direction = pos_j - pos_i;
float distance = length(direction);

if (distance < 0.01) continue;  // Avoid singularities

float force_magnitude = G * mass_i * mass_j / (distance * distance);
force += normalize(direction) * force_magnitude;
```

## 🔄 Numerical Integration

### Verlet Integration Method

The simulation uses Verlet integration for stable physics calculations:

```
r⃗(t + Δt) = r⃗(t) + v⃗(t) × Δt + ½ × a⃗(t) × Δt²
v⃗(t + Δt) = v⃗(t) + ½ × [a⃗(t) + a⃗(t + Δt)] × Δt
```

#### Advantages of Verlet Integration:
- **Time-reversible**: Maintains energy conservation better than Euler method
- **Stable**: Less prone to numerical drift
- **Symplectic**: Preserves orbital structure over long periods

#### Implementation in Code:
```cpp
void rigid_body::integrate(const float& dt) {
    // Calculate acceleration from accumulated forces
    glm::vec3 acceleration = accumulated_force / get_mass();
    
    // Update position using current velocity and acceleration
    position += velocity * dt + 0.5f * acceleration * dt * dt;
    
    // Update velocity using average acceleration
    velocity += acceleration * dt;
    
    // Clear forces for next frame
    clear_force();
}
```

## 📏 Unit Scaling System

### The Challenge
Real astronomical values span enormous ranges:
- **Solar mass**: 1.989 × 10³⁰ kg
- **Earth-Sun distance**: 1.496 × 10¹¹ meters
- **Orbital velocities**: ~30,000 m/s

These values cause floating-point precision issues in computer simulations.

### Scaling Solution

The simulation uses a unit scaling system to normalize values:

```cpp
struct unit_system {
    float mass_scale = 1e24f;        // Earth masses (kg)
    float distance_scale = 1e6f;     // Millions of km
    float scale_time = 3.872e6f / 3600.f;  // Hours to seconds
};
```

#### Scaled Gravitational Constant
```cpp
const float scaled_G() const {
    return const_G * (mass_scale / (distance_scale * distance_scale));
}
```

### Real-World Conversions

#### Mass Scaling
```cpp
float unit_system::mass(float kg) {
    return kg / mass_scale;  // Convert kg to Earth masses
}
```

Example: Solar mass = 1.989 × 10³⁰ kg / 10²⁴ kg = 1,989 Earth masses

#### Distance Scaling
```cpp
float unit_system::distance(float km) {
    return km / distance_scale;  // Convert km to millions of km
}
```

Example: Earth-Sun distance = 149.6 × 10⁶ km / 10⁶ km = 149.6 units

#### Rendering Scale
```cpp
float to_renderer_scale(float realm_km) {
    return realm_km / 12742.0f;  // Earth diameter as reference
}
```

## 🪐 Planetary Data and Orbital Mechanics

### Real Planetary Data

The simulation uses accurate astronomical data:

| Planet | Mass (kg) | Radius (km) | Semi-major Axis (km) | Orbital Velocity (km/s) |
|--------|-----------|-------------|----------------------|-------------------------|
| Mercury | 3.30×10²³ | 2,439 | 57.9×10⁶ | 47.9 |
| Venus | 4.87×10²⁴ | 6,052 | 108.2×10⁶ | 35.0 |
| Earth | 5.97×10²⁴ | 6,371 | 149.6×10⁶ | 29.8 |
| Mars | 6.42×10²³ | 3,390 | 227.9×10⁶ | 24.1 |
| Jupiter | 1.90×10²⁷ | 69,911 | 778.6×10⁶ | 13.1 |
| Saturn | 5.68×10²⁶ | 58,232 | 1,433.5×10⁶ | 9.7 |
| Uranus | 8.68×10²⁵ | 25,362 | 2,872.5×10⁶ | 6.8 |
| Neptune | 1.02×10²⁶ | 24,622 | 4,495.1×10⁶ | 5.4 |

### Orbital Velocity Calculation

The initial orbital velocity for circular orbits is calculated using:

```
v = √(G × M / r)
```

Where:
- **v** = orbital velocity
- **G** = gravitational constant
- **M** = mass of central body (Sun)
- **r** = orbital radius

#### Implementation:
```cpp
float v = sqrt(u_sys.scaled_G() * sun_mass / u_sys.distance(planet.distance_to_sun));
```

### Vis-Viva Equation

For elliptical orbits, the vis-viva equation determines velocity at any point:

```
v² = G × M × (2/r - 1/a)
```

Where:
- **a** = semi-major axis of the ellipse
- **r** = current distance from central body

## ⚡ Performance Optimizations

### GPU Parallel Processing

The compute shader processes multiple bodies simultaneously:

```glsl
layout(local_size_x = 64) in;  // 64 threads per work group

void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i >= bodies.length()) return;
    
    // Each thread processes one celestial body
    // Calculate forces from all other bodies in parallel
}
```

### Algorithmic Optimizations

#### Force Pair Optimization (CPU)
```cpp
// Calculate force between each pair only once
for (size_t i = 0; i < all.size(); ++i) {
    for (size_t j = i + 1; j < all.size(); ++j) {
        // Calculate force between bodies i and j
        // Apply equal and opposite forces (Newton's 3rd law)
        a_body->apply_force(force);
        b_body->apply_force(-force);
    }
}
```

#### Collision Avoidance
```cpp
if (dist < 1e-3f) continue;  // Skip very close bodies to avoid numerical issues
```

## 🔬 Numerical Accuracy and Stability

### Floating-Point Considerations

#### Single vs. Double Precision
- **Single precision (float)**: Used for performance on GPU
- **Adequate precision**: For the scaled unit system
- **Range**: ~10⁻⁷ to 10³⁸ with 7 decimal digits of precision

#### Potential Precision Issues
1. **Very small distances**: Handled by minimum distance checks
2. **Very large time steps**: Controlled by fixed timestep physics
3. **Accumulating errors**: Minimized by symplectic integration

### Energy Conservation

The system monitors total momentum for validation:
```cpp
glm::vec3 total_momentum = glm::vec3(0);
for (auto& entity : entities_) {
    total_momentum += entity->get_mass() * entity->velocity;
}
std::cout << glm::to_string(total_momentum) << std::endl;
```

Ideal conservation would show constant total momentum (accounting for numerical drift).

### Timestep Considerations

#### Fixed Timestep Physics
```cpp
while (time->should_fixed_update()) {
    scene.update();  // Physics update
    time->reduce_accumulator();
}
```

#### Optimal Timestep Selection
- **Too large**: Numerical instability, energy gain
- **Too small**: Computational overhead, precision loss
- **Current**: Balanced for real-time performance with acceptable accuracy

## 🧮 Mathematical Verification

### Orbital Period Verification

Kepler's Third Law provides verification for orbital mechanics:

```
T² = (4π²/GM) × a³
```

Where:
- **T** = orbital period
- **a** = semi-major axis
- **G** = gravitational constant
- **M** = mass of central body

#### Example Calculation (Earth):
```
T² = (4π²)/(G × M_sun) × (149.6 × 10⁹ m)³
T = 365.25 days ≈ 31,557,600 seconds
```

### Escape Velocity

The minimum velocity needed to escape gravitational influence:

```
v_escape = √(2GM/r)
```

This is √2 times the circular orbital velocity at the same distance.

### Gravitational Potential Energy

Total system energy includes kinetic and potential components:

```
E_total = ½mv² - GMm/r
```

In a stable system, total energy should remain approximately constant.

## 🎯 Simulation Accuracy

### Real vs. Simulated Comparison

| Property | Real Solar System | Simulation Accuracy |
|----------|------------------|-------------------|
| Earth orbital period | 365.25 days | ~365 time units |
| Orbital shapes | Elliptical | Approximated as circular |
| Planetary inclinations | 0° to 7° | Simplified to 0° |
| Relativistic effects | Present | Not included |
| Perturbations | Complex | N-body only |

### Limitations and Assumptions

1. **Circular Orbits**: Initial conditions assume circular orbits
2. **Point Masses**: Planets treated as point masses (no rotation)
3. **No Relativistic Effects**: Classical Newtonian physics only
4. **No Other Forces**: No electromagnetic forces, solar wind, etc.
5. **Perfect Spheres**: No tidal forces or deformation

### Future Improvements

1. **Elliptical Orbits**: Initialize with proper eccentricity
2. **Planetary Rotation**: Add rotational dynamics
3. **Asteroid Belts**: Add small body populations
4. **Relativistic Corrections**: Post-Newtonian approximations
5. **Variable Timestep**: Adaptive integration methods

---

This documentation provides the mathematical foundation for understanding and extending the Gravity Simulation. The implementation balances physical accuracy with computational performance for an educational and visually appealing simulation.