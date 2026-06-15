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
- `.hanso` v2 binary container export with JSON metadata plus dry/captured/aligned audio chunks.
- Guided Capture Wizard with Standard Capture and Easy Capture modes.
- Easy Capture mono-left output routing: Left = test signal, Right = silence.
- Basic Amp Liquid Gain recipe with Gain 10%, 50%, and 100% anchor captures.
- Per-step capture quality checks for signal presence, clipping, level range, length, latency, and alignment confidence.
- Capture-first UI with a top settings button for audio/log utilities.
- `Finish Capture` analysis/validation and an `Export` button that opens the package form.
- Branded final package write action named `Let's HANSO!`.

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

1. Press the top-right settings button and choose the input/output device, sample rate, and buffer size.
2. Return to the main Capture Wizard.
3. Choose `정식 캡쳐` or `간편 캡쳐`.
4. Follow the connection guide and amp safety warning.
5. Complete the checklist: setup, calibration, Gain 10%, Gain 50%, Gain 100%, and final validation.
6. Press `Start Capture` on each step. HANSO Lab generates the test signal automatically.
7. Press `Finish Capture` to run analysis and validation without sending any new output signal.
8. Press `Export`, enter package metadata in the popup, then press `Let's HANSO!` to write a categorized `.hanso` file.

The old manual test-signal generator is kept in the Capture tab's `Advanced` section for developer checks only.

## `.hanso` Package Shape

Phase 2 exports a single-file binary package:

```text
HANSO2
int32 formatVersion
int64 metadataJsonSize
metadata JSON bytes
int32 chunkCount
chunk records...
```

Every package includes a category field such as `Amp`, `Pedal`, `Cabinet`, `Speaker`, `Pickup`, `Rig`, `Utility`, or `Unknown`.

Manual capture chunks may still use HANSO float32 audio encoding and include:

- `capture/dry-reference.f32`
- `capture/captured.f32`
- `capture/aligned-captured.f32`

Guided Liquid Gain captures use compact PCM16 audio chunks. The dry reference is shared instead of duplicated per anchor, and each anchor stores the aligned capture used for analysis:

- `capture/shared/dry-reference.pcm16`
- `capture/gain-010/aligned-captured.pcm16`
- `capture/gain-050/aligned-captured.pcm16`
- `capture/gain-100/aligned-captured.pcm16`

Guided capture metadata is stored under `captureWorkflow`, including capture mode, output routing, cable guide, fixed knob instructions, anchor status, quality reports, chunk ids, and a future interpolation placeholder.

The metadata also includes residual dataset preparation info so future neural residual training can reconstruct aligned input/target segments.
