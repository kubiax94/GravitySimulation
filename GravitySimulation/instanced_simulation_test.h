#pragma once
#include "Scene.h"
#include "instanced_renderer.h"
#include "instance_buffer_manager.h"
#include "instanced_physics_system.h"

namespace simtest_instanced
{
    void init_instanced_gravity_test(scene* s_to_init, 
                                   InstancedRenderer** out_planet_renderer,
                                   InstanceBufferManager** out_instance_manager,
                                   InstancedPhysicsSystem** out_physics_system,
                                   renderer** out_sun_renderer);
}