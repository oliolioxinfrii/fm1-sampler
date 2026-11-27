# Surface v2 (0.41 experimental)

The phone remains authoritative for audio, project state and parameter formatting. The FM-1 runs the fm1-base/felucca-bsp platform, with a native renderer and a volatile cache of parameters, analysis data and pending encoder edits. No audio processing or project data is moved to the firmware. The original drawing protocol remains supported by the Android application for older firmware.

## Build

From the repository root on Linux/WSL:

```sh
bash tools/build_surface.sh --host
python3 tools/test_surface_firmware.py
cd engine
make -j"$(nproc)" "$PWD/build/test_surface_v2"
./build/test_surface_v2
cd ..
(cd .fm1-base && bash tools/setup_toolchain.sh --only uv,python)
(cd .fm1-base && bash tools/setup_toolchain.sh --only jieli,ac79-sdk)
bash tools/build_surface.sh
```

The build wrapper pins fm1-base, including its submodules, to f4bd9c5798f7e398f7a93e0d891f7d878121f1de. See that revision's README for platform dependencies. FM1_BASE_DIR can designate an alternate clean checkout. The firmware package is expected at surface-firmware/build/sampler.fwsc. The local experimental identity prefix FM-1_941 is not an allocated public identity; do not submit it to the public catalogue before coordination. Use the platform's standard local-file updater, not a raw flash write. Back up the existing firmware and restore stock through the existing updater before switching firmware families. Build Android 0.41 before switching: older APKs do not understand the semantic surface.

## Protocol

All messages use F0 7D 44 41 57 01 command payload F7, with seven-bit-safe payloads. The existing ping discovers SV_HELLO (44, version 2). Without HELLO, the phone uses the legacy graphics path. The shared wire definitions are in shared/surface_v2.h.

- SV_STATE (20): active or background page snapshot, context, acknowledged edit sequence, theme, transport, marker values, analysis generation and four parameter descriptors. Each descriptor contains flags, normalized value, normalized step, label and phone-formatted value.
- SV_EDIT (45): context, sequence, encoder slot, signed detent count. Firmware predicts normalized feedback and accelerates eligible continuous controls. Phone rejects stale-context and duplicate edits and returns authoritative state.
- SV_DATA (21): generation-tagged analysis chunks. Only complete transfers become visible; cached generations can be reused across page changes. SV_REQUEST (46) requests missing data. State and data are refreshed periodically to repair interrupted transfers.

State is not persisted to hardware flash. Background page snapshots populate the tape/generative cache opportunistically; field processing parameters are cached as pages are visited. Selection and navigation remain phone-authoritative. While a parameter edit is pending, the native renderer shows provisional normalized feedback rather than claiming exact formatted units. Graphs are native interpretations of the existing visual language, not pixel-identical reproductions. Spectrogram intensity and waveform peaks are computed on the phone; the firmware maps them to its palette. The learned noise profile still uses reduced raster data.

## Tests and release gate

The engine regression checks negotiation, edit acknowledgements, stale/duplicate edits, field event routing and analysis generations. The firmware host script checks native prediction, acknowledgements, incomplete analysis transfers, palette changes and USB reconnection. Run the existing engine suite as well. Before distributing, complete the device build, inspect reported memory use, check input and USB playback on FM-1, and compare native pages with the app in both themes. Slipway installation and rollback are integration checks, not changes to the community bootloader. The existing firmware remains the fallback.

## Installer discovery

Slipway Web requires Chrome Web MIDI with SysEx permission, matching input/output port names and a successful OTA identity handshake. A native USB backup utility detecting the unit does not establish OTA compatibility. Close other MIDI/USB apps, reconnect the cable and verify detection in Chrome; if legacy firmware does not answer the standard OTA handshake, use the existing workbench to restore a verified stock image before retrying. Do not flash a raw backup as a .fwsc package or assume a backup utility accepts arbitrary .fwsc files.

### Initial installation from a computer

After both native tests and the device build succeed, use desktop Slipway or the pinned platform's `tools/fm1_install.py --info` on a computer with direct USB MIDI access. If the legacy firmware does not respond, restore the verified stock V15 package with `firmware/FM1-Sampler-Workbench.html` using desktop Chrome, verify the stock identity, and then install `surface-firmware/build/sampler.fwsc`. Do not start installation from a device in an unidentified state. Android Slipway is a separate follow-up test.

