# HANSO Lab

HANSO Lab은 기타 앰프·캐비넷 등 하드웨어의 사운드를 캡쳐하고, 분석하고, HANSO 생태계에서 사용하는 `.hanso` 패키지로 export하는 JUCE 기반 standalone 데스크탑 앱이다.

HANSO Lab is a standalone JUCE desktop application that captures, analyses, and exports guitar hardware models into `.hanso` packages. This README is written primarily in Korean.

HANSO 생태계에서의 위치:

- **HANSO Engine** — 실시간 하이브리드 기타 DSP 런타임.
- **HANSO Lab (이 앱)** — 캡쳐 / 분석 / 모델 authoring / `.hanso` export 도구. 실시간 FX 플러그인이 아니다.
- **`.hanso`** — Amp, Cabinet, Pedal, FullRig 등 여러 asset type을 담는 universal 패키지 포맷.

### 저장 티어 (RAW/JPG 모델 — 정본: `hanso-dsp/docs/HANSO_CONTAINER_FORMAT.md §6.2`)

**UX 원칙: 사용자는 `.hansocap`을 보지 않는다. 사용자 손에 쥐어지는 파일은 `.hanso` 하나로
충분하다.** (`Source/Serialization/DistributionExport.*` 구현)

- **마스터 `.hansocap`** (`tier: "master"`) — 캡쳐 오디오(dry/aligned/probe/sample, float32)
  전부를 담는 원본. `Let's HANSO!` export 시 **앱 관리 폴더
  (`~/Library/Application Support/HANSO Lab/Masters/<captureId>.hansocap`)에 조용히 자동
  보관**된다 — 저장 다이얼로그도, 유저 노출도 없다. 재피팅·재검증·de-embedding의 원천.
  로컬 전용 — 공유·배포 금지 (유저 연주가 담길 수 있어 프라이버시 규칙이기도 함).
- **배포 `.hanso`** (`tier: "distribution"`) — 유저가 export로 받는 유일한 파일. 분석 청크
  제거 + 캐비닛 IR pcm24 재인코딩된 런타임 슬림본. 단독으로 완결(재생·오디션·공유 전부 가능).
  마스터는 오직 "나중에 더 좋은 모델로 재피팅"에만 필요하며, `captureId`로 배포본과 아카이브
  마스터가 연결된다.
- 재생 경로(`DeviceLibrary::resolve`, HANSOTONE)는 마스터를 거부한다. 배포본을 열어
  재export해도 아카이브 마스터를 덮어쓰지 않는다.
- **소비자(consumers)** — FX 플러그인, Unity 기반 리듬게임(가제 '기타리스트 키우기') 등. 캐비넷은 IR convolution 없이 slot별 compact tone profile(EQ 파라미터)만으로도 렌더링할 수 있도록 설계되어 있다.

---

## 1. 빌드 (Build)

요구사항: CMake ≥ 3.22, C++20 컴파일러, JUCE 소스 체크아웃.

```bash
cd "HANSO Lab"
cmake -B build                     # JUCE는 알려진 로컬 경로에서 자동 탐색된다
cmake --build build
open "build/HANSO_Lab_artefacts/Debug/HANSO Lab.app"
```

JUCE가 자동으로 잡히지 않으면:

```bash
cmake -B build -DHANSO_JUCE_SOURCE_DIR=/path/to/JUCE-master
```

### 테스트 (Regression tests)

분석 파이프라인은 합성 신호 기반 회귀 테스트로 검증된다. 오디오 장비 없이 실행 가능하다.

```bash
cmake --build build --target HANSO_Lab_Tests
"./build/HANSO_Lab_Tests_artefacts/Debug/HANSO_Lab_Tests"
```

검증 항목: 전대역 스펙트럼 분석, 정렬(latency) 추정 정확도, tanh drive의 실측 피팅(ground-truth 대비), 클린 장비의 low-drive 판정, 캐비넷 tone profile 추출·보간·직렬화, 캡쳐 품질 검사(무신호/클리핑/정렬 실패). **분석 코드를 수정했다면 반드시 이 테스트를 돌려라.**

---

## 2. 시작하기 (Getting started)

### 오디오 라우팅

Amp 캡쳐 기준 기본 연결:

```
오디오 인터페이스 Output → (리앰프 박스) → 앰프 Input
앰프의 라인/캡쳐 출력(로드박스 등) → 오디오 인터페이스 Input
```

