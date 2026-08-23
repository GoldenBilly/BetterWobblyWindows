# Configuration

The configuration schema is stored in `wobblywindows.kcfg`. Most users only need a preset and the basic move or resize switches.

## Basic settings

- **Wobblyness level** selects a predefined parameter set.
- **Move wobble** enables deformation while moving windows.
- **Resize wobble** enables deformation while resizing windows.
- **Visual opposite point follow** changes how the opposite side reacts during resize.
- **Advanced mode** exposes the raw physics and blur values.

## Physics settings

- **Stiffness** controls how strongly the mesh returns to its resting shape.
- **Drag** removes energy from the simulation. Higher values settle faster.
- **Move factor** controls how strongly pointer movement affects the window.
- **X/Y tessellation** controls mesh density. Higher values improve curvature but create more vertices.
- **Velocity and acceleration limits** prevent unstable simulation spikes.
- **Stop velocity and stop acceleration** decide when a window is considered settled.
- **Maximum physics steps per frame** limits catch-up work after a slow frame.

## Blur settings

- **Embedded DX blur** enables the built-in fallback renderer.
- **Better Wobbly API only** keeps the Better Blur DX API hand-off enabled but disables the embedded fallback renderer.
- **Use Better Blur DX settings** reuses compatible blur appearance values when available.
- **Strength** controls blur intensity.
- **Saturation, contrast, and brightness** control the final color transform.
- **Noise strength and mode** control the optional noise pass.
- **Corner radius** changes edge masking.
- **Handoff frames** smooth the transition to provider rendering.
- **Fallback frames** smooth the transition back to embedded rendering.
- **Edge softness** softens the composite boundary.
- **Suppress external blur** prevents an additional normal blur composite when the deformed blur is active.

## Safe tuning order

When creating a custom preset, change values in this order:

1. stiffness and drag;
2. move factor;
3. stop thresholds;
4. tessellation;
5. blur appearance and transition values.

Change one group at a time. Very low drag, very high movement strength, and loose velocity limits can keep the simulation active for too long or make it unstable.
