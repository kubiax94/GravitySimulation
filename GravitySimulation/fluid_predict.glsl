#version 460
layout(local_size_x = 64) in;

uniform float dt;
uniform vec3 gravity;
uniform vec3 boundsMin;
uniform vec3 boundsMax;
uniform float restitution;
uniform float collisionDamping;
uniform float interactionRadius;
uniform float particleRadius;
uniform float separationStrength;
uniform float nearPressureStrength;
uniform float velocityDamping;
uniform float viscosityStrength;
uniform float restDensity;
uniform float cellSize;
uniform int gridSizeX;
uniform int gridSizeY;
uniform int gridSizeZ;
uniform int passMode;
uniform int simulationMode;
uniform vec3 planetaryCenter;
uniform float planetaryRadius;
uniform float planetaryShellThickness;
uniform float planetaryGravityStrength;

struct FluidParticle {
    vec4 position;
    vec4 velocity;
    vec4 predicted_position;
    vec4 delta_position;
    vec4 solver_data;
};

layout(std430, binding = 0) buffer FluidParticles {
    FluidParticle particles[];
};

layout(std430, binding = 1) buffer FluidGridCellHeads {
    int cellHeads[];
};

layout(std430, binding = 2) buffer FluidGridNextParticle {
    int nextParticle[];
};

const float eps = 0.0001;
const float epsSq = eps * eps;
const float constraintRelaxation = 0.05;

ivec3 compute_cell_coords(vec3 position) {
    vec3 relative = (position - boundsMin) / max(cellSize, eps);
    return clamp(ivec3(floor(relative)), ivec3(0), ivec3(gridSizeX - 1, gridSizeY - 1, gridSizeZ - 1));
}

int flatten_cell_index(ivec3 cell) {
    return cell.x + cell.y * gridSizeX + cell.z * gridSizeX * gridSizeY;
}

float density_kernel(float distance) {
    float q = 1.0 - clamp(distance / max(interactionRadius, eps), 0.0, 1.0);
    return q * q * q;
}

vec3 gradient_kernel(vec3 delta, float distance) {
    if (distance <= eps || distance >= interactionRadius)
        return vec3(0.0);

    float q = 1.0 - clamp(distance / max(interactionRadius, eps), 0.0, 1.0);
    return -(3.0 * q * q / max(interactionRadius, eps)) * (delta / distance);
}

float tensile_correction(float distance) {
    float q = 1.0 - clamp(distance / max(interactionRadius, eps), 0.0, 1.0);
    float q2 = q * q;
    return -nearPressureStrength * q2 * q2;
}

vec3 compute_external_acceleration(vec3 position) {
    if (simulationMode == 1) {
        vec3 toCenter = planetaryCenter - position;
        float distanceSq = dot(toCenter, toCenter);
        if (distanceSq > epsSq)
            return toCenter * inversesqrt(distanceSq) * planetaryGravityStrength;

        return vec3(0.0);
    }

    return gravity;
}

void constrain_to_planetary_shell(inout vec3 position) {
    vec3 offset = position - planetaryCenter;
    float distance = length(offset);
    vec3 normal = distance > eps ? offset / distance : vec3(0.0, 1.0, 0.0);
    float minRadius = max(planetaryRadius + particleRadius * 0.35, eps);
    float maxRadius = max(minRadius, planetaryRadius + max(planetaryShellThickness - particleRadius * 0.35, 0.0));
    position = planetaryCenter + normal * clamp(distance, minRadius, maxRadius);
}

void compute_lambda(uint selfIndex) {
    vec3 position = particles[selfIndex].predicted_position.xyz;
    float density = 1.0;
    float restDensitySafe = max(restDensity, eps);
    float radius = max(interactionRadius, eps);
    float radiusSq = radius * radius;
    float radiusInv = 1.0 / radius;
    float sumGradSq = 0.0;
    vec3 gradI = vec3(0.0);
    ivec3 baseCell = compute_cell_coords(position);
    ivec3 minCell = max(baseCell - ivec3(1), ivec3(0));
    ivec3 maxCell = min(baseCell + ivec3(1), ivec3(gridSizeX - 1, gridSizeY - 1, gridSizeZ - 1));
    int particleCount = particles.length();

    for (int z = minCell.z; z <= maxCell.z; ++z) {
        for (int y = minCell.y; y <= maxCell.y; ++y) {
            for (int x = minCell.x; x <= maxCell.x; ++x) {
                int neighborIndex = cellHeads[flatten_cell_index(ivec3(x, y, z))];
                int guard = 0;

                while (neighborIndex >= 0 && guard < particleCount) {
                    if (neighborIndex != int(selfIndex)) {
                        vec3 delta = position - particles[neighborIndex].predicted_position.xyz;
                        float distanceSq = dot(delta, delta);
                        if (distanceSq < radiusSq) {
                            float distance = sqrt(distanceSq);
                            float q = 1.0 - distance * radiusInv;
                            float q2 = q * q;
                            density += q2 * q;

                            if (distanceSq > epsSq) {
                                vec3 grad = -(3.0 * q2 * radiusInv / restDensitySafe) * (delta * inversesqrt(distanceSq));
                                sumGradSq += dot(grad, grad);
                                gradI += grad;
                            }
                        }
                    }

                    neighborIndex = nextParticle[neighborIndex];
                    ++guard;
                }
            }
        }
    }

    sumGradSq += dot(gradI, gradI);
    float constraint = density / restDensitySafe - 1.0;
    float lambda = -constraint / (sumGradSq + constraintRelaxation);
    particles[selfIndex].solver_data = vec4(lambda, density, constraint, 0.0);
}