> **경고: 앰프의 Speaker Out을 오디오 인터페이스에 직접 연결하지 마라.** 반드시 로드박스나 라인 레벨 출력을 사용하라.

앱 상단의 설정 버튼에서 오디오 장치와 입출력 채널, 샘플레이트, 버퍼 크기를 선택한다.

- **간편 캡쳐 (Easy)**: 리앰프 박스가 없는 사용자용(Phones Out → 일반 케이블 → 장비 In). 테스트 신호가 Left 채널로만 출력되고(Right 뮤트), 앱 출력이 유일한 게인 레버다. **출력 슬라이더로 직접 조절**하며 상한은 슬라이더 최대치(-3 dBFS, 트루피크 여유)다. 약한 헤드폰 출력은 세게 밀어야 하므로 캡을 두지 않는다.
- **일반 캡쳐 (Standard)**: 리앰프 박스가 있는 사용자용(Line Out → 리앰프 박스 → 장비 In). 스테레오 출력. **출력 슬라이더가 없고 앱 디지털 출력은 -12 dBFS로 고정**된다. 드라이브(앰프에 들어가는 세기)는 리앰프 박스로 조절한다.
- **모드 = 물리적 배선**이므로 calibration 시작 후에는 모드 버튼이 잠긴다. 바꾸려면 `새 캡쳐`로 초기화한다(= calibration 무효화).
- 두 모드 모두 출력에는 상한선 개념이 없다(캡이 아니라 모드별 고정/슬라이더). 유일한 위험은 리턴 클리핑이므로, 리턴이 −36 ~ −8 dBFS 안에 들도록 오디오 인터페이스 입력 게인을 맞춘다.

### 캡쳐 타입

`Amp`, `Cabinet`, `Pedal / Static Effect`를 선택하면 대상별 워크플로우 레시피가 로드된다. Pedal 캡쳐는 Distortion / Overdrive / Fuzz / Boost / 고정 EQ처럼 정적 비선형 장비만 지원하며 Delay / Reverb / Modulation은 지원하지 않는다.

---

## 3. Amp 캡쳐 워크플로우

```
Setup Confirmed → Calibration → Gain 100% → Gain 50% → Gain 10% → Finish Capture
```

1. **Setup** — 앰프의 Bass / Mid / Treble / Presence를 12시 방향에 고정한다. 워크플로우 중에는 Gain 노브만 바꾼다.
2. **Calibration** — Gain을 100%에 두고 Calibration Start. 최대 게인(최대 스트레스)에서 레벨·노이즈를 먼저 검증해 이후 앵커에서 클리핑 서프라이즈를 없앤다. 리턴 입력이 −36 ~ −8 dBFS 범위에 3초 이상 머물면 자동 완료된다.
3. **Gain anchor 캡쳐 (100% / 50% / 10%)** — 각 단계에서 Gain 노브를 지시된 위치로 맞추고 Start Capture. 앱이 HANSO Probe A1을 보내고 리턴을 녹음한다.

4. 각 캡쳐 직후 **품질 검사**가 자동 실행된다: 신호 유무, 클리핑, 권장 RMS 범위, 녹음 길이, 레이턴시, **정렬 신뢰도**(리턴이 실제 테스트 신호의 응답인지 — 잘못된 입력 채널 감지), **노이즈 플로어/SNR**(레이턴시 이전 무신호 구간에서 실측). 문제가 있으면 한/영 안내에 따라 해당 단계만 다시 캡쳐하면 된다.
5. **Finish Capture** — 분석과 compact model 추출을 실행한다. 이 단계에서는 장비로 신호를 보내지 않는다.
6. **Export** — 메타데이터 입력 후 `Let's HANSO!`로 `.hanso` 파일 저장.

### Pedal / Static Effect 캡쳐

체인은 `앱 출력 → 리앰프 박스(권장) → 페달 Input → 페달 Output → 인터페이스 Instrument/Line Input`이다. Tone/EQ와 Level을 고정하고 Drive 100% / 50% / 10% anchor를 HANSO Probe A1으로 캡쳐한다. Finish 단계에서 compact model을 추출해 Tone Preview의 Pedal 슬롯에 삽입한다.

### Cabinet 직접 캡쳐

