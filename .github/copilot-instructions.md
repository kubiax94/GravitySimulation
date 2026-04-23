# Copilot Instructions

## Project Guidelines
- User wants engine-level generic solutions for planetary water/fluid and does not want scene-specific per-planet tuning hacks; improvements should stay generic and follow an explicit step-by-step plan.
- User prefers handling high-count simulated/rendered objects with a particle-system-like architecture treated as a single CPU transform.
- User prefers physically simulated planetary water/ocean behavior driven by the existing fluid solver over fake ocean surface shaders for planets.
- User wants close-up views of planetary water rendering to show a detailed continuous water surface/tafla rather than visible particle dots; particle appearance at near range is undesirable.
- User wants work to follow an explicit step-by-step plan so progress stays organized and does not drift while improving planetary water, debug views, and later engine-level LOD.
- After fixing planetary water/debug, implement a generic engine-level LOD system in `render_pipeline/renderer` instead of a scene-specific close-range water rendering path.