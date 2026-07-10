# HANSO Container Reference for HANSO Lab

이 문서는 HANSO Lab이 `.hanso` 포맷을 어떻게 생산하는지 기억하기 위한 로컬 진입점이다.
공식 source of truth는 항상 [`../hanso-dsp/docs/HANSO_CONTAINER_FORMAT.md`](../hanso-dsp/docs/HANSO_CONTAINER_FORMAT.md)이다.
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

1. 공식 명세: `../hanso-dsp/docs/HANSO_CONTAINER_FORMAT.md`
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
