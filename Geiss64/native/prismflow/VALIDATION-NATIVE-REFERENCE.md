# Original JavaScript on the matching native Dawn backend

Measured locally on 2026-11-16. This resolves the renderer-port question behind
the retained Linux cross-adapter failures. No production rendering method or
shader was changed for this comparison. D35 remains open for the final Mac
rerun and outstanding release validation; macbook1 is offline by owner request.

## Independent reference and scope

`tests/prismflow_dawn_reference.cjs` runs the preserved original JavaScript
Engine/GPUWarp/presenter through Dawn's official Node bindings, at the same
pinned Dawn revision as C++: `7daf3b0bee2c73621001ab15b256b5a84fb0b45f`.
It never imports the C++ visualizer renderer. Existing browser fixture
harnesses execute the same original methods and WGSL; only their asset imports
are rewritten to local files. Original source and harness SHA-256 values,
binding SHA-256, Node version, requested backend and actual adapter are recorded.
The binding recorder checks clean pinned Dawn source and hashes the installed
shared library against its SDK build provenance.

The scene fixture retains its existing deterministic seed/reset/export shims
and rejection of unexpected background-worker requests. A strict blank 1x1
Canvas shim permits original initialization only; any actual Canvas drawing
throws. Fonts, image decoding, background scheduling and microphone capture
are separate tests. The presentation fixture uses a synthetic title mask,
not platform font rasterization. Scenes replay the actual captured delayed
bokeh-lifetime scalar while still executing particle compute/draw. Scheduling
can change that scalar between independent captures, so matching different
capture files is not required for scene pass/fail.

Presentation references explicitly enable the documented native calibration
clamp correction. Its fix ID is recorded, and upstream assets remain untouched.
Full-scene references do not enable that correction because calibration is
inactive there. All 17 native shader extractions remain independently checked.
The runner has no HTTP server and disables fetch, XMLHttpRequest and WebSocket;
captures/reports stay on the owner's machines. Node, the binding, development
dependencies and reference data are excluded from production package plans.

## Results

Each presentation run checks 24 cases, 24,576 exact packed palette bytes,
648 exact uniform components and 3,710,976 output components. Each scene run
checks 36 complete frames and 5,381,100 feedback/color components, including
resize, transitions, keyboard changes and 1x/2x supersampling.

| Original JavaScript / C++ on the same backend | Presentation | Complete scenes |
| --- | --- | --- |
| Windows RTX 3070 Ti Laptop, D3D12 32.0.16.1060 | exact, zero differences | exact, zero differences |
| Linux GTX 1660 Ti, NVK Mesa 25.2.8 | exact, zero differences | exact, zero differences |
| Linux lavapipe, LLVM 20.1.2 / Mesa 25.2.8 | exact, zero differences | exact, zero differences |
| Linux SwiftShader 5.0.0 | exact, zero differences | exact, zero differences |
| Mac M1, Metal, latest source/reference runner | deferred M08 | deferred M08 |

Windows original-JavaScript/Dawn presentation additionally matches all 49 raw
fixture input/output files from the corrected Chrome/D3D12 capture byte for
byte. Earlier Chrome-based Windows/Metal/SwiftShader scene results remain in
VALIDATION-FRAME.md. They do not count as a rerun of the new Mac runner.

The tracked SDK-linked Linux binding recipe was built independently and all
six Linux comparisons above rerun using that resulting module, including
24 raw presentation dumps per backend. Each dump equals the corresponding
original output exactly. Windows also exercises RGBA16F/RGBA8/BGRA8 dumps and
the bounded title UV/alpha diagnostic. Diagnostic rendering is test-only.

### Retained cross-backend failures reproduced in the original renderer

These comparisons use two original-JavaScript outputs, with no C++ renderer:

| Comparison | Components differing | Maximum error | Mean error |
| --- | ---: | ---: | ---: |
| Title scene 6, original D3D12 vs original lavapipe | 17,970 / 61,440 | 1.80078125 | 0.019049369917289973 |
| First scene feedback, original NVK vs original SwiftShader | 32,447 / 40,960 | 0.82537841796875 | 0.0023502756986999883 |

These values reproduce the old native cross-adapter failures. Together with
the exact same-backend comparisons, they establish that the observed
differences also occur in the original effect across backends. They do not
identify a particular driver arithmetic instruction as the cause. The old
cross-adapter runs remain failures under their original limits; no tolerance
was widened, scene dropped or production effect substituted.

The independent unsuppressed 64-byte NVK CreateDevice driver allocation
finding remains unresolved. Exact pixels do not resolve allocation findings,
long-running stability, physical HDR or Mint's missing DRI3 window support.

## Reproduction

Use stock Node 24+ with native Float16Array; these runs used 24.22.3. A retained
Node 20 attempt used the original fallback half converter and failed scene
parity; the runner now refuses that configuration. An Electron attempt rejected
external buffers. No Electron security setting was disabled; stock Node was
used instead. Failed attempts remain separate from passing output directories.

Build the official pinned Dawn `dawn_node` target with
`DAWN_BUILD_NODE_BINDINGS=ON` in a development build. Follow the pinned source's
`src/dawn/node/README.md` and CMake files. Supply the pinned DEPS versions:

