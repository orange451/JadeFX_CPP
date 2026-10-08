# JadeFX style and layout performance

## Problem

Studio's "Styles and layout" profiler zone costs over 1 ms per frame. JadeFX restyles and re-lays-out the entire scene tree every frame with no caching:

- `Stage::frame` (src/stage/Stage.cpp:355) calls `Scene::layout` unconditionally, which calls `Node::applyStyles` on the root (src/scene/Scene.cpp:117) and then `performLayout`.
- `applyStyles` (src/scene/Node.cpp:514) always recurses into every child.
- Selector matching (`collectMatching`, src/style/Stylesheet.cpp:1039) linearly scans every rule for every node, once for the UA sheet and once per ancestor stylesheet, using string compares.
- Each node allocates about 9 vectors per frame and copies `ComputedStyle`, which contains `std::string` and `unordered_map` members.
- Declaration values (colors, lengths, `var()`) are re-parsed every frame through an `if (property == "...")` chain.
- Layout measurement is uncached. Containers such as `VBox` re-measure grandchildren at each nesting level.
- `:focus-within` walks the whole subtree for every candidate node.
- Popups are restyled separately (Scene.cpp:660).

## Goal

Make the zone measurably faster. There is no hard budget. Before and after numbers come from the benchmark and from Studio's profiler. Visible behavior must not change.

## Phase A: cheaper full pass (semantics unchanged)

1. **Instrumentation.** Add `FramePhase` sub-zones (or a nested-zone hook) for Styles, Layout, and Popups, and map them in AnarchyEngine's `UiFrameProfile`. Add a JadeFX benchmark (`tests/style_layout_bench.cpp`) that builds a Studio-sized tree (about 2,000 nodes, nested boxes, a realistic stylesheet with descendant selectors and `var()`) and reports the mean time per frame over 100 frames.
2. **Rule index.** When a stylesheet loads, bucket each selector by its rightmost compound: by id if present, else by first class, else by type, else universal. Matching collects candidates from the node's id, class, type and universal buckets, then orders them by (specificity, source order) exactly as today.
3. **Pre-parsed declarations.** At load time each `Declaration` stores a `PropertyId` enum and a parsed value variant (color, length, keyword, number, raw string). `applyDeclarations` dispatches on the enum. Values containing `var()` keep the raw string and resolve per node as today.
4. **Allocation removal.** Scratch vectors (matches, chain, the agent/author/important lists, kids) come from reusable per-pass buffers. `ComputedStyle` holds shared immutable handles (`std::shared_ptr<const ...>` or interned ids) for the transitions map, the box-shadow list and the font family, so copies are cheap. Default strings such as "Open Sans" are interned. `TimingOf` takes a `PropertyId`, not a string.
5. **Measure cache.** Each node caches its preferred width and height, keyed by the available size. In Phase A the cache is cleared at the start of each pass.
6. **themeColor.** Cache the parsed color for each theme-variable name. The cache is cleared when the theme changes.

Exit criteria: all existing JadeFX and AnarchyEngine tests pass unchanged, and the benchmark shows a measurable reduction. The numbers are recorded in the commit message.

## Phase B: incremental invalidation

### Flags

Each node has `styleDirty_`, `childStyleDirty_`, `layoutDirty_` and `childLayoutDirty_`. `markStyleDirty()` and `markLayoutDirty()` set the node's own flag, then set the child flag on each ancestor, stopping at the first ancestor where it is already set. New nodes start dirty.

### Style invalidation sources

- Class add or remove, id change, inline style change.
- Pseudo-state change (hover, pressed, focus, disabled, selected, and any other state used by selectors).
- Child add, remove or reorder. This dirties the parent's children, because structural pseudo-classes depend on order.
- Stylesheet add, remove or change. This dirties the owning node's subtree. A UA sheet or theme change dirties the whole tree.
- Inherited values. After a node is restyled, if its inheritable style differs from the previous one, its children are marked style-dirty.
- Focus change. Mark the old and new focus nodes' ancestor chains style-dirty. `:focus-within` then becomes a flag lookup, with no subtree walk.

### Layout invalidation sources

- A restyle that changes any layout-affecting property (size, padding, margin, border width, font, display, gaps, alignment, and similar). Paint-only changes such as color don't invalidate layout.
- Text or content changes, image size changes, child list changes, and stage or popup resize.
- A node whose layout is dirty marks its ancestors dirty, because preferred sizes propagate upward. The measure cache becomes persistent and is cleared whenever that node's layout is marked dirty.

### Transitions

A node with an active transition is added to the scene's animating set and is marked style-dirty each frame until all of its transitions complete. Then it is removed from the set.

### Frame pass

`applyStyles` skips a node that is clean and has no dirty children. If only `childStyleDirty_` is set, it recurses without restyling the node itself, reusing the node's stored inheritable style. Layout works the same way: a clean node keeps its bounds, and a node whose bounds didn't change and whose only flag is `childLayoutDirty_` lays out only its dirty children. Popups use the same flags.

### Verification mode

In debug builds, setting the `JADEFX_VERIFY_INCREMENTAL=1` environment variable runs a full pass with the dirty logic ignored after each incremental pass. It compares every node's computed style and bounds, and asserts with the node path and the field that differs on any mismatch. The JadeFX and AnarchyEngine test suites run once with the flag on in CI or local verification.

### Tests

- Unit tests for each invalidation source: mutate, run a frame, and assert the expected restyle or relayout, both that it happened and that clean siblings were not touched (counted with a debug counter).
- An idle-frame test: after the first frame, a static tree performs zero restyles and zero layouts.
- A transition test: only the animating node restyles while the transition runs.
- Benchmark numbers for an idle frame and for a hover-change frame.

## Out of scope

Rendering cost, multithreading, and changes to CSS feature coverage.
