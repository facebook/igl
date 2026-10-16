# Slang sessions

Two render sessions are written entirely in Slang. Their programs are listed in `programs.json`, and
`generated/` holds the headers produced by `tools/slang/generate_slang.py`.

## SlangSplatsSession (portable)

Fits 2048 anisotropic 2D splats to an image by gradient descent, on the GPU. The four panels show the
target, the fit, the error, and how the fit changes as every splat moves right. Every backend draws
the splats; backends with compute train them live (4 iterations per frame), and the others draw
splats trained ahead of time (`generated/SplatsBaked.h`, written by the session's `--bake-splats`
mode).

| Program | Stage | What it does |
|---|---|---|
| `SplatCommon` | module | Splat model, kernels and losses, the exact gradient of normalized blending |
| `SplatBin` | compute | Lists the splats reaching each 8×8 tile, counting overflows atomically |
| `SplatForward` | compute | Blends each pixel and differentiates the loss |
| `SplatBackward` | compute | Gathers each splat's gradient over its pixels with a fixed-order sum |
| `SplatAdam` | compute | Adam step |
| `SplatRender` | vertex, fragment | Draws instanced splats and their sensitivity, then the panels |

## SlangShowcaseSession (high-end: Metal and Vulkan)

A grid of physically based spheres, one chapter at a time:
1. Materials: metal-rough, clearcoat, sheen and anisotropic lobes, shaded with a pipeline per
   material, with one pipeline dispatching on a type id, or in half precision.
2. Inverse rendering: recovers every sphere's parameters from images rendered with hidden ones,
   back-propagating through the same materials, and shows how the fit responds to roughness.
3. Post effects: three chains of effects (neutral, film, noir) composed at compile time, each its
   own pipeline.

| Program | Stage | What it does |
|---|---|---|
| `ShowcaseMaterials` | module | Material models, generic over the float type and differentiable |
| `ShowcaseMaterialsPass` | vertex, fragment | Draws the spheres in every mode, and their linear radiance for chapter 3 |
| `ShowcaseInverse` | compute | Per-pixel gradients against the hidden parameters, then Adam per sphere |
| `ShowcasePost` | vertex, fragment | Post chains over the linear radiance of chapter 3 |

## Slang features used

| Feature | Where |
|---|---|
| Modules with access control (`module`, `import`, `public`) | `SplatCommon`, `ShowcaseMaterials` |
| Interfaces and generics, specialized at compile time | `ISplatKernel`, `ILoss`, `IMaterialModel` |
| Generics over the float type (`__BuiltinFloatingPointType`) | `ShowcaseMaterials` lobes in `float` and `half` |
| Static interface requirements | `IMaterialModel` |
| Dynamic dispatch (`createDynamicObject`, `[anyValueSize]`) | `IMaterial` in chapters 1 and 3 |
| Reverse-mode autodiff (`bwd_diff`, `DifferentialPair`) | `SplatForward`, `SplatBackward`, `ShowcaseInverse` |
| Forward-mode autodiff (`fwd_diff`) | `SplatRender`, `ShowcaseMaterialsPass` |
| `no_diff` and a custom `[BackwardDerivative]` | `SplatCommon` |
| `IDifferentiable` structs | `Splat`, `MaterialData` |
| `Atomic<T>` | `SplatBin` |
| `groupshared` with fixed-order reductions | `SplatBackward`, `ShowcaseInverse` |
| Variadic generics (`each`, `expand`) | `PostChain` in `ShowcasePost` |
| `[ForceUnroll]`, `nointerpolation` | `ShowcaseMaterials`, `ShowcaseMaterialsPass` |
| Several entry points per program | `SplatRender`, `ShowcaseMaterialsPass`, `ShowcaseInverse`, `ShowcasePost` |