The experimental build includes `words.tsv` for fm1-base's mandatory vocabulary generator. Its generated JSON is stored under `surface-firmware/web/words/`. The development warning about missing install/screenshot metadata is expected; no public release is being built yet.

The target is freestanding: do not include hosted C library headers in the device application. The BSP unity unit supplies memcpy, memset and memcmp; host-only comparisons use the simulator's libc. To resolve the toolchain setup's missing uv prerequisite, run `bash tools/setup_toolchain.sh --only uv,python` from `.fm1-base`.

## 0.41-experimental.2: discovery correction

PING is sent as its own SysEx message independently of v1 batching and CLEAR. The original v2 firmware does not parse v1 batches, so the first APK could never reliably negotiate it. No firmware rebuild is needed for this correction. Android logs MIDI device properties, opening both ports, outgoing PING and incoming HELLO/RECONNECT (`adb logcat -s fm1sampler`).

## 0.41-experimental.3: graphic fidelity and wider phone layout

The native compact glyphs now follow StudyGraphic.inc's 0.40 geometry, including the source-section blocks, six grain windows, motion trajectories, filter, feedback routing, reverb mesh, tape head and insert M/S display. The native FM-1 chrome is unchanged. Peak data, chord states and M/S energy are semantic inputs; no rendered graphics are transferred. The old compact reference is centered in its original plot, with approximate RGB565 blending and freestanding math rather than a guaranteed pixel-for-pixel match. Field process plots also use their original schematic silhouettes. Both tape and generator phone decks expand the graph column into extra available width, with the same vertical scale and inverse-transformed touch targets. The field recorder expands its graph canvas and surrounding bars. Android's protected display-cutout area remains respected. Native encoder edits now saturate at min/max; choices retain their confirmed label instead of overlaying a normalized percentage. The boundary checks include the GAUSS endpoint. Rebuild both APK and firmware, then test `test_study_layout`, `test_surface_v2` and `test_surface_firmware.py`.

## 0.41-experimental.4: Android display cutout

The previous width adjustment could only use the area allocated to the activity. Explicitly enable layout into the camera cutout (ALWAYS on API 30+, SHORT_EDGES on API 28–29), disable decor fitting on API 30+, and hide bars via WindowInsetsController with transient swipe behavior. The native layout retains the previous horizontal stretch and font scale. The physical camera hole may obscure a small part of the interface; inspect its actual bounds on the Pixel and refine control placement if necessary. `adb logcat -s fm1sampler` reports the root dimensions, origin and camera bounding rectangle. APK only; no firmware changes.

## 0.41-experimental.5: localized camera clearance

Use the left display-cutout bounding rectangle from Android, converted to native UI units and WebView viewport fractions. Only the generator's FIELD / PITCH text, field recorder's RESIDUE label and sublabel, and the editor's fourth track row adjust when their bounds overlap the camera. The fourth row retains its original right edge and touch target follows the shortened row. No global side inset, font resize, graph relocation, or firmware change. The layout regression uses a synthetic cutout; verify the actual Pixel position on-device.

## Wide native pictograph revision

Fix custom RGB565 raster output: the platform canvas is stored byte-swapped for panel transfer, so the direct pixel writer must use `swap16(c)` just like `cv_rect` and `cv_line`. Keep the original theme trace colors and change the in-window graph domain from 96 to 280 design units without increasing isotropic scale. Reverb uses 16 vertical strands with repeated waves and alternating overpasses; granular source uses all nine sections, grain windows and delay/arp repetitions fill the time domain, filter adds a drive-transfer graph, stereo insert adds both channel paths and M/S energy indicators. The keyboard stays compact and translates along an octave rail (seven positions for sampler, five for generator). Existing 0.41-experimental.6 APK already supplies normalized octave and signal state; no protocol or APK change. Test host palette byte order and wide plot coverage, then build device package. These are schematic adaptations, not exact replicas of the larger phone art.

## Native value labels and arpeggiator pattern correction

Use confirmed formatted text for every knob value, including during pending encoder edits; local prediction remains for plots and bars. The arpeggiator preview uses the DSP's continuous UP, DOWN, UPDOWN and CONVERGE index traversal and a deterministic illustrative RANDOM sequence. Three reference pitches per octave, event count from rate, first visible minimum/maximum accented once each, subsequent events neutral. This is a schematic rather than a display of live held MIDI notes. Regression tests cover pending text and pattern ordering/color assignments. Firmware-only change.
