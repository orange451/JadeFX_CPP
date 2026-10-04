# Box Batching Spec

2026-10-04 · Draw runs of boxes (rectangles, rounded rectangles, borders, shadows) in one GL call instead of one call each.

## Why

Every `fillRect`, `fillRounded`, `strokeRounded`, `outerShadow` and `innerShadow` goes through `UiRenderer::drawBox`, which binds the box program and VAO, sets its uniforms, and issues `glDrawArrays` for 6 vertices. After `a0ba215` (skip unchanged uniforms), a box still costs about 1.3 µs of CPU, nearly all of it the per-call GL overhead. Anarchy's profiler overlay draws about 1,100 boxes a frame and spends 1.45 ms doing it; every studio pane pays the same per-box price for its backgrounds, borders, selection bars, and grid cells.

Measured on Anarchy's profiler-demo (CannonVsPigs, M1 Pro), UI thread, average per frame:

| | before `a0ba215` | after `a0ba215` | goal |
| --- | --- | --- | --- |
| Profiler overlay | 2.29 ms | 1.45 ms | under 0.4 ms |
| Scene View paint (3D + overlay) | 4.06 ms | 3.13 ms | under 2.1 ms |

## Goal

A run of consecutive boxes is drawn with one instanced draw call. Nothing on screen changes: the same pixels, byte for byte, in the same order. Code that draws raw GL inside a paint keeps working.

## Decisions

| Question | Decision |
| --- | --- |
| Mechanism | Instancing. One shared unit quad; each box is one instance whose parameters are per-instance vertex attributes (`glVertexAttribDivisor` 1) in a streamed buffer; one `glDrawArraysInstanced` per run. Core in GL 3.3 (the desktop context JadeFX asks for) and GLES 3.0, which the mobile host asks for (`GlfmHost.cpp`, `GLFMRenderingAPIOpenGLES3`, shaders `#version 300 es`). No fallback is needed. The mobile build has never been built or run, so it is not tested here. |
| What batches | Every box with one color stop: solid fills, rounded fills, borders, and shadows. That is nearly every box a UI draws. |
| What does not | A gradient (2 to 8 stops) keeps today's uniform path: the run so far is flushed, the gradient drawn alone. Its stops are too many to carry per instance cheaply, and gradients are rare. |
| Per-instance data | Today's uniforms, minus the stops: `rect`, `box`, `radii`, `params`, `border`, `clip`, `clipRadii` (seven vec4) and one color (vec4). 128 bytes a box, 8 instanced attributes plus the quad's corner: 9 of the 16 every GL 3.3 and GLES 3.0 context guarantees. `uViewport` stays a uniform. |
| Shader | `box.vert`/`box.frag` take the per-box values as attributes passed through as `flat` varyings, so the fragment math is unchanged. A `uGradient` uniform chooses where the color comes from: 0, the instance's color; 1, today's `uStops`/`uStopAt`/`uStopCount` uniforms. |
| Drawing a gradient | Flush the run, then draw the gradient box as a run of one instance with `uGradient` 1 and its stops set as uniforms (through `UniformCache`), then set `uGradient` back to 0 for the next run. One VAO and one draw path for every box. |
| When a run is drawn (flushed) | Before any other draw: text, an image, or a gradient box; before `pushClip`/`popClip`, since the scissor is GL state; at `end()`; and on `flush()`. Draw order is the call order, so overlap and blending are unchanged. |
| Raw GL inside a paint | A node that issues its own GL calls opts in with `Node::setDrawsRawGl(true)`, and `Node::render` then flushes before its `renderContent`. `UiRenderer::flush()` is public, and `Painter::flush()` reaches it, for a node that draws through a Painter and then raw GL within one `renderContent`. Anarchy's GameView and MaterialBall opt in (a change in the Anarchy repo), and the Rainbow-Triangle demo would too. A node that forgets gets its GL drawn under boxes queued before it, so the opt-in is documented on `renderContent`. As today, raw GL must leave the viewport, scissor, and blend enable as it found them; every flush binds the box program, its VAO and instance buffer, and the blend function itself. |
| Buffer | One instance buffer of 4,096 boxes (512 KB), grown by doubling when a frame needs more. `begin()` orphans it (`glBufferData` with null); each flush writes its run after the last one with `glBufferSubData` and draws from that offset, so no run overwrites data the GPU may still read. A run that would pass the end orphans again and starts at 0. A run is at most 4,096 boxes; a longer one is flushed and continues as a new run. |
| Uniform cache | `UniformCache` stays, now for the gradient path and the per-run uniforms. |

## Units

- **`BoxBatch`** (`src/gl/BoxBatch.hpp/.cpp`, no GL): builds instance records from `drawBox`'s arguments, applying the scale and padding exactly as `drawBox` does today, and holds the pending run. `add`, `size`, `data`, `clear`. Unit-tested without a context.
- **`UiRenderer`**: owns a `BoxBatch`, the instance VAO and buffer; `drawBox` with one stop appends; `flushBoxes()` uploads and draws the run; every other draw and state change calls it first.
- **`Node`**: `setDrawsRawGl` / `drawsRawGl`, and the flush in `render`.
- **`Painter`**: `flush()`.

## Testing

- **`BoxBatch` unit tests** (jadefx-tests, no GL): records match what `drawBox` would have set as uniforms for fills, rounded fills, borders, shadows, clipped boxes, and fractional scales; the run splits at the size limit; `clear` empties it.
- **Pixel equality** (by hand, needs GL): Anarchy's `assets-demo` (its 16 deterministic screenshots, light and dark) and a new JadeFX program, `render-check`, that lays out fixed scenes in a hidden window and writes each to a PPM through `UiRenderer::writePpm`: solid, rounded, bordered, shadowed, and gradient boxes; boxes between text and images; nested clips; more than 4,096 boxes in one run. Byte-compared against the same program built on `master` before the change.
- **Raw GL ordering**: a render-check scene with a node that draws a raw GL quad over boxes drawn before it, with the opt-in (the quad is on top) and without (it is under them, the documented failure), checking the pixels.
- **Performance**: Anarchy's profiler-demo before and after; the table above is the bar.
- **Platforms**: macOS GL 4.1 only. The GLES path shares the shaders (`#version 300 es`), but the mobile builds are neither built nor run.

## Out of scope

Batching text (one draw per string today) and images; doing the scissor in the shader so clips no longer split runs; caching static UI between frames. Each is a later step if the numbers still call for it.

## Risks

- **Pixel drift**: attribute and uniform floats are the same 32-bit values, and the fragment code is unchanged, so none is expected; the byte comparison catches any.
- **A raw-GL node that does not opt in** draws under queued boxes. Mitigation: the opt-in, the note on `renderContent`, and the ordering check.
- **Driver streaming stalls**: buffer orphaning per frame; measured on the profiler-demo.

## Implementation outline (for the plan, after this design is approved)

Two repos: steps 1–3 and 5 are JadeFX; the opt-in in step 4 is Anarchy.

1. `BoxBatch` and its unit tests.
2. The shader with per-instance attributes and the gradient flag; `UiRenderer` streams runs; flush points. Pixel equality on assets-demo and render-check.
3. `Node::setDrawsRawGl`, `Painter::flush`, the ordering check.
4. Anarchy's GameView and MaterialBall opt in; Anarchy rebuilt against it, its suites and `assets-demo` compared.
5. Measure, update this table, and update JadeFX's README on raw GL in a node.