각 위치는 그릴 천에서 마이크 캡슐 중심까지 2 cm를 유지한다. Off-Axis는 같은 지점과 거리를 유지한 채 30도로 회전한다. HANSO Cabinet Probe C1(6초 synchronized ESS + 2초 decay tail)을 재생하고, 정규화 역필터링으로 추출한 IR만 Cabinet slot에 저장한다.

### 분석이 실제로 측정하는 것

- **Drive (왜곡량)** — dry/captured 진폭의 분위수 매핑(quantile–quantile transfer curve)에 tanh를 피팅해 얻는다. **노브 위치가 아니라 캡쳐된 소리에서 측정된다.** 클린 앰프는 낮은 drive로, 하이게인 앰프는 높은 drive로 피팅된다.
- **Output gain** — dry 대비 captured RMS 차이.
- **Tone (low/mid/high)** — 캡쳐 전 구간의 프레임 평균 파워 스펙트럼으로 계산한 밴드 델타(평균 레벨 성분 제거).
- **Anchor 일관성** — gain 단계 간 레벨이 역전되면 경고한다(노브를 잘못 맞춘 경우 감지).
- **측정하지 않는 것** — envelope attack/release 등 transient 특성. 스윕으로는 측정 불가능하므로 manifest에 `transientMeasured: false`로 기록된다. 전용 프로브는 로드맵 참조.

---

## 4. Cabinet 캡쳐 워크플로우

```
Setup Confirmed → Calibration → Cone → Cone Edge → Edge → Off-Axis → Finish / Build Cabinet
```

캐비넷 체인: 앱 출력 → 파워앰프/IR 소스 → 캐비넷+마이크(또는 로드박스) → 인터페이스 입력.

직접 캡쳐 시에는 워크플로우 상단의 **캡쳐 마이크** 선택(클래스 + 모델명)이 captured-ir slot에 기록된다. 세션 중 변경하면 이미 캡쳐된 slot에도 소급 적용되며, import slot은 import 시 입력한 자체 메타데이터를 유지한다.

각 마이크 포지션 slot은 세 가지 방법으로 채운다:

| 방법 | slot 상태 | 설명 |
|---|---|---|
| 직접 캡쳐 | `captured-ir` | 스윕을 보내 IR 응답을 녹음 |
| 외부 WAV import | `imported-ir` | IR 파일이 `.hanso` 내부 chunk로 복사 저장 (외부 경로 참조 없음). import 시 마이크 클래스(Dynamic/Ribbon/Condenser)와 모델명을 입력받으며, 모델명만 입력하면 클래스를 자동 추정한다 |
| 자동 추정 | `estimated-compact-cab` | Finish 단계에서 real source들로부터 보간 |

**Export 규칙: 최소 1개 slot은 real source(`captured-ir` 또는 `imported-ir`)여야 한다.** 추정만으로 채운 캐비넷은 export할 수 없다.

### Finish / Build Cabinet에서 일어나는 일

1. 모든 real source slot의 IR에서 **compact tone profile**을 추출한다 — 6개 밴드(80–160 / 160–350 / 350–800 / 800–1800 / 1800–4000 / 4000–9000 Hz)의 상대 게인 + 레벨.
2. 비어 있는 slot은 real source의 마이크/포지션 색채를 디임베딩한 후, 각 목표 포지션(Cone / Cone Edge / Edge / Off-Axis)에 다시 입힌 **계산된 tone profile**로 채운다. 추정 slot은 `estimated: true`로 표시되며, 라벨만이 아니라 **실제 렌더링 가능한 EQ 데이터**를 갖는다.
3. manifest의 `interpolationPlan.status`가 `computed`로 갱신된다.

이 tone profile이 경량 소비자(게임 등)가 사용하는 기본 페이로드다. IR convolution이 가능한 소비자는 real slot의 IR chunk를 그대로 쓰면 된다.

### 마이크 매트릭스 (mic matrix)

Export 시 `cabProfile.micMatrix`에 **모든 (마이크 클래스 × 마이크 위치) 조합의 tone profile 매트릭스**가 추가된다. real source slot의 IR에서 메타데이터로 알려진 마이크/위치 색채를 로그 스펙트럼 도메인에서 제거(디임베딩)해 중립 캐비넷 응답을 얻고, 각 목표 조합의 색채를 다시 입혀 재합성한다. 즉 **마이크 정보가 있는 IR 1개만 있으면 나머지 마이크 종류×위치 조합을 추정**할 수 있다. 실측 조합은 `source: "measured"`로, 추정 조합은 `estimated: true`로 정직하게 표시된다. 마이크 클래스가 미상인 slot은 표준 캡 마이크인 dynamic으로 가정한다. 색채 커브는 HANSO TONE 캐비넷 스테이지의 파라메트릭 정의를 미러링하므로 소비자 렌더링과 일관된다 (`Source/Analysis/MicColorationProfiles.*` 주석 참조).

