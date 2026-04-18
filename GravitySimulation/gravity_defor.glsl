#version 460

struct PhysicsBody {
        vec4 position;
        vec4 velocity;
        vec4 accumulated_force;
    };

layout(std430, binding = 0) buffer PhysicsData {
    PhysicsBody bodies[];
};

void main() {
    
}