# Blur

Better Wobbly Windows supports two blur paths. Both paths use the deformed window mesh so the blur follows the moving window.

## Better Blur DX API V1

The public contract is defined in:

```text
include/BetterWobblyWindows/BetterBlurDxApi.h
```

There is currently one API version: **V1**.

Before painting a wobbling window, Better Wobbly Windows publishes a request that contains a `MeshProvider`. Better Blur DX can call the provider to obtain the vertices required to composite its cached blur through the current deformation.

The request can also ask Better Blur DX to suppress its normal rectangular composite and require a fresh cache. The provider owns only the mesh description; Better Blur DX keeps ownership of its textures and blur cache.

### Provider results

- `Rendered` — Better Blur DX rendered the deformed blur. No fallback is needed.
- `Declined` — the provider could not render this request. The embedded fallback may be used.
- `DeclinedByPolicy` — Better Blur DX intentionally excluded the window through its own matching rules. The fallback must not override that decision.
- `None` — no valid provider result was received.

API data is validated before use. The version, structure size, provider address, and callback must be valid. Unknown result values are decoded as `None`.

## Embedded blur

The embedded renderer is used when Better Blur DX is unavailable or technically declines the request. It is not a second public API and does not have separate compatibility versions.

Its pipeline is implemented in `src/EmbeddedBlur/`:

1. prepare or resize render targets;
2. capture the required source area;
3. downsample the image;
4. upsample it to produce the blur;
5. apply color adjustments and optional noise;
6. composite the result through the wobbling mesh;
7. restore the OpenGL state modified by the renderer.

The shaders are stored in `resources/shaders/`. Compatibility and core variants are selected according to the detected KWin version.

## Handoff

The handoff and fallback frame settings reduce visible jumps when rendering changes between Better Blur DX and the embedded renderer. The transition must not create two full blur composites for the same frame.

## Common blur problems

### Blur is cut at an edge

Check the capture region, output scale, texture origin, and clipping calculations. The blur region must include the full deformed mesh, not only the original rectangular window geometry.

### Blur looks different while moving

Confirm that the blur and the window use the same current mesh and coordinate scale. A stale cache, mismatched source mapping, or different color settings can make the moving window look softer or darker.

### Blur disappears with matching rules

A `DeclinedByPolicy` result is intentional. Better Wobbly Windows must not replace it with the embedded fallback.