---

## 5. Tone Preview

Tone Preview는 **샘플 기반** 프리뷰다. 피드백 루프 위험 때문에 라이브 입력 프리뷰는 지원하지 않는다.

```
샘플 재생 → [Pedal 슬롯] → [Amp Head 슬롯] → [Cabinet 슬롯] → (Normalization) → 볼륨 → 출력
```

### 프리뷰 리그 (preview rig)

Tone Preview는 캡쳐 여부와 무관하게 **항상 고정된 풀 시그널 체인**을 표시한다. 각 슬롯의 기본값은 클린 표준 장비다: Pedal은 비움(바이패스), Amp Head는 클린 표준(무착색 unity, 클릭 시 PreAmp+PowerAmp로 확장 — 파워앰프 `.hanso`는 별도 제작 중이라 현재 placeholder), Cabinet은 표준 캡(Standard EQ 또는 Custom `.hanso` IR).

- **Finish Capture 완료 시** 캡쳐 타입에 따라 직전 캡쳐가 해당 슬롯에 자동 삽입된다: Pedal→Pedal 슬롯, Amp→Amp 슬롯, PreAmp→Amp 내 PreAmp 서브슬롯(+클린 파워앰프, 표준 캡 유지), Cabinet→Cabinet 슬롯(IR+micMatrix), Full Rig→Amp+Cab을 묶은 단일 블록.
- 슬롯은 위저드 리셋 후에도 유지되므로 여러 파트를 차례로 캡쳐해 리그를 조립할 수 있고, 각 블록의 **✕ 버튼으로 기본 장비로 되돌릴 수 있다**.
- 체인 스트립에서 실선(초록) 블록은 캡쳐/불러온 패키지, 점선 블록은 클린 표준 장비다.
- **Open HANSO**는 파일 타입과 대상 슬롯을 확인창으로 보여준 뒤 로드하며, 저장되지 않은 세션 캡쳐가 대체될 경우 경고한다.
- Cabinet 슬롯에 패키지가 있으면 **Position 콤보(Cone / Cone Edge / Edge / Off-Axis)**, 그 포지션에서의 **Mic Distance(0.5–30 cm)**, Mic 콤보(micMatrix)가 나타난다. 포지션과 거리는 서로 독립적으로 적용된다. 프리뷰 리그 구성은 `.hanso`에 기록되지 않으며, 패키지는 자신이 담는 스테이지를 `modelData.chainCoverage`로 명시한다.

- Amp 패키지: Gain 파라미터로 anchor 사이를 보간하며 audition한다.
- Cabinet `.hanso`도 열 수 있다: real IR로 재생한 후, 선택한 포지션의 목표 tone profile과 real IR profile의 차이를 6밴드 EQ로 적용한다. 따라서 IR chunk가 없는 추정 slot도 계산된 포지션 성향으로 audition할 수 있다.
- 패키지에 `cabProfile.micMatrix`가 있으면 **Mic 콤보(Original/Dynamic/Ribbon/Condenser)**로 마이크 전환을 프리뷰할 수 있다. Original은 real IR의 마이크 클래스를 유지하며 선택한 포지션의 성향만 적용한다.
- Mic Distance는 프리뷰 전용으로, 2 cm close-mic 기준에서 거리가 늘어날 때의 레벨 감쇠와 고역 공기 손실을 근사한다. 캡쳐된 IR의 실제 마이크 거리를 측정한 것은 아니며, `.hanso`에 저장되지 않는다.

---

## 6. `.hanso` 패키지 포맷

공식 기준 문서: [`../hanso-dsp/docs/HANSO_CONTAINER_FORMAT.md`](../hanso-dsp/docs/HANSO_CONTAINER_FORMAT.md)

이 섹션은 HANSO Lab의 캡쳐 산출물 요약이다. 컨테이너 레이아웃, 메타데이터 루트, `dspCore`, chunk 규칙을 바꾸는 작업은 반드시 공통 명세 문서를 같은 변경에서 수정하고, 루트에서 `scripts/check-hanso-container-spec.sh`를 실행한다.

