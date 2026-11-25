# Rhythm Terrain

Native candidate: **0.5.0-rc.1**, on `feature/dawn-native`. Final releases are
pending validation. Each project keeps its own Git history, version and release
artifacts. The Addon repository's `todo.md` tracks release work and its
`RELEASES.md` points to the current packages.
Mac return validation resumed on 2026-11-17; remaining Mac gates stay open.
Fresh arm64 dependencies/binary drafts target macOS 11.0; execution is tested
on macOS 26.6.2 only. Older macOS and Intel Macs remain unqualified. Preserved
earlier drafts retain their macOS 26+ requirement.

Rhythm Terrain uses a waveform-shaped valley and eight selectable cameras.
The flyover, ridge, survey and skimmer views emphasize the height pulse.
Sidewall and reverse-bank emphasize the wall-width pulse. By default a beat
sets the enabled dimension to 200% and it returns to 100% after 0.5 seconds.
Each camera can enable either pulse. Their timing and peak are independent.

**Beach** follows a neon shoreline. Wavefronts use the current waveform and
repeat at randomized intervals. Waves advance from sea to sand, then recede
and fade. Every fourth beat sends spray over a rock, with occasional rainbows
in the mist. Successive impacts move toward the camera. Surf and rain send
ripples through the same water surface that reflects the sky and refracts the
seabed. Caustic light plays across the sand and layered rocks in the shallows.
The shoreline has tiled roofs, porches, palm leaflets and textured rock
outcrops. Offshore, sea turtles dive independently for 30 seconds by default
and continue swimming underwater. Fish and bottlenose dolphins leap on a beat
about once a minute and once every five minutes, respectively. With the Beach
window focused, press **D** for a close view of a dolphin jump. The scene
returns to its regular camera afterward.

