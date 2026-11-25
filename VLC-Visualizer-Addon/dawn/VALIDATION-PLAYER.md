# Native player audio and window ownership

Development evidence observed 2026-11-15. This is the native workbench's
ownership layer; production dispatch and real VLC/Winamp playback remain
tasks D40/D44. Generated PCM fixtures are not real player validation.
All test data and analysis stay on the owner's machines.

## Audio source isolation

`dawn_window --vlc-pid <pid>` (also `--player-pid`) consumes the existing
player PCM mapping. It does not create a native recording context, request
microphone permission or show the input picker. Ctrl-L/Command-L leaves the
player's audio source unchanged. Conflicting `--line-in`, `--select-input`,
`--synthetic` and reselection-test arguments are rejected before opening
graphics or capture. Standalone recording remains a separate path.

The reader uses the existing pointer-free audio IPC layout and its nonblocking
lock. It interpolates the newest samples to 2,048 stereo float frames at
44.1 kHz, without the legacy 8-bit waveform conversion. Invalid formats,
nonfinite samples, future/stale timestamps and missing data are handled
explicitly. Stale/missing/contended input becomes silence. A missing mapping
is retried at most four times per second; the consumer releases its mapping
and lock handles at shutdown. The existing producer's amplitude/folding policy
is unchanged by this layer.

`player_audio_probe` checks exact float order/precision across ring wrap,
48-kHz interpolation, finite-value sanitation/headroom bounds, short-history
padding, malformed/stale metadata, and discovery of a mapping created after
the reader. It uses the same platform monotonic clock as the existing writer.
These cases pass on all three systems and under Linux ASan/UBSan with leak
detection. This does not instrument the full GPU driver.

## Process lifetime and closure

The host keeps a Windows process handle, macOS kqueue process-exit watch or
Linux pidfd for the owner. Once established, that kernel reference tracks the
particular process through exit, rather than rebinding to a reused PID.
Linux player mode requires pidfd support; a missing/unsupported process watch
produces an explicit error. Standalone mode has no player watch.

Owner exit stops its renderer loop and releases the associated native window.
Nonce-protected control registrations include the real owner PID and suite
path. `--suite` selects the suite used by menu close requests; without it the
workbench uses its output directory. Closing a visualizer does not terminate
the player.

`tools/check_dawn_player.py` passes on Windows D3D12, Mac Metal and Linux
lavapipe. Each run starts only its own bounded generated-PCM/idle helper:

- Two renderer windows receive one owner's generated float PCM and both close
  after that owner exits.
- A missing audio bridge renders silence and still closes on owner exit.
- A startup-fullscreen window closes on owner exit.
- A nonce menu-close request closes the renderer while its owner remains alive.
- Every window blocks the recording-selection shortcut and opens no picker or
  recording stream. Conflicting source arguments and malformed PIDs fail.
- Control registrations and generated POSIX IPC files are released.

The current complete native CTest sets pass: Windows 10/10, macOS 10/10 and
Linux 9/9. Real player audio, rapid lifecycle/soak coverage and final installers
remain separate release gates. The Mac standalone recording milestone passed
on the settings build after owner approval; subsequent ad hoc rebuilds can
require another normal OS approval during final recording checks.