void compute_delta_position(uint selfIndex) {
    vec3 position = particles[selfIndex].predicted_position.xyz;
    float lambdaI = particles[selfIndex].solver_data.x;
    float restDensitySafe = max(restDensity, eps);
    float radius = max(interactionRadius, eps);
    float radiusSq = radius * radius;
    float radiusInv = 1.0 / radius;
    float minDistance = particleRadius * 2.0;
    vec3 deltaPosition = vec3(0.0);
    ivec3 baseCell = compute_cell_coords(position);
    ivec3 minCell = max(baseCell - ivec3(1), ivec3(0));
    ivec3 maxCell = min(baseCell + ivec3(1), ivec3(gridSizeX - 1, gridSizeY - 1, gridSizeZ - 1));
    int particleCount = particles.length();

    for (int z = minCell.z; z <= maxCell.z; ++z) {
        for (int y = minCell.y; y <= maxCell.y; ++y) {
            for (int x = minCell.x; x <= maxCell.x; ++x) {
                int neighborIndex = cellHeads[flatten_cell_index(ivec3(x, y, z))];
                int guard = 0;

                while (neighborIndex >= 0 && guard < particleCount) {
                    if (neighborIndex != int(selfIndex)) {
                        vec3 delta = position - particles[neighborIndex].predicted_position.xyz;
                        float distanceSq = dot(delta, delta);
                        if (distanceSq > epsSq && distanceSq < radiusSq) {
                            float distance = sqrt(distanceSq);
                            float q = 1.0 - distance * radiusInv;
                            float q2 = q * q;
                            vec3 grad = -(3.0 * q2 * radiusInv) * (delta * inversesqrt(distanceSq));
                            float lambdaJ = particles[neighborIndex].solver_data.x;
                            float scorr = -nearPressureStrength * q2 * q2;
                            deltaPosition += (lambdaI + lambdaJ + scorr) * grad;
                            if (distance < minDistance)
                                deltaPosition += (delta / distance) * (minDistance - distance);
                        }
                    }

                    neighborIndex = nextParticle[neighborIndex];
                    ++guard;
                }
            }
        }
    }

    deltaPosition *= separationStrength / restDensitySafe;
    float maxCorrection = particleRadius * 0.75;
    float correctionLength = length(deltaPosition);
    if (correctionLength > maxCorrection)
        deltaPosition *= maxCorrection / correctionLength;

    particles[selfIndex].delta_position = vec4(deltaPosition, 0.0);
}

void solve_boundaries(uint selfIndex) {
    vec3 position = particles[selfIndex].predicted_position.xyz + particles[selfIndex].delta_position.xyz;

    if (simulationMode == 1) {
        constrain_to_planetary_shell(position);
        particles[selfIndex].predicted_position = vec4(position, particles[selfIndex].predicted_position.w);
        particles[selfIndex].delta_position = vec4(0.0);
        return;
    }

    vec3 wallMin = boundsMin + vec3(particleRadius);
    vec3 wallMax = boundsMax - vec3(particleRadius);

    if (position.x < wallMin.x)
        position.x = wallMin.x;
    else if (position.x > wallMax.x)
        position.x = wallMax.x;

    if (position.y < wallMin.y)
        position.y = wallMin.y;
    else if (position.y > wallMax.y)
        position.y = wallMax.y;

    if (position.z < wallMin.z)
        position.z = wallMin.z;
    else if (position.z > wallMax.z)
        position.z = wallMax.z;

    particles[selfIndex].predicted_position = vec4(position, particles[selfIndex].predicted_position.w);
    particles[selfIndex].delta_position = vec4(0.0);
}

