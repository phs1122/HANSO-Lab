# HANSO Container Reference for HANSO Lab

이 문서는 HANSO Lab이 `.hanso` 포맷을 어떻게 생산하는지 기억하기 위한 로컬 진입점이다.
공식 source of truth는 항상 [`../../hanso-dsp/docs/HANSO_CONTAINER_FORMAT.md`](../../hanso-dsp/docs/HANSO_CONTAINER_FORMAT.md)이다.
컨테이너 레이아웃, metadata root, chunk convention, `metadata.dspCore`, `captureModel`, `circuitModel`, `parameterMap`, version 정책은 공식 명세를 기준으로 판단한다.

## HANSO Lab 책임 범위

HANSO Lab은 캡쳐 기반 `.hanso` producer이다. 주요 책임은 다음 영역이다.

- 앰프/캐비넷/향후 페달 캡쳐 workflow
- `metadata.dspCore.captureModel`
- 캡쳐/정렬 audio chunks
- `cabProfile`
- `captureWorkflow`
- `captureFidelity`
- `captureData`
- `modelData`
- analysis metadata
- capture asset의 `parameterMap`
- CaptureFirstHybrid 경로

Lab에서 만든 앰프 캡쳐 기반 `.hanso`는 캡쳐된 실제 장비 소리를 중심으로 동작해야 한다. 회로 정보가 함께 들어가더라도 캡쳐 소리를 임의로 덮어쓰지 않는다.

## 포맷 변경으로 간주하는 영역

아래 파일이나 metadata/chunk 표면을 바꾸면 `.hanso` 포맷 영향 여부를 반드시 확인한다.

- `Source/Serialization/HansoSerializer.*`
- `Source/Model/HansoPackage.*`
- `captureWorkflow`
- `cabProfile`
- `captureData`
- `captureFidelity`
- `modelData`
- `metadata.dspCore.captureModel`
- `metadata.dspCore.hybridBinding`
- `metadata.dspCore.parameterMap`
- chunk id
- chunk role
- chunk `mediaType`
- chunk encoding
- public parameter id
- `assetType` / `category`
- container `formatVersion`
- `dspCore.formatVersion`

가능한 변경은 additive metadata로 처리한다. public parameter id, chunk id, asset type, category, 또는 기존 field semantics를 바꿀 때는 HANSO Amp Forge와 hanso-dsp loader 호환성을 먼저 확인한다.

## 변경 시 같이 갱신할 것

`.hanso` 생산 코드나 포맷 관련 metadata를 바꿀 때는 세 문서를 함께 확인한다.

1. 공식 명세: `../../hanso-dsp/docs/HANSO_CONTAINER_FORMAT.md`
2. Lab 로컬 참조: `docs/HANSO_CONTAINER_REFERENCE.md`
3. 상대 프로젝트 호환 지점: Amp Forge의 `metadata.dspCore.circuitModel`, `hybridBinding`, `parameterMap` 사용성

포맷 영향이 있으면 같은 변경에서 다음을 갱신한다.

- 공식 명세
- Lab 로컬 참조 문서
- 관련 테스트/fixture
- hanso-dsp loader fixture 또는 round-trip 테스트가 필요한 경우 해당 자산

변경 후에는 워크스페이스 루트에서 다음을 실행한다.

```sh
scripts/check-hanso-container-spec.sh
```

Lab producer 변경이면 관련 Lab regression test도 실행한다.

```sh
cmake --build build --target HANSO_Lab_Tests
"./build/HANSO_Lab_Tests_artefacts/Debug/HANSO_Lab_Tests"
```

hanso-dsp loader가 새 `.hanso`를 읽을 수 있는지, Amp Forge가 사용할 `metadata.dspCore` 호환성이 깨지지 않았는지도 확인한다.

## `cabProfile` 마이크 메타데이터와 micMatrix (additive)

Cabinet 패키지의 `cabProfile`은 다음 additive 필드를 가질 수 있다. 상세 스키마는 공식 명세 7.2를 따른다.

- `positions[].micClass` — real source slot의 마이크 클래스 (`dynamic` / `ribbon` / `condenser`, 미상이면 void)
- `positions[].micModel` — 자유 텍스트 마이크 모델명 (미입력 시 void)
- `micMatrix` — real source slot의 알려진 마이크/포지션 색채를 디임베딩하여 모든 (마이크 클래스 × 포지션) 조합의 tone profile을 재합성한 매트릭스. 포지션은 `Cone` / `Cone Edge` / `Edge` / `Off-Axis`이며, `source: "measured"` 항목은 실측 slot의 profile을 그대로 쓴다. 소비자는 이 필드를 optional로 취급해야 한다.

색채 커브 정본은 `Source/Analysis/MicColorationProfiles.*`이며, HANSO TONE의 `hst_fx/CabinetProfiles.h` 파라메트릭 정의를 미러링한다. 어느 한쪽 커브를 바꾸면 같은 변경에서 다른 쪽도 갱신한다 (통합 단계에서 공유 모듈로 단일화 예정).