**Objects** cycles through five original implicit 3D forms inspired by the
isosurface approach documented by [GPUCaster](https://www.geisswerks.com/gpucaster/tutorial.txt).
Beats swell the current form, the waveform controls rotation, and low and high
energy bend its joints and surfaces. Each shape has independent switches and
size, rotation, note and beat controls.

**UnderSea** moves between a stationary aquarium view and a glide through
branching, fan, tube and brain coral. Some colonies grow together on the reef,
with kelp rising between them. A slow current reverses every 15 seconds by
default and carries the kelp and caustics together. Overhead, the water bends
and reflects the light. Fish schools surge on beats and seahorses swim around
the reef. Mature kelp occasionally breaks free while a new stalk grows.
Dolphins, a screen-spanning whale, and a close turtle swim through at longer
intervals.

**CoralCanyon** travels through a winding channel between coral-covered walls.
The same current pushes the kelp and fish across the canyon. Press **Space**
or **C** to switch between the low and raised glides. The canyon dimensions,
reef detail, current, and camera speed are adjustable in `coralcanyon.ini`.

Waveform history uses RGBA16F textures. HDR is **Experimental**, opt-in and off
by default. Windows D3D12 checks the actual monitor/OS state and float16 surface
before scRGB, with SDR fallback. Mac Metal uses Experimental EDR with SDR fallback at headroom 1.0;
Linux native HDR transport remains open. Physical
HDR appearance is unverified because no HDR display is available.

## Use

All five visualizers have standalone and VLC packages. The combined addon adds
them to VLC in one installer. Windows standalone packages also offer Winamp
bridges. VLC plugin-only installs reuse the shared native runtime.

Standalone mode opens an audio-input selector. Ctrl-L reopens it (Command-L
on macOS). A default input, exact device ID or exact case-insensitive label
can be selected at startup:

```text
Visualizers.exe --visualizer terrain --line-in default
Visualizers.exe --visualizer beach --line-in "Recording device name"
Visualizers.exe --visualizer objects --demo
Visualizers.exe --visualizer undersea --demo
```

On Unix use `Visualizers` without `.exe`. Player-owned windows consume their
player's decoded PCM; source selection belongs to standalone mode. The
visualizer does not save recordings.

Windows open borderless at the configured size. Double-click toggles fullscreen
and restores the windowed bounds; the saved fullscreen preference controls
startup. Right-click or Escape closes the window. VLC provides per-visualizer
Open, Settings and Close, plus Close all and Rescan commands. Display changes
apply on the next launch. Uninstall retains user settings and refuses changed
owned files until they are repaired.

Space advances the camera in Rhythm Terrain and Beach, or the shape in Objects.
In UnderSea, Space previews the next enabled event in order. C switches the
camera and L holds or releases the view. The `--demo` option plays a generated
signal.

## INI configuration

Complete defaults live in the visualizer's installed plugin assets directory
and in [terrain.ini](assets/terrain/terrain.ini), [beach.ini](assets/beach/beach.ini),
[objects.ini](assets/objects/objects.ini), [undersea.ini](assets/undersea/undersea.ini),
and [coralcanyon.ini](assets/undersea/coralcanyon.ini). Put a copy using the same filename
in the user profile directory. The copy can contain a subset of settings.
Reopen the visualizer to read changes. Invalid values report the option name
on startup. Keep at least one camera or object enabled.

Beach's `[water]` section controls clarity, caustics, chop and how strongly
rain and spray disturb the surface. In `[ocean_life]`, set
`turtle_dive_seconds=0` to keep the turtles at the surface, or adjust dive
depth and the usual time between dives.

Beach adds **Turtle Follow** to the camera cycle. Press Space to advance to it.
When the view comes up in the rotation, it stays with the selected turtle
from the surface through the dive and back up. `[turtle_follow]` controls the
camera's availability, turtle selection and distance. `[landscape]` controls
dune height, sky color intensity and sand grain.

The first time Beach prepares shaders for a build and graphics adapter, it
shows an animated progress bar and saves the compiled shader data in the
user profile. Its beach-chair visitor sits between the rocks and the houses
and nods gently on the beat. `[beach_chair]` controls the visit interval,
visibility duration and head movement. The houses cycle through independent
wall and roof colors.

| Platform | Profile directory |
| --- | --- |
| Windows | `%LOCALAPPDATA%\VLCVisualizers\profiles` |
| macOS | ~/Library/Application Support/VLCVisualizers/profiles |
| Linux | $XDG_CONFIG_HOME/vlc-visualizers/profiles, or ~/.config/vlc-visualizers/profiles |

The host's Settings dialog continues to manage the window and input. Set
Terrain's INI camera seconds and palette beat count to zero to use the host's
corresponding settings.

## Validation and limits

An actual Windows VLC run reaches 3 h 20 min / 718,474 frames and closes through
VLC's menu. All four camera modes are active. Two snapshots 393 seconds apart
after warm-up have unchanged private/working-set/GPU memory, handles and
threads. This is bounded evidence, not continuous peak tracking or a universal
no-leaks claim. Steady tones in that run do not qualify musical beat response.

Linux actual menu/playback checks also pass using lavapipe. The white diagonal
was reproduced and corrected in raw GPU output, with an animated pixel
regression; hardware/backend findings remain recorded separately. Real Winamp
UI, remaining media checks, final-source soaks/installers and Mac capture work remain
open in the Addon checklist. No browser WebGPU adapter claim is inferred for
the new native Vulkan path.

NVK window support on the tested Xorg session and an unsuppressed 64-byte
NVK/Dawn allocation finding remain open; the Windows run does not qualify
that Linux backend. Mac passes actual menu actions and ten generated media
formats with decoded PCM; those checks do not qualify beat counts for every
track. Mac arm64 rebuild, EDR fallback and bounded
archive ownership tests pass; they do not qualify extended Mac stability.

Native targets are `VisualizerNative`, `terrain_native`, `Visualizers` and
`VisualizersCtl`. The SDK-only runtime loads the terrain module through API 4
and does not link the terrain implementation into the host.

## Build and package

Use this repository's pinned source SDK; an Addon checkout is not required.
See [sdk/NATIVE-RUNTIME.md](sdk/NATIVE-RUNTIME.md) for dependency setup,
Dawn provenance, build directories and platform packaging arguments.

Windows uses x64 MSVC, CMake, Python 3.11+, Git, NSIS and the pinned Dawn/SDL
inputs. Configure with `-DVIS_BUILD_DAWN_RUNTIME=ON`, the source-built
`Dawn_DIR` and `SDL2_DIR`. With those dependencies prepared:

```powershell
python sdk/renderer-release.py --dawn-install sdk/build/dawn-install --sdl-sdk sdk/build/sdl-sdk
```

The builder creates a native standalone runtime, plugin, notices and matching
x86/x64 Winamp bridges. It audits imports and refuses occupied staging/output
paths. `--package-only` accepts explicit existing build directories. Final
artifacts must come from the final committed source and pass the release gates.

Linux x86_64 and macOS arm64 archives use `sdk/renderer-release-native.py`;
supply host/runtime/module paths as documented in the SDK guide. Mac builds
also require `--sdl-library`; libraries are relocated and staging copies receive
locally verified ad-hoc signatures. VLC-only installation adds just the plugin
to the existing runtime. Final-source/platform QA remains open; Mac archives
are not Developer ID signed or notarized.

## Source archives and prior versions

From a clean committed checkout, run `python tools/source_release.py`.
It writes a fresh `releases/<VERSION>/source/` directory; `--output PATH`
selects another fresh location. Archives, source metadata and checksums bind to
one fixed Git commit. Nothing is published automatically.

The v0.4.0-rc.1 tag preserves the browser-based candidate; v0.3.0-rc.1 records
the earlier engine port. Use the matching tag and its README to reproduce
those packages. Their old validation does not qualify new native binaries.

## Licenses

The terrain renderer is MIT licensed; see [LICENSE](LICENSE). The shared host,
bridges and SDK retain their GPL and dependency notices. Keep Dawn/SDL/runtime
licenses with distributed packages. The Addon's historical 0.2 installer
detection remains unresolved; see its SECURITY.md. It has not been classified
as a false positive.