| Dependency | Revision / version | Download SHA-256 |
| --- | --- | --- |
| node-addon-api | 1e26dcb52829a74260ec262edb41fc22998669b6 | 2143a6730301e6d84e0c3c0c6057d15d7af18188058bd9d0ec1820a2a358f233 |
| node-api-headers | d5cfe19da8b974ca35764dd1c73b91d57cd3c4ce | ec068a441c7d5563cc0eb94f15b42eb056abc070c4408f0cbde3792daea0d6a1 |
| gpuweb webgpu.idl | 22507c95ccb9e705eddc7cab8f28aa46b134f5fe | 5293ea7f38416cb37b0ddfc636286514ff622901caa19504a4bdede2ec5a80b2 |
| Go, Windows AMD64 ZIP | 1.27.4 | 178ab98950ac69ab25069307dc6ade79f3816e414cce95b4da166a115cde1e7e |

Hashes identify the exact owner-local downloads, not a new production
dependency. Go must satisfy pinned Dawn's go.mod; use private GOCACHE/GOMODCACHE
and `GOTOOLCHAIN=local`. On Windows keep `NODE_API_BINARY_FILE=node.exe`,
build Release `dawn_node`, retain its adjacent shader compiler, and do not
install this development target over the production SDK. Production
`tools/build_dawn.py` continues to explicitly disable Node bindings.

For Linux, `tests/dawn_reference_binding/CMakeLists.txt` links only the official
Node adapter to the already installed pinned shared SDK, avoiding a second
Dawn backend build. Supply the same pinned original sources and generated
WebGPU.h/.cpp. The latter come from upstream's idlgen with its WebGPU templates,
Browser.idl, DawnExtensions.idl and pinned webgpu.idl; they can be copied from
an official Node build's `gen/node`. The platform-independent generated pair
used here came from the pinned Windows Node build. Dawn proc and ComboLimits
generated sources come from the installed SDK's matching native build.

```text
cmake -S tests/dawn_reference_binding -B <fresh-binding-build> -DCMAKE_BUILD_TYPE=Release -DDAWN_SOURCE=<pinned-dawn-source> -DDAWN_GENERATED=<native-dawn-build>/gen -DNODE_GENERATED=<node-generated-root> -DNODE_ADDON_API_DIR=<pinned-addon-api> -DNODE_API_HEADERS_DIR=<pinned-api-headers> -DDAWN_INSTALL=<pinned-shared-sdk>
cmake --build <fresh-binding-build> --parallel 2
python tests/dawn_reference_binding/record_provenance.py --module <binding.node> --dawn-source <pinned-dawn-source> --dawn-install <pinned-shared-sdk> --output <fresh-provenance.json>
node --expose-gc tests/prismflow_dawn_reference.cjs --module <binding.node> --provenance <provenance.json> --assets assets/prismflow --output <fresh-present-reference> --backend vulkan --case presentation --native-calibration-fix
prismflow_present_probe --assets native/prismflow/shaders --fixtures <present-reference>/fixtures.json --output <fresh-present-comparison> --backend vulkan --exact
node --expose-gc tests/prismflow_dawn_reference.cjs --module <binding.node> --provenance <provenance.json> --assets assets/prismflow --output <fresh-scene-reference> --backend vulkan --case scene
prismflow_scene_reference_probe native/prismflow/shaders <scene-reference>/fixtures.json <fresh-scene-comparison> --exact
```

Use `d3d12` on Windows. Mac `metal` execution remains M08. For both original
and native Linux processes select the same ICD in the child environment:
`VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json` for lavapipe, the retained
SwiftShader ICD path for SwiftShader, or the machine's normal NVK selection.
Inspect the recorded adapter rather than assuming environment selection worked.
Fresh reference directories prevent stale captures from becoming passing evidence.

## Owner-local evidence

Windows paths below are relative to `VLC-Visualizer-Addon/build`:

- `native-original-present-windows-v2/` and `native-original-scene-windows-v5/`:
  current stock-Node reference captures/provenance.
- `native-original-present-compare-windows-v3/` and
  `native-original-scene-compare-windows-v3/`: exact comparisons.
- `native-original-reference-evidence-v1/`: copied Linux reports/manifests,
  binding/configure/build provenance and original-to-original comparison data.
- `dawn-reference-deps-v1/`: development-only toolchain, failed-attempt logs
  and the preserved Windows Node-importing binding in `node20/dawn.node`
  (that directory name records its first runtime, not the required Node version).

On Mint, under `~/vlc-visualizers-work/`, the reproducible binding build is
`native-dawn-js-binding-recipe-v1/`; reference and comparison folders are
`native-original-{present,scene}-{lavapipe,nvk,swiftshader}-recipe-v1/` and
`native-original-{present,scene}-compare-{lavapipe,nvk,swiftshader}-recipe-v1/`.
Raw capture outputs and earlier failed cross-adapter reports are retained.

## Metal return run — 2026-11-17

The shared-SDK Node adapter recipe now builds on arm64 macOS using upstream
Node symbol lookup policy. Original JavaScript on pinned Dawn/Apple M1 matches
all 24 presentation cases (3,710,976 components) and 36 scene frames
(5,381,100 components) exactly, with unchanged limits. Stock Node 24.22.3
provides native Float16Array. Binding/input/SDK hashes and original raw outputs
remain in the owner-local mac-return-20261117-v1 workspace. Node/the adapter
remain development-only. No physical HDR or extended stability claim follows.
