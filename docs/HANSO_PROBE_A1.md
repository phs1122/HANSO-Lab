# HANSO Probe A1

HANSO Probe A1은 Amp 계열 Gain anchor 캡쳐를 위한 HANSO 고유의 결정론적 합성 신호다.
외부 WAV, NAM `input.wav`, 타사 연주 데이터 또는 타사 파형을 포함하지 않는다.

목표:

- 하나의 고정 레벨 guitar program에 의존하지 않고 입력 레벨과 주파수 영역을 통제한다.
- 회로 기반 compact model fitting과 held-out fidelity 검증 데이터를 분리한다.
- Gain 100 / 50 / 10 세 anchor의 기본 재생 시간을 약 44.5초로 제한한다.
- 모든 peak를 calibration에서 결정한 안전 출력 ceiling 이하로 유지한다.

## Variant

### Full, 24.5 seconds

Gain 100 anchor에서 사용한다.

| Time | Segment | Purpose | Model fit |
| --- | --- | --- | --- |
| 0.0-0.5 | `noise-floor` | return noise baseline | no |
| 0.5-1.0 | `coded-sync` | latency/routing correlation marker | no |
| 1.0-3.0 | `reference-a-start` | start-state repeatability reference | no |
| 3.0-6.0 | `level-ladder` | six controlled input levels | yes |
| 6.0-10.5 | `dual-level-ess` | two-level synchronized exponential sweeps | yes |
| 10.5-14.5 | `odd-grid-multisine` | broadband response and nonlinear detection lines | yes |
| 14.5-19.5 | `transient-memory` | attack, release, gate, sag and recovery stress | yes |
| 19.5-21.5 | `guitar-program` | deterministic guitar-like program material | yes |
| 21.5-23.5 | `reference-a-end` | end-state repeatability and held-out fidelity | no |
| 23.5-24.5 | `tail-silence` | release and alignment guard | no |

The model-fit payload is 18.5 seconds. The two reference segments are sample-identical at the dry input.

### Delta, 10 seconds

Gain 50 and Gain 10 anchors에서 사용한다.

| Time | Segment | Purpose | Model fit |
| --- | --- | --- | --- |
| 0.0-0.5 | `noise-floor` | return noise baseline | no |
| 0.5-1.0 | `coded-sync` | latency/routing correlation marker | no |
| 1.0-4.0 | `level-ladder` | six controlled input levels | yes |
| 4.0-7.0 | `odd-grid-multisine` | compact broadband/nonlinear delta measurement | yes |
| 7.0-9.0 | `reference-a` | held-out fidelity reference | no |
| 9.0-10.0 | `tail-silence` | release and alignment guard | no |

The model-fit payload is 6 seconds.

## Signal Construction

- `coded-sync` is a deterministic 31-chip, band-limited multi-carrier sequence.
- `level-ladder` uses `-24, -18, -12, -8, -4, 0 dBr` relative to the calibrated ceiling.
- `dual-level-ess` uses approximately 35 Hz to 12 kHz synchronized exponential sweeps at `-12` and `0 dBr`.
- `odd-grid-multisine` excites odd 20 Hz-grid lines up to 12 kHz, leaves one odd detection line empty in each four-line group, and uses frequency weighting for approximately equal energy per octave.
- `transient-memory`, `guitar-program`, and `reference-a` are generated from deterministic band-limited harmonic plucks and short pick transients.
- Every generated segment is peak-limited and the complete output receives a final ceiling guard.

## Capture And Extraction Contract

- Amp Gain 100 uses `HansoProbeA1Full`.
- Amp Gain 50 and Gain 10 use `HansoProbeA1Delta`.
- Full/Delta selection comes from each recipe anchor's explicit `probeVariant`; normalized gain values do not select a variant implicitly.
- Cabinet position captures continue to use the existing 5-second `LogSineSweep` until dedicated cabinet ESS deconvolution is implemented.
- `Analysis` and `Training` segments are concatenated into each anchor's existing `dry-reference` and `aligned-captured` chunks.
- Validation segments are stored separately and are never included in compact model fitting.
- If a capture ends inside model-fit data, dry and return fitting chunks are cropped to the same available segment timeline and a re-capture warning is logged.
- Full fidelity adopts only a complete `reference-a-end`; it does not silently fall back to the start reference.
- Imported preview samples remain optional, play after the primary probe, and continue to participate in fidelity refinement.

Current compact extraction consumes the richer model-fit waveform through the existing transfer-curve and spectrum fitting path. Explicit ESS deconvolution, harmonic-kernel extraction, odd/even detection-bin analysis, and adaptive targeted extension are later analysis stages; the A1 waveform and segment contract are designed to support them without changing the emitted signal.