## `modelData.chainCoverage` (additive)

캡쳐가 체인의 어느 스테이지를 담고 있는지 명시하는 optional 문자열 배열이다. 공식 명세 7.3을 따른다. FullRig=`["amp","cabinet"]`, Amp=`["amp"]`, PreAmp=`["preamp"]`, Pedal/Effect=`["pedal"]`, Cabinet=`["cabinet"]`. 소비자는 이 필드로 무엇을 보완해 렌더링할지 결정하고, 없으면 `deviceType`/`category`로 폴백한다. Tone Preview의 보완 체인 정책(`Source/Preview/PreviewChainPolicy.h`)과 의미가 일치해야 한다.

## HANSO Probe A1 capture artifacts (additive)

Amp Gain 및 정적 Pedal Drive anchor workflow는 `metadata.captureSettings.testSignalType`에 `HansoProbeA1`을 기록한다. `captureWorkflow.captureRecipe.anchors[]`의 `probeVariant`와 `testSignalType`은 anchor별 `HansoProbeA1Full` 또는 `HansoProbeA1Delta`, `testSignalDurationSeconds`는 각각 24.5 또는 10.0을 기록한다. 파형과 segment 역할의 정본은 `docs/HANSO_PROBE_A1.md`이다.

Anchor별 optional probe validation chunks:

- `capture/gain-<anchor>/probe-reference-dry.pcm16` — held-out dry reference, role `probeValidationDry`
- `capture/gain-<anchor>/probe-reference-a-start.pcm16` — Full 시작 실측 reference, role `probeValidationReal`
- `capture/gain-<anchor>/probe-reference-a-end.pcm16` — Full 종료 실측 reference, role `probeValidationReal`
- `capture/gain-<anchor>/probe-reference-a.pcm16` — Delta 실측 reference, role `probeValidationReal`
- `capture/gain-<anchor>/sample-hanso-probe-reference.pcm16` — fidelity evaluator가 소비하는 held-out 실측 reference, role `ampProcessedSample`

Full의 시작/종료 실측 reference가 모두 있으면 `captureFidelity.anchors[].repeatabilityEsrDb`를 기록할 수 있다. 이 값은 모델 출력 ESR이 아니라 동일한 dry reference에 대한 장비의 시작/종료 repeatability ESR이다. 알 수 없는 소비자는 이 metadata와 chunks를 무시할 수 있으며 기존 compact model chunk의 runtime semantics는 변하지 않는다.

## Cabinet Probe C1 direct IR capture

Cabinet 위치 직접 캡쳐는 `metadata.captureSettings.testSignalType`과 해당 position anchor의
`probeVariant`/`testSignalType`에 `CabinetProbeC1`, `testSignalDurationSeconds`에 `8.0`을 기록한다.
신호는 6초 synchronized ESS와 2초 decay tail로 구성되며 정본은
`docs/CABINET_PROBE_C1.md`이다.

`cabinet/positions/<position-id>/ir.pcm16`은 정렬된 sweep return이 아니라 정규화
주파수영역 역필터링으로 추출한 실제 impulse response여야 한다. role은 `cabinet-ir`,
encoding은 PCM16이다. 역필터링에 실패한 slot은 `error` 상태가 되며 real source로
인정하지 않는다.

## CaptureFirstHybrid 원칙

- 캡쳐 데이터가 존재하면 캡쳐된 실제 소리가 우선이다.
- 회로/파라미터는 캡쳐 anchor 보간 또는 제어 레이어 역할을 우선한다.
- CaptureOnly 패키지는 `captureModel`과 캡쳐 chunks만으로 렌더 가능해야 한다.
- CaptureFirstHybrid 패키지는 캡쳐 소리를 권위 있는 기준으로 두고, `circuitModel`이나 `parameterMap`은 anchor 사이를 제어하거나 보정한다.
- 회로 정보가 포함되어도 캡쳐 결과를 무조건 대체하는 CircuitOnly 해석으로 바꾸지 않는다.

## Amp Forge 호환성 주의

Lab producer 변경은 Amp Forge의 회로 기반 producer와 같은 `.hanso` 생태계를 공유한다. 특히 다음 항목을 바꿀 때는 상대 프로젝트 호환성을 먼저 확인한다.

- `metadata.dspCore`
- `metadata.dspCore.parameterMap`
- `metadata.dspCore.hybridBinding`
- public parameter id
- chunk id
- chunk role / `mediaType` / encoding
- `assetType` / `category`
- `dspCore.formatVersion`
- container `formatVersion`

새 production `.hanso`는 binary `HANSO` container를 써야 하고, `metadata.dspCore`를 포함해야 한다. JSON-only `.hanso`는 개발/테스트 shortcut으로만 취급한다.
