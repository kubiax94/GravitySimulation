# Copilot Instructions

## Project Guidelines
- User treats GravitySimulation as a graphics/engine project rather than a gravity-simulation-first app. The physics system should integrate compute shaders as potential force providers acting on rigid bodies and evolve toward a true async engine-level pipeline rather than quick scene-specific or temporary fixes. `simulation_state` should remain a generic simulation/engine state and should not own scene-specific resources; scene-specific resources and logic should live per scene or dedicated engine/render systems instead.
- User wants engine-level generic solutions for planetary water/fluid and does not want scene-specific per-planet tuning hacks; improvements should stay generic and follow an explicit step-by-step plan.
- User prefers handling high-count simulated/rendered objects with a particle-system-like architecture treated as a single CPU transform.
- User prefers physically simulated planetary water/ocean behavior driven by the existing fluid solver over fake ocean surface shaders for planets.
- User wants close-up views of planetary water rendering to show a detailed continuous water surface/tafla rather than visible particle dots; particle appearance at near range is undesirable.
- User wants work to follow an explicit step-by-step plan so progress stays organized and does not drift while improving planetary water, debug views, and later engine-level LOD.
- After fixing planetary water/debug, implement a generic engine-level LOD system in `render_pipeline/renderer` instead of a scene-specific close-range water rendering path.
- User prefers a dedicated planetary water masking/occlusion module instead of overloading the current atlas and shell stages.
- Use existing resource classes for planetary/generated textures instead of ad-hoc raw GPU textures, because these resources should later be saved to disk, reloaded, and reused after crashes; procedurally generated planetary water/ocean textures should be centralized in one resource layer.
- User does not want ad-hoc texture abstractions that are unused by `gpu_compute_shader` or the rest of the engine; `simulation_state` should have narrower responsibilities, and `render_pipeline` should handle scene rendering concerns generically at the engine level rather than leaving scene-specific runtime rendering in `simulation_state`.
- Use the user's existing project abstractions, resource classes, and previously designed systems instead of inventing new ad-hoc implementations; do not rebuild what already exists in the codebase.
- User wants scene runtime data organized as a scene resource map/registry built on existing asset/resource abstractions, instead of many dedicated per-scene raw pointer fields or scene-specific factory abstractions.
- User prefers a separate physics stage/integration class with an explicit ordered physics pipeline, instead of mixing force accumulation and integration ad hoc.
- User prefers to finish the `render_pipeline` and current rendering-related fixes before returning to the physics refactor work.
- When an optimization does not materially improve the target metric, diagnose the remaining bottleneck in that path and apply a concrete fix instead of restating the plan.
- User prefers recurring render/diagnostic metrics to be integrated into the existing frame_profiler-style reporting and emitted at the profiler report interval instead of spamming stdout every frame. User also prefers these metrics to be extended for later on-screen display.
- User prefers scene-specific or component-specific debug/input handling; avoid pushing scene-specific wave debug controls into engine-level or global simulation_state input/title logic.

## Collision and Ray Casting
- User wants collision support to use a layer system.
- Implement ray casting as a separate class with hit tracking/results rather than ad-hoc logic.
- User prefers a separate `collision_data` buffer for colliders instead of overloading `rigid_body` `physics_data`, as it is architecturally cleaner and simpler than rebuilding `rigid_body` around collider state.
- User wants concrete instrumentation/logging instead of speculative debugging when investigating collision behavior.

## Spatial Structures
- User wants hierarchical bounding boxes where a parent node's bounds aggregate its own content and all child nodes, while child nodes keep their own local subtree bounds for debug/rendering use.