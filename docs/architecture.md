# Architecture

## Main flow

Better Wobbly Windows is a KWin effect. KWin creates the effect through the plugin factory and calls it during the compositor paint cycle.

The important runtime sequence is:

1. A window starts moving or resizing.
2. The effect creates or updates the window state.
3. The physics simulation advances for the current frame.
4. The control points are converted into a deformed mesh.
5. KWin paints the window through that mesh.
6. Blur is rendered through Better Blur DX API V1 or the embedded fallback.
7. The simulation settles after interaction ends, then its state is removed.

## Main components

### Effect and lifecycle

`src/Effect/WobblyWindowsEffect.cpp` connects the effect to KWin. `WindowLifecycle.cpp` handles windows entering and leaving the wobble state. `WobblyWindowMesh.cpp` builds the final geometry used for painting.

The private effect state is stored in `src/Effect/WobblyWindowsEffect_p.h`. It keeps per-window simulation and rendering data out of the public KWin-facing class.

### Physics

`src/Physics/WobblyWindowPhysics.cpp` updates the spring model. The model uses a small grid of control points rather than simulating every output vertex. This keeps the animation inexpensive while still producing a curved surface.

The main settings are stiffness, drag, movement strength, velocity limits, acceleration limits, and the maximum number of physics steps per frame. Tessellation controls how densely the curved surface is converted into triangles.

### Configuration

`src/Configuration/` loads the selected preset and advanced values from KConfig. Default presets are kept separately so they can be tested without starting KWin.

### Painting

The mesh contains the deformed destination positions and the source coordinates required to sample the window or blur texture. The same deformation must be used for the window and its blur; otherwise the blur appears detached, clipped, or delayed while the window moves.