vec3 compute_viscosity(uint selfIndex, vec3 velocity) {
    vec3 position = particles[selfIndex].predicted_position.xyz;
    vec3 viscosity = vec3(0.0);
    float radius = max(interactionRadius, eps);
    float radiusSq = radius * radius;
    float radiusInv = 1.0 / radius;
    float weight = 0.0;
    ivec3 baseCell = compute_cell_coords(position);
    ivec3 minCell = max(baseCell - ivec3(1), ivec3(0));
    ivec3 maxCell = min(baseCell + ivec3(1), ivec3(gridSizeX - 1, gridSizeY - 1, gridSizeZ - 1));
    int particleCount = particles.length();

    for (int z = minCell.z; z <= maxCell.z; ++z) {
        for (int y = minCell.y; y <= maxCell.y; ++y) {
            for (int x = minCell.x; x <= maxCell.x; ++x) {
                int neighborIndex = cellHeads[flatten_cell_index(ivec3(x, y, z))];
                int guard = 0;

                while (neighborIndex >= 0 && guard < particleCount) {
                    if (neighborIndex != int(selfIndex)) {
                        vec3 delta = position - particles[neighborIndex].predicted_position.xyz;
                        float distanceSq = dot(delta, delta);
                        if (distanceSq > epsSq && distanceSq < radiusSq) {
                            float distance = sqrt(distanceSq);
                            float q = 1.0 - distance * radiusInv;
                            float q2 = q * q;
                            vec3 neighborVelocity = (particles[neighborIndex].predicted_position.xyz - particles[neighborIndex].position.xyz) / max(dt, eps);
                            float w = q2 * q;
                            viscosity += (neighborVelocity - velocity) * w;
                            weight += w;
                        }
                    }

                    neighborIndex = nextParticle[neighborIndex];
                    ++guard;
                }
            }
        }
    }

    if (weight > 0.0)
        viscosity /= weight;

    return viscosity;
}

void finalize_particle(uint selfIndex) {
    vec3 previousPosition = particles[selfIndex].position.xyz;
    vec3 position = particles[selfIndex].predicted_position.xyz;
    vec3 velocity = (position - previousPosition) / max(dt, eps);
    velocity += compute_viscosity(selfIndex, velocity) * clamp(viscosityStrength * dt, 0.0, 1.0);
    velocity *= max(0.0, 1.0 - velocityDamping * dt);

    if (simulationMode == 1) {
        constrain_to_planetary_shell(position);
        vec3 radial = position - planetaryCenter;
        float radialSq = dot(radial, radial);
        if (radialSq > epsSq) {
            vec3 normal = radial * inversesqrt(radialSq);
            velocity -= normal * dot(velocity, normal);
        }

        velocity *= collisionDamping;
        particles[selfIndex].position = vec4(position, particles[selfIndex].position.w);
        particles[selfIndex].predicted_position = vec4(position, particles[selfIndex].predicted_position.w);
        particles[selfIndex].velocity = vec4(velocity, particles[selfIndex].velocity.w);
        particles[selfIndex].solver_data = vec4(0.0);
        particles[selfIndex].delta_position = vec4(0.0);
        return;
    }

    vec3 wallMin = boundsMin + vec3(particleRadius);
    vec3 wallMax = boundsMax - vec3(particleRadius);

    if (position.x <= wallMin.x + eps || position.x >= wallMax.x - eps)
        velocity.x *= -restitution;
    if (position.y <= wallMin.y + eps || position.y >= wallMax.y - eps)
        velocity.y *= -restitution;
    if (position.z <= wallMin.z + eps || position.z >= wallMax.z - eps)
        velocity.z *= -restitution;

    velocity *= collisionDamping;

    particles[selfIndex].position = vec4(position, particles[selfIndex].position.w);
    particles[selfIndex].predicted_position = vec4(position, particles[selfIndex].predicted_position.w);
    particles[selfIndex].velocity = vec4(velocity, particles[selfIndex].velocity.w);
    particles[selfIndex].solver_data = vec4(0.0);
    particles[selfIndex].delta_position = vec4(0.0);
}

void main() {
    uint i = gl_GlobalInvocationID.x;

    if (passMode == 1) {
        if (i < uint(cellHeads.length()))
            cellHeads[i] = -1;
        return;
    }

    if (i >= uint(particles.length()))
        return;

    if (passMode == 0) {
        vec3 velocity = particles[i].velocity.xyz + compute_external_acceleration(particles[i].position.xyz) * dt;
        vec3 predicted = particles[i].position.xyz + velocity * dt;
        particles[i].predicted_position = vec4(predicted, particles[i].predicted_position.w);
        particles[i].delta_position = vec4(0.0);
        particles[i].solver_data = vec4(0.0);
        return;
    }

    if (passMode == 2) {
        int cellIndex = flatten_cell_index(compute_cell_coords(particles[i].predicted_position.xyz));
        nextParticle[i] = atomicExchange(cellHeads[cellIndex], int(i));
        return;
    }

    if (passMode == 3) {
        compute_lambda(i);
        return;
    }

    if (passMode == 4) {
        compute_delta_position(i);
        return;
    }

    if (passMode == 5) {
        solve_boundaries(i);
        return;
    }

    if (passMode == 6)
        finalize_particle(i);
}
