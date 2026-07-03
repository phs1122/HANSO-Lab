# HANSO Lab Agent Guide

이 문서는 HANSO Lab 프로젝트를 다루는 AI 에이전트와 개발자가 따라야 할 기준을 정리한다.

## 응답 언어

- 사용자가 영어 프롬프트를 제공하더라도, 사용자에게 제공하는 답변은 한국어로 작성한다.
- 코드, 식별자, 로그, 파일명, UI 텍스트 중 기존 영어 관성이 있는 항목은 필요한 경우 영어를 유지한다.

## 프로젝트 정체성

HANSO Lab은 JUCE 기반 standalone desktop application이다.

목적:
- 기타 관련 하드웨어/모델러 사운드 캡쳐
- 분석 및 모델 추출
- Tone Preview
- `.hanso` 패키지 export

HANSO 생태계:
- HANSO Engine: realtime guitar/audio DSP core
- HANSO Lab: capture, analysis, authoring, export tool
- `.hanso`: universal HANSO asset package format
- FX Plugin: 추후 `.hanso`를 로드할 별도 realtime plugin

중요:
- `.hamp`를 사용하지 않는다.
- 패키지 확장자는 항상 `.hanso`이다.
- `.hanso`는 amp-only 포맷이 아니며 Amp, Pedal, Cabinet, Speaker, Pickup, Rig, Utility 등 여러 asset type을 담을 수 있다.

## 현재 개발 방향

HANSO Lab은 NAM clone 또는 pure black-box neural amp modeller가 아니다.

장기 모델링 철학:
- circuit-inspired DSP
- dynamic behaviour analysis
- neural residual correction
- parameter-aware model packaging
- realtime playback compatibility

Neural model은 전체 제품이 아니라 correction layer 중 하나로 취급한다.

## 프로젝트 분리 원칙

- HANSO Lab은 기존 guitar FX plugin과 별도 프로젝트이다.
- FX plugin의 AmpFx, FxRuntime, RigSignalChain, cabinet stage, preset system을 수정하지 않는다.
- FX plugin과 `.hanso` 로딩 통합은 별도 단계에서 진행한다.

## 기술 스택

- C++20
- JUCE
- CMake
- JUCE AudioDeviceManager
- JSON metadata serialization
- Binary chunks for audio/model data

Phase 1~3A에서는 불필요한 대형 의존성을 추가하지 않는다.
- LibTorch 추가 금지
- ONNX Runtime 추가 금지
- GPU training 구현 금지

추후 붙일 수 있도록 인터페이스만 열어둔다.

## 빌드

일반 빌드:

```sh
cd "/Users/hanseo/Desktop/기타리스트 키우기/HANSO Engine/HANSO Lab"
cmake --build build
```

오래된 object/link 문제가 의심될 때:

```sh
cd "/Users/hanseo/Desktop/기타리스트 키우기/HANSO Engine/HANSO Lab"
cmake --build build --clean-first
```

앱 경로:

```text
/Users/hanseo/Desktop/기타리스트 키우기/HANSO Engine/HANSO Lab/build/HANSO_Lab_artefacts/Debug/HANSO Lab.app
```

JUCE `Font` deprecated warning은 현재 무시 가능하다.

## 작업 경로

Codex 작업용 경로:

```text
/Users/hanseo/Documents/한소랩/HANSO Lab
```

실제 사용 프로젝트 경로:

```text
/Users/hanseo/Desktop/기타리스트 키우기/HANSO Engine/HANSO Lab
```

Codex가 sandbox 안에서 먼저 수정한 뒤 실제 프로젝트에 반영해야 할 때는 `build`, `build-local`, `.git`을 제외하고 동기화한다.

## 아키텍처 개요

주요 디렉터리:

```text
Source/App
Source/Audio
Source/Capture
Source/Analysis
Source/Model
Source/Preview
Source/Serialization
Source/UI
```

중요 클래스:
- `ApplicationState`: 현재 package, wizard, export path, unsaved state 관리
- `CaptureWizardState`: capture workflow, step 상태, cabinet slot 상태 관리
- `CaptureEngine`: 캡쳐 실행, calibration, IR import, preview model 연결
- `CaptureAudioSource`: audio callback, test signal, capture, preview playback 처리
- `LabWorkflow`: analysis/model extraction orchestration
- `AssetPanel`: `.hanso` export metadata dialog
- `TonePreviewPanel`: sample-based preview UI

## Capture Type

메인 Capture UI는 Capture Type selector를 기준으로 workflow를 전환한다.

현재 활성 타입:
- Amp
- Cabinet

향후 확장 예정:
- Pedal
- Effect
- FullRig

### Amp Workflow

Amp 선택 시 기존 Liquid Gain workflow를 유지한다.

단계:
- Setup Confirmed
- Calibration
- Gain 10% Capture
- Gain 50% Capture
- Gain 100% Capture
- Finish Capture

Finish Capture는 analysis, validation, compact model extraction을 실행한다.

### Cabinet Workflow

Cabinet 선택 시 mic-position slot workflow를 사용한다.

단계:
- Setup Confirmed
- Calibration
- Center
- Edge
- Cone
- Off-Axis
- Finish / Build Cabinet

Cabinet slot 상태:
- `empty`
- `capturing`
- `captured-ir`
- `imported-ir`
- `estimated-compact-cab`
- `error`

