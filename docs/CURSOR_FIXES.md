# Cursor Debug / Refactor Fixes

This file records stabilization fixes that must not be reverted by Codex.

**Canonical reference commit:** `c9a98bb` (`Phase 2: capture wizard, model extraction, and tone preview.`)

Phase-3 Codex work regressed several items listed under 2026-06-15. Those regressions were **restored on 2026-06-16** (see entries below). Before deleting or simplifying related code, read this file first.

## Format

### YYYY-MM-DD — Short title

Files:
- `path/to/file.cpp`

Problem:
- What was broken?

Fix:
- What changed?

Do not revert because:
- Why this is required?

Regression symptoms:
- What happens if this is removed?

---

### 2026-06-15 — Calibration right-column layout (meters invisible)

Files:
- `Source/UI/CapturePanel.cpp`

Problem:
- During Calibration, Output/Return level meters were not visible. The right info column collapsed to 0px width because flexible column splitting consumed all horizontal space.

Fix:
- Reserve a fixed right column with `infoColumnWidth = juce::jmin(460, columns.getWidth())` and `columns.removeFromRight(infoColumnWidth)` before laying out the left step list.
- Give each capture row action button a fixed width (`Start` 76px, `Re-Capture` 94px, `Stop`/`Reset` 54px, etc.) instead of sharing leftover space proportionally.

Do not revert because:
- Users cannot complete calibration without visible output/return meters and tappable step controls.

Regression symptoms:
- Calibration screen shows steps on the left but no meters on the right.
- Start / Re-Capture / Stop / Reset buttons shrink to uneven sizes per row.

---

### 2026-06-15 — Advanced section pushed off-screen

Files:
- `Source/UI/CapturePanel.cpp`

Problem:
- The Advanced output-level area was laid out after the main columns without reserving height, so it could end up below the visible panel bounds.

Fix:
- Reserve `advancedHeight` and `completionHeight` before computing `columnsHeight`:
  `columnsHeight = juce::jmin(area.getHeight(), juce::jmax(500, area.getHeight() - advancedHeight - completionHeight - 28))`.
- Place Advanced controls in the bottom `area` region after the main columns.

Do not revert because:
- Advanced calibration/output controls must remain reachable without scrolling off a fixed-size panel.

Regression symptoms:
- Advanced section disappears or is clipped at the bottom of Capture tab.

---

### 2026-06-15 — Calibration output control uses dBFS, not linear amplitude

Files:
- `Source/UI/CapturePanel.cpp`
- `Source/Capture/CaptureEngine.cpp`
- `Source/Capture/CaptureEngine.h`
- `Source/Audio/CaptureAudioSource.cpp`
- `Source/Audio/CaptureAudioSource.h`

Problem:
- Calibration output was controlled by a linear amplitude slider (`0.001`–`0.12`). Users saw ~-42 to -38 dBFS on the meter while the UI target band was -42 to -24 dBFS, which was unintuitive and too quiet for practical calibration.

Fix:
- Replace amplitude slider with dBFS slider (`-50` to `-18`, default around `-33`).
- Add `setCalibrationOutputDb()` / `calibrationOutputDb()` API.
- Convert dBFS to signal amplitude with `juce::Decibels::decibelsToGain()` when generating the calibration test tone.
- Add `CaptureAudioSource::replaceCalibrationSignal()` to swap the looping calibration buffer **without stopping** the calibration monitor when the slider changes.

Do not revert because:
- Calibration safety targets are defined in dBFS (`calibrationOutputMinDb` / `calibrationOutputMaxDb`). The UI must speak the same unit as the meters and auto-pass logic.

Regression symptoms:
- Calibration output stays too quiet even when the slider is moved.
- Users must guess linear amplitude values instead of targeting dBFS.
- Changing output level during calibration stops/restarts the monitor or has no audible effect.

---

### 2026-06-15 — Build fix: `calibrationOutputDb` member name collision

Files:
- `Source/Capture/CaptureEngine.h`
- `Source/Capture/CaptureEngine.cpp`

Problem:
- A member variable and accessor method were both named `calibrationOutputDb`, causing compile failure.

Fix:
- Rename the stored value to `userCalibrationOutputDb`; keep public accessor `calibrationOutputDb() const`.

Do not revert because:
- Required for a clean build after introducing the dBFS calibration API.

Regression symptoms:
- CMake build fails with duplicate symbol / undeclared identifier errors around calibration output accessors.

---

### 2026-06-15 — Gain 10% capture quality lower bound (-60 dBFS)

Files:
- `Source/Capture/CaptureEngine.cpp`

Problem:
- Gain 10% anchor quality checks used a higher RMS floor than intended for low-gain captures, causing false warnings/failures on valid quiet captures.

Fix:
- In `qualityTargetForStep()`, use `{ -60.0f, -12.0f, "Gain 10%" }` when `normalizedValue <= 0.15f`.

Do not revert because:
- This threshold was explicitly requested for real-world low-gain amp captures.

Regression symptoms:
- Gain 10% step fails or warns on otherwise acceptable quiet returns (for example when RMS is between -60 and -48 dBFS).

---

### 2026-06-15 — Preview gain API split (load vs. set gain)

Files:
- `Source/Audio/PreviewModelProcessor.cpp`
- `Source/Audio/PreviewModelProcessor.h`
- `Source/Audio/CaptureAudioSource.cpp`
- `Source/Audio/CaptureAudioSource.h`
- `Source/Capture/CaptureEngine.cpp`
- `Source/Capture/CaptureEngine.h`
- `Source/UI/TonePreviewPanel.cpp`
- `Source/UI/CapturePanel.cpp`

Problem:
- `loadPreviewModel(model, gain)` mixed model loading and gain control. Reloading a model reset gain unexpectedly, and multiple call sites passed gain inconsistently.

Fix:
- `loadPreviewModel(const CompactHansoModel& model)` loads anchors only.
- `PreviewModelProcessor::loadModel()` reads current gain from `targetParameter.load()` so reload preserves slider value.
- `setPreviewGainPercent(float percent)` updates gain live without reloading the model.
- UI pattern after load:
  ```cpp
  capture.loadPreviewModel(model);
  capture.setPreviewGainPercent(static_cast<float>(gainSlider.getValue()));
  ```

Do not revert because:
- Tone Preview gain must update during playback without reloading DSP state or restarting transport.

Regression symptoms:
- Gain slider changes do nothing until playback restarts.
- Loading a model snaps gain back to 50%.
- Duplicate/conflicting `loadPreviewModel(model, float)` overloads reappear across engine/UI layers.

---

### 2026-06-15 — Preview cabinet DSP only when amp model is loaded

Files:
- `Source/Audio/CaptureAudioSource.cpp`

Problem:
- Cabinet preview filtering was applied even when no amp model was loaded, coloring clean/sample-only preview incorrectly.

Fix:
- Gate preview DSP chain:
  ```cpp
  if (previewProcessor.hasModel()) {
      previewProcessor.process(...);
      if (previewCabinetEnabled.load())
          cabinetProcessor.process(...);
  } else {
      // copy dry preview sample only
  }
  ```

Do not revert because:
- Clean preview must remain unprocessed; cabinet coloration is an amp-preview enhancement, not a global monitor effect.

Regression symptoms:
- With no model loaded, toggling Cab still changes the sound.
- Clean sample preview sounds filtered when it should be dry.

---

### 2026-06-15 — Use `PreviewCabinetProcessor`, not inline preview biquads

Files:
- `Source/Audio/PreviewCabinetProcessor.cpp`
- `Source/Audio/PreviewCabinetProcessor.h`
- `Source/Audio/CaptureAudioSource.cpp`
- `CMakeLists.txt`

Problem:
- An earlier inline 5-biquad stub in `CaptureAudioSource` was a placeholder and did not match the intended cabinet voicing. A later refactor also placed cabinet filtering in `applyPreviewMonitoring()`, which runs after normalization/output gain and is the wrong point in the chain.

Fix:
- Port Guitar Plugins `CabinetModelFx` chain into `PreviewCabinetProcessor` (American Combo + Dynamic mic + Cap Edge defaults).
- Call `cabinetProcessor.prepare()` in `prepareToPlay()`.
- Call `cabinetProcessor.process()` immediately after `previewProcessor.process()` in the preview audio path.
- Reset via `cabinetProcessor.reset()` (not a removed `resetCabinetFilters()` helper).

Do not revert because:
- Deleting `PreviewCabinetProcessor` and reverting to inline biquads loses the reference voicing and breaks the intended amp+cab preview signal flow.

Regression symptoms:
- Build error: `use of undeclared identifier 'resetCabinetFilters'`.
- Cab preview sounds like a generic EQ rather than the reference cabinet chain.
- Cabinet coloration is applied at monitoring stage instead of on modeled amp output.

---

### 2026-06-15 — Tone Preview waveform seek (real-time safe)

Files:
- `Source/UI/PreviewWaveformComponent.cpp`
- `Source/UI/PreviewWaveformComponent.h`
- `Source/UI/TonePreviewPanel.cpp`
- `Source/Audio/CaptureAudioSource.cpp`
- `Source/Audio/CaptureAudioSource.h`
- `Source/Capture/CaptureEngine.cpp`
- `Source/Capture/CaptureEngine.h`

Problem:
- Users could not jump playback position by clicking/dragging the waveform during preview.

Fix:
- Add `PreviewWaveformComponent::onSeek` callback from mouse down/drag.
- Wire to `CaptureEngine::seekPreviewSample(double normalizedProgress)`.
- Update only atomic `previewSamplePlayhead` in the audio source (no allocations/locks on audio thread).

Do not revert because:
- Seek must remain real-time safe and UI-driven without stopping/reloading the sample buffer.

Regression symptoms:
- Clicking the waveform does nothing during playback.
- Seek implementation blocks or reallocates inside `process()`.

---

### 2026-06-15 — Preview sample resampling quality (Lagrange)

Files:
- `Source/UI/TonePreviewPanel.cpp`

Problem:
- Preview samples were resampled with simple linear interpolation when file sample rate differed from device rate, causing dull highs and aliasing.

Fix:
- Use `juce::LagrangeInterpolator` in `resampleLinear()` with `ratio = sourceSampleRate / targetSampleRate`.

Do not revert because:
- Tone Preview is an audition path; poor resampling makes models sound worse than runtime playback.

Regression symptoms:
- Imported WAV previews sound muffled or slightly detuned vs. source file.
- High-frequency content collapses on 44.1 kHz → 48 kHz conversion.

---

### 2026-06-15 — Tone Preview Norm / Volume / Cab UI semantics

Files:
- `Source/UI/TonePreviewPanel.cpp`

Problem:
- Norm, Volume, and Cab controls had generic or misleading tooltips/enablement after refactors.

Fix:
- **Norm** tooltip: balances perceived level between clean vs modeled preview (AGC toward ~-20 dBFS RMS).
- **Volume** tooltip: final speaker/output monitor level.
- **Cab** tooltip: applies cabinet DSP to modeled amp output; enable only when `modelReady`.
- `cabinetButton.setEnabled(modelReady)` in `updateButtonState()`.

Do not revert because:
- These controls affect different stages of the monitoring chain; conflating them causes user confusion and invalid clean-preview expectations.

Regression symptoms:
- Cab toggle active with no model loaded.
- Users interpret Volume as gain modeling or Norm as speaker volume.

---

### 2026-06-15 — Remove dead `restartIfPlaying` gain restart flag

Files:
- `Source/UI/TonePreviewPanel.cpp`
- `Source/UI/TonePreviewPanel.h`

Problem:
- `updateGainModel(bool restartIfPlaying)` accepted a flag that was ignored (`juce::ignoreUnused`). It implied playback restarts were required for gain changes.

Fix:
- Simplify to `updateGainModel()` calling only `capture.setPreviewGainPercent(...)`.

Do not revert because:
- Gain updates are live via `setPreviewGainPercent`; reintroducing restart logic causes unnecessary playback interruptions.

Regression symptoms:
- Gain slider triggers stop/restart flicker or still does not update live audio.

---

### 2026-06-16 — Phase 2 regression restoration (Codex revert recovery)

Files:
- `Source/Audio/PreviewCabinetProcessor.cpp`
- `Source/Audio/PreviewCabinetProcessor.h`
- `Source/Audio/CaptureAudioSource.cpp`
- `Source/Audio/CaptureAudioSource.h`
- `Source/Audio/PreviewModelProcessor.cpp`
- `Source/Audio/PreviewModelProcessor.h`
- `Source/UI/PreviewWaveformComponent.cpp`
- `Source/UI/PreviewWaveformComponent.h`
- `Source/UI/TonePreviewPanel.cpp`
- `Source/Capture/CaptureEngine.cpp`
- `Source/Capture/CaptureEngine.h`
- `Source/UI/CapturePanel.cpp`
- `CMakeLists.txt`

Problem:
- Phase-3 Codex work reverted multiple Phase-2 stabilization fixes: inline preview biquads instead of `PreviewCabinetProcessor`, removed waveform seek, split gain API restored incorrectly, calibration dBFS slider reverted to linear amplitude, Gain 10% quality bound reverted to -48 dBFS, Lagrange resampling removed.

Fix:
- Restored `c9a98bb` audio preview chain and UI behavior listed in 2026-06-15 entries above.
- Re-added `PreviewCabinetProcessor` to `CMakeLists.txt`.
- Re-applied calibration dBFS API (`setCalibrationOutputDb`, `replaceCalibrationSignal`).
- Re-applied `qualityTargetForStep()` Gain 10% floor `-60 dBFS`.

Do not revert because:
- These fixes were explicitly validated in Phase 2 and documented as production requirements.

Regression symptoms:
- Same as individual 2026-06-15 entries if reverted again.

---

### 2026-06-16 — Tone Preview cabinet `.hanso` IR playback

Files:
- `Source/Audio/PreviewCabinetIrProcessor.cpp`
- `Source/Audio/PreviewCabinetIrProcessor.h`
- `Source/Audio/CaptureAudioSource.cpp`
- `Source/Capture/CaptureEngine.cpp`
- `Source/Capture/CaptureEngine.h`
- `Source/UI/TonePreviewPanel.cpp`
- `Source/UI/TonePreviewPanel.h`
- `CMakeLists.txt`

Problem:
- Cabinet `.hanso` packages could be exported but not auditioned in Tone Preview. Only amp compact models were supported.

Fix:
- Added `PreviewCabinetIrProcessor` to load `cabProfile.positions` IR chunks (`captured-ir` / `imported-ir`), blend by mic position, and convolve preview samples.
- Added `CaptureEngine::loadPreviewCabinetPackage()`, `setPreviewMicPositionPercent()`, `clearPreviewCabinetPackage()`.
- Tone Preview `Open HANSO` detects cabinet packages and switches slider to **Mic Position** mode.
- Preview audio path priority: cabinet IR processor → amp model (+ optional `PreviewCabinetProcessor`) → dry sample.

Do not revert because:
- Cabinet capture/repackaging workflow requires audition before export/runtime use.

Regression symptoms:
- Opening cabinet `.hanso` in Tone Preview falls back to amp/compact path or clean preview.
- Mic position slider has no effect on cabinet packages.

---

### 2026-06-16 — Manifest `packageKind` / `assetType` for runtime loaders

Files:
- `Source/Model/HansoPackage.cpp`
- `Source/Model/HansoCategory.h`

Problem:
- Exported `.hanso` manifest lacked `packageKind: toneAsset` and `assetType` (`amp`, `cabinet`, etc.). External runtimes had to infer asset type from `category` or `cabProfile` presence alone.

Fix:
- `createMetadataVar()` now always writes:
  - `packageKind: "toneAsset"`
  - `assetType: assetTypeForCategory(metadata.category)` (`amp`, `cabinet`, `pedal`, `full-rig`, …)
- Added `assetTypeForCategory()` and `hansoCategoryFromAssetType()` helpers.

Do not revert because:
- Guitar FX / game runtime loaders expect stable manifest discriminators, not inferred heuristics.

Regression symptoms:
- External tools cannot reliably distinguish tone assets from other package kinds.
- Cabinet vs amp routing in runtime breaks when `category` text varies.

---

### 2026-06-16 — Serializer reads `cabProfile` and `assetType` on import

Files:
- `Source/Serialization/HansoSerializer.cpp`

Problem:
- `HansoSerializer::readFromFile()` only parsed basic metadata fields. `cabProfile`, `captureWorkflow`, and `assetType` in the JSON manifest were ignored, so cabinet Tone Preview failed after re-opening an exported `.hanso` file.

Fix:
- On read, populate `package.cabinetProfile` from `cabProfile`, `package.captureWorkflow` from `captureWorkflow`.
- If `category` is unknown, infer from `assetType`.

Do not revert because:
- Without this, cabinet IR preview only works for unsaved in-memory packages, not exported files.

Regression symptoms:
- `Open HANSO` on exported cabinet file loads chunks but `cabProfile` is empty → cabinet preview fails.
- Tone Preview says no previewable IR positions despite successful export.

---

### 2026-06-16 — `previewCabinetRevision` member name collision (build fix)

Files:
- `Source/Capture/CaptureEngine.h`
- `Source/Capture/CaptureEngine.cpp`

Problem:
- Added cabinet preview revision counter named `previewCabinetRevision` conflicted with public method `previewCabinetRevision() const`, breaking compilation.

Fix:
- Rename stored counter to `previewCabinetRevisionCounter`; keep accessor `previewCabinetRevision()`.

Do not revert because:
- Required for clean build after cabinet preview API addition.

Regression symptoms:
- Compile error: duplicate member `previewCabinetRevision`.

---

## Notes for Codex (not protected fixes)

- `drawDebugBounds()` overlay rendering in `CapturePanel::paint()` is **temporary layout debug instrumentation**. It should be removed before release; do **not** treat it as production behavior to preserve.
