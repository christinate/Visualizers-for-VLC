# API 3 WebGPU visualizer packages

API 1 (legacy Windows) and API 2 (native OpenGL) remain supported. API 3 uses
the bundled local browser runtime for a sandboxed WebGPU renderer. See
`schemas/manifest-v3.schema.json` and `examples/manifest-webgpu.json`.

Install your files to `visualizers/plugins/YOUR_ID/`. IDs contain lowercase
ASCII letters, numbers, underscores or hyphens and must be unique. Use a
manifest for the target platform and architecture (`windows`/`linux` with
`x86_64`, or `macos` with `arm64` for the current candidates). Set
`schema_version` and `api_version` to 3, `adapter` to `webgpu`, and `entrypoint`
to a relative HTML path. A package path must stay within its own directory.
Include your code/assets, licenses and any attribution notices there.

The entry HTML can use the shared runtime:

```html
<link rel="stylesheet" href="visualizer://runtime/common.css">
<body class="renderer"><canvas id="canvas"></canvas><div id="error"></div>
<script type="module" src="visualizer://runtime/renderer.mjs"></script>
```

Place `adapter.mjs` alongside `index.html`. Export a `Renderer` class with
`constructor(canvas, settings)`, async `init()` returning diagnostic metadata,
`resize()`, `render(audio, dt, seconds)`, async `done()` that waits for its GPU
queue, and `dispose()` that releases resources. Optional `key(key)` receives
unreserved keyboard input. The host reserves Escape, Ctrl-L, F11 and Alt-Enter.
It handles double-click fullscreen and right-click closure for you.

`audio` contains a 2048-sample mono `wave`, a 1024-bin `spectrum`, `rms`, `bass`,
`visual_energy`, `beat` (0/1), `beatEnvelope`, and cumulative `beatCount`.
The capture/IPC layer retains float PCM; Classic compatibility adapters still
convert into the original eight-bit Winamp visualization ABI where required.
Use fixed-size history buffers, wait for GPU completion, and destroy textures
on resize/close. Report initialization failures by throwing an Error.

Renderer pages have no Node access, no arbitrary native module loading and no
external network access. The main process exposes a limited preload API for
PCM, settings, window controls and the standalone recording-input selector.
Do not depend on remote scripts, fonts or assets. Audio is not recorded to disk.

An optional `classic` object may identify a compatible legacy entrypoint and
adapter, as in Prism Flow's manifest. It is not needed for a WebGPU-only plugin.
HDR requires a compatible display, GPU and OS configuration. Test SDR fallback;
RGBA16F feedback storage alone does not establish HDR display output. The
current Mint test GPU does not expose a compatible WebGPU adapter.

Your NSIS installer should copy only your package's owned files and run:

```nsis
ExecWait '"$VLCROOT\vlc.exe" --scan-visualizers' $0
```

The persisted installed catalog supplies menu rows on the next VLC launch.
After removal, run the scan again while the addon remains installed. Menu
clicks revalidate the manifest and entrypoint before starting a renderer.
Missing packages produce a controlled unavailable result. The menu's Close
command remains available even if a package was removed while its window ran.

For standalone and Winamp distribution, use the pinned SDK release tooling
and the official runtime hashes from Project 3. Do not replace another
package's runtime or registry ownership. The SDK builds a 64-bit rendering
host and x86/x64 Winamp capture bridges. Standalone mode accepts `--line-in
default`, a device ID, or an exact case-insensitive input label; Ctrl-L opens
the list again. Include all runtime/source license notices in your release.