Cabinet slot 규칙:
- 각 slot은 직접 캡쳐 가능하다.
- 각 slot은 외부 WAV IR import 가능하다.
- 비워둔 slot은 Finish / Build Cabinet 단계에서 estimated slot으로 채울 수 있다.
- 단, 최소 1개 이상의 real source가 있어야 export 가능하다.
- real source는 `captured-ir` 또는 `imported-ir`이다.
- estimated-only cabinet package는 export를 허용하지 않는다.

Cabinet export metadata:
- category: `Cabinet`
- modelData.primaryModelType: `cabinet-mic-position-v1`
- modelData.algorithm: `hanso.cabinet.mic-position.v1`
- modelData.micPositionControlAvailable: `true`
- cabProfile.positions에 position별 source/chunk/source file/estimatedFrom을 기록한다.

주의:
- 현재 Cabinet 직접 캡쳐는 정식 deconvolution IR 생성까지 완성된 단계가 아니다.
- 현재는 캡쳐/정렬된 응답 또는 imported IR을 cabinet position source chunk로 패키징하는 골격이다.
- 진짜 IR deconvolution, mic-position interpolation DSP, Tone Preview에서 cabinet `.hanso`를 cabinet source로 쓰는 기능은 다음 단계 작업이다.

## Tone Preview

Tone Preview는 실시간 오디오 입력을 프리뷰하지 않는다.

이유:
- 간편캡쳐 사용자의 장비환경에서는 실시간 입력 프리뷰가 output-input feedback loop를 만들 수 있다.

현재 방식:
- Preview Samples 폴더의 WAV 샘플을 재생한다.
- model이 없으면 clean sample을 재생한다.
- Finish Capture 후 current session model이 preview tone으로 올라간다.
- Open HANSO로 기존 `.hanso` model preview 가능하다.

Preview monitoring chain:

```text
Sample Playback
→ Compact HANSO Model processing
→ Default Cabinet Preview Filter, if enabled
→ Preview Normalization, if enabled
→ User Output Volume
→ Audio Output
```

Preview 전용 기능:
- Loop
- Volume
- Normalization
- Cabinet preview filter

주의:
- Preview monitoring 설정은 `.hanso` model/export data에 영구 반영하지 않는다.
- Cab 버튼은 실제 cabinet model이 아니라 preview-only cabinet-like EQ/filter이다.

## Realtime Safety

Audio callback에서는 다음을 피한다.
- heap allocation
- file IO
- lock
- 긴 blocking 작업

UI thread와 audio thread 사이 값 전달은 atomic, preallocated buffer, lock-free 또는 안전한 구조를 사용한다.

Analysis, model extraction, neural/residual training은 offline/background 작업으로 둔다.

## Export Policy

- 모든 HANSO asset은 `.hanso` 확장자를 사용한다.
- package category는 Capture Type과 일치해야 한다.
- Amp workflow에서는 기존 amp export schema를 깨지 않는다.
- Cabinet workflow에서는 Cabinet 전용 metadata와 `cabProfile`을 포함한다.
- Export 버튼은 package metadata dialog를 열고, 최종 저장 버튼은 `Let's HANSO!`로 유지한다.

## UI/UX 원칙

- 사용자가 왜 막혔는지 알 수 있어야 한다.
- 단순히 "실패"라고 표시하지 말고 가능한 조치나 조건을 함께 제공한다.
- Spinning wait cursor 대신 작업 중 overlay/progress UI를 우선한다.
- 불필요한 탭은 줄이고, capture-first workflow를 유지한다.
- Settings는 pop-up menu를 통해 Audio Settings / Log 등으로 진입한다.

## Git

- 사용자가 git 관리는 Cursor에서 한다고 했다.
- Codex는 로컬 파일 작업과 빌드 확인만 수행한다.
- 임의로 commit, push, branch 작업을 하지 않는다.

## AI Workflow Contract

This project uses both Codex and Cursor.

Codex is mainly used for:
- major feature implementation
- scaffolding
- architecture expansion
- integration work

Cursor is mainly used for:
- debugging
- stabilization
- refactoring
- local IDE-based fixes

Therefore, Cursor changes are stabilization changes and must not be regressed.

## Regression Protection

Before starting any task:

1. Check current git status.
2. Check recent commit history.
3. Read `docs/CURSOR_FIXES.md` if it exists.
4. Treat current HEAD as the only valid baseline.
5. Never restore older implementations unless explicitly instructed.

## Protected Code

Do not remove or simplify code related to:

- real-time audio safety
- parameter smoothing
- denormal prevention
- sample rate handling
- block size handling
- bypass behavior
- input/output gain staging
- IR loading and buffering
- NAM model loading/playback boundaries
- calibration
- thread-safe state transfer
- UI state synchronization
- plugin format settings
- build system fixes

If one of these areas must change, preserve the previous behavior unless the user explicitly asks for behavior change.

## File Touch Discipline

When implementing a feature:

- Modify the smallest reasonable set of files.
- Do not edit files unrelated to the task.
- Do not mass-format files.
- Do not rewrite classes just to make them cleaner.
- Do not remove compatibility code unless explicitly requested.
- Do not change plugin identifiers, bundle identifiers, manufacturer codes, or format settings.

## Cursor Fix Preservation Report

Every final response must include:

### Cursor baseline preservation
- Current HEAD treated as baseline: yes/no
- Cursor fix log checked: yes/no/not present
- Recent Cursor-modified files touched: yes/no
- Existing stabilization logic removed: yes/no
- Regression risk: low/medium/high

If the answer is not clearly "yes, preserved", explain why.