단일 파일 바이너리 컨테이너:

```text
HANSO
int32 formatVersion
int64 metadataJsonSize
metadata JSON bytes
int32 chunkCount
chunk records...
```

(`HANSO2`는 구버전 읽기 전용 magic으로 허용된다.)

Manifest 주요 필드:

```
packageKind: toneAsset
assetType: amp | cabinet | pedal | full-rig
category, modelData / dspCore
cabProfile          — 캐비넷 slot 목록 + slot별 toneProfile
captureWorkflow     — 단계별 품질 리포트, interpolationPlan
```

Guided 캡쳐 오디오 chunk (float32 마스터/분석 티어, dry는 anchor 간 공유):

```
capture/shared/dry-reference.f32
capture/gain-010/aligned-captured.f32
capture/gain-050/aligned-captured.f32
capture/gain-100/aligned-captured.f32
cabinet/positions/<slot>/ir.f32
model/compact.hanso-model
```

오디오 인코딩 티어(정본: `hanso-dsp/docs/HANSO_CONTAINER_FORMAT.md §6.1`):
현재 모든 캡처 녹음은 모델 피팅/피델리티 평가에 소비되므로 **float32(`.f32`,
`audio/x-hanso-float32`)** 로 저장한다. **pcm24(`.pcm24`)** 재생 티어는 향후
런타임 전용 자산을 위한 것으로 현재 producer는 없다. **pcm16(`.pcm16`)** 은
구자산 읽기 전용 — 신규 producer는 쓰지 않는다. 소비자는 현재 접미사를 먼저
찾고 형제 접미사로 폴백해 구자산을 로드한다.

메타데이터의 측정치는 실측된 것만 값으로 존재한다. 측정되지 않은 항목(예: transient)은 `*Measured: false` 플래그와 함께 비워진다 — 소비자는 이 플래그를 신뢰해도 된다.

---

## 7. 프로젝트 구조

```
Source/
  App/            앱 진입점, 상태, 워크플로우 오케스트레이션
  Audio/          오디오 장치, 캡쳐 소스, 프리뷰 프로세서
  Capture/        워크플로우 상태머신, 레시피, 테스트 신호, 정렬, 안전 정책
  Analysis/       품질 검사, 스펙트럼/전달곡선 분석, 모델 추출, 캐비넷 프로파일러
  Model/          .hanso 데이터 모델 (패키지, compact model, tone profile)
  Serialization/  .hanso 컨테이너, 오디오/모델 chunk 코덱
  Preview/        프리뷰 샘플 라이브러리
  UI/             패널 컴포넌트
  Tests/          합성 신호 회귀 테스트 (HANSO_Lab_Tests 타깃)
```

## 8. 현재 한계와 로드맵

현재 compact model은 `input gain → tanh(drive) → 3-band tone → output gain` 구조의 프리뷰용 근사 모델이다. drive와 tone은 실측 기반이지만 다음이 남아 있다:

1. **비선형 ESS 분리 확장** — Cabinet 선형 IR 역필터링은 구현됨. Amp/Pedal에서 선형 IR과 배음별 IR을 분리해 EQ 추정의 왜곡 배음 바이어스를 제거하고 H2/H3 실측치를 얻는 작업이 남아 있다.
2. **레벨 프로브 신호** — 다단계 진폭 버스트로 compression attack/release 실측 → `transientMeasured: true` 달성.
3. **Wiener–Hammerstein 구조** — pre-filter → waveshaper → post-filter. circuit-inspired DSP의 최소 단위.
4. **Neural residual correction** — residual dataset은 dry/aligned chunk에서 학습 시점에 재생성.
5. **가변 마이크 거리 실측** — 현재 직접 캡쳐 기준은 2 cm로 고정. 추후 실제 거리를 metadata로 기록하고 프리뷰 거리 모델과 연결.
6. **시간 기반 Effect 캡쳐** — Delay / Reverb / Modulation은 정적 Pedal compact model과 분리된 전용 표현이 필요하다.

## 9. 개발 시 주의 (Protected areas)

작업 전 확인: `git status --short`, `git log --oneline -5`.

보호 대상(동작 변경 금지): calibration dBFS 정책, preview gain API split, `PreviewCabinetProcessor`, waveform seek, Lagrange resampling, cabinet `.hanso` IR preview, serializer의 `cabProfile`/`assetType` import.
