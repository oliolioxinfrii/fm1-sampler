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
(cd .fm1-base && bash tools/setup_toolchain.sh)
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
