# HANSO Lab

HANSO Lab is a standalone JUCE desktop application for capturing, analysing, and exporting guitar-related hardware models into `.hanso` packages.

It is part of the wider HANSO ecosystem:

- HANSO Engine: realtime hybrid guitar/audio DSP runtime.
- HANSO Lab: capture, analysis, model-authoring, and `.hanso` export app.
- `.hanso`: universal HANSO asset package format.
- FX Plugin: a separate realtime plugin that will later load `.hanso` packages through HANSO Engine.

## Phase 1 Scope

Implemented in this starter project:

- C++20 JUCE standalone app structure.
- Audio device manager setup.
- Reusable test signal buffer generation: log sine sweep, white noise, pink noise, impulse burst, and multi-sine.
- Capture session state with session id, dry/reference audio, captured audio, latency, analysis, and export status.
- Realtime-safe playback and mono recording path for basic amp capture.
- Deterministic cross-correlation latency estimation and captured-audio alignment.
- Basic peak, RMS, frequency-response, transfer-curve, and dynamic-envelope analysis.
- Residual dataset preparation structure for future neural residual learning.
- `.hanso` package metadata model.
- Readable JSON `.hanso` export with a serializer API that can later become a binary or ZIP-like container.
- Modular UI panels for audio, capture, analysis, assets, and logs.

Intentionally left for future phases:

- Heavy neural residual training.
- GPU training.
- LibTorch or ONNX Runtime integration.
- Commercial-grade capture automation.
- FX plugin integration.
- Full HANSO Engine realtime playback integration.

## Build On macOS

Install JUCE so CMake can find `JUCEConfig.cmake`, then run:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DJUCE_DIR=/path/to/JUCE/install/lib/cmake/JUCE
cmake --build build
open build/HANSO_Lab_artefacts/Debug/HANSO\\ Lab.app
```

If JUCE is installed in a standard CMake package location, `-DJUCE_DIR=...` may not be needed.

In this starter project, JUCE is intentionally treated as an external SDK/package instead of being downloaded automatically.

## First Capture Test

1. Open the Audio tab and choose the input/output device, sample rate, and buffer size.
2. Open the Capture tab.
3. Select `Log sine sweep`, set duration and amplitude, then press `Generate Test Signal`.
4. Connect the selected output through the amp or device under test into the selected input.
5. Press `Start`; capture stops automatically when the generated signal ends.
6. Open the Analysis tab and press `Run Basic Analysis`.
7. Open the Assets tab, enter metadata, and export a categorized `.hanso` file.

## `.hanso` Package Shape

Phase 1 exports a readable JSON package:

```text
{
  "format": "hanso",
  "formatVersion": 1,
  "category": "Amp",
  "captureSettings": {},
  "analysisSummary": {},
  "futureExtensions": {}
}
```

Every package includes a category field such as `Amp`, `Pedal`, `Cabinet`, `Speaker`, `Pickup`, `Rig`, `Utility`, or `Unknown`.
