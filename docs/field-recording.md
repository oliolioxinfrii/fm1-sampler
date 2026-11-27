# Field examination (experimental.4)

The Android field hub uses the approved Examination Study IV HTML as a bundled local asset. No remote web content is loaded. Its restoration/analysis DSP is JavaScript/Web Audio; it is separate from the real-time native sampler. The app pauses the Oboe streams while examination owns the audio devices and restarts them on exit. Android/Web Audio chooses the audio route; verify FM-1 USB output and microphone selection on device.

Hold HOME to enter/leave, or use PROJECT > FIELD RECORDING and tap the F badge to return. The previous native editor/deck is retained. SELECT, PRESETS, ALGORITHM and K1–K4 are routed through SamplerApp's MIDI parser. FX, SEL, ENV, LFO, EDIT, GLO, HOME, SAVE, ARP and SEQ follow the study; PLAY is transport, REC starts/stops PCM capture. Hold GLO changes the shared theme. The original editor remains accessible via EDIT/PROJECT. On the physical surface, a compact textual status replaces the phone spectrogram.

SAVE OUTPUT and SAVE RESIDUE write the selected interval into the native USER sample library, preserving the custom RIFF fm1 metadata chunk. The original source is not embedded. The recording remains in memory while the view is alive; it is not a persisted project. Capture is uncompressed mono PCM, limited to five minutes. Native engine and Android build/device verification are required. No firmware change.

Host: make -j1 and make "$PWD/build/test_field_controls" && ./build/test_field_controls from engine/.
Android: gradle :app:assembleDebug from android/.
