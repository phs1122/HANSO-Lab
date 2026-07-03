# HANSO Lab

HANSO Lab은 기타 앰프·캐비넷 등 하드웨어의 사운드를 캡쳐하고, 분석하고, HANSO 생태계에서 사용하는 `.hanso` 패키지로 export하는 JUCE 기반 standalone 데스크탑 앱이다.

HANSO Lab is a standalone JUCE desktop application that captures, analyses, and exports guitar hardware models into `.hanso` packages. This README is written primarily in Korean.

HANSO 생태계에서의 위치:

- **HANSO Engine** — 실시간 하이브리드 기타 DSP 런타임.
- **HANSO Lab (이 앱)** — 캡쳐 / 분석 / 모델 authoring / `.hanso` export 도구. 실시간 FX 플러그인이 아니다.
- **`.hanso`** — Amp, Cabinet, Pedal, FullRig 등 여러 asset type을 담는 universal 패키지 포맷.
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

- **간편 캡쳐 (Easy)**: 테스트 신호가 Left 채널로만 출력되고(Right 뮤트) 출력 진폭이 안전 정책으로 제한된다. 처음이라면 이 모드를 쓰라.
- **정식 캡쳐 (Standard)**: 스테레오 출력, 고정 진폭.

### 캡쳐 타입

`Amp` 또는 `Cabinet`을 선택하면 해당 워크플로우 레시피가 로드된다. (Effector/Pedal은 미구현.)

---

## 3. Amp 캡쳐 워크플로우

```
Setup Confirmed → Calibration → Gain 10% → Gain 50% → Gain 100% → Finish Capture
```

1. **Setup** — 앰프의 Bass / Mid / Treble / Presence를 12시 방향에 고정한다. 워크플로우 중에는 Gain 노브만 바꾼다.
2. **Calibration** — Gain을 50%에 두고 Calibration Start. 리턴 입력이 −36 ~ −8 dBFS 범위에 3초 이상 머물면 자동 완료된다.
3. **Gain anchor 캡쳐 (10% / 50% / 100%)** — 각 단계에서 Gain 노브를 지시된 위치로 맞추고 Start Capture. 앱이 로그 사인 스윕(클릭 방지 페이드 처리)을 보내고 리턴을 녹음한다.
4. 각 캡쳐 직후 **품질 검사**가 자동 실행된다: 신호 유무, 클리핑, 권장 RMS 범위, 녹음 길이, 레이턴시, **정렬 신뢰도**(리턴이 실제 테스트 신호의 응답인지 — 잘못된 입력 채널 감지), **노이즈 플로어/SNR**(레이턴시 이전 무신호 구간에서 실측). 문제가 있으면 한/영 안내에 따라 해당 단계만 다시 캡쳐하면 된다.
5. **Finish Capture** — 분석과 compact model 추출을 실행한다. 이 단계에서는 장비로 신호를 보내지 않는다.
6. **Export** — 메타데이터 입력 후 `Let's HANSO!`로 `.hanso` 파일 저장.

### 분석이 실제로 측정하는 것

- **Drive (왜곡량)** — dry/captured 진폭의 분위수 매핑(quantile–quantile transfer curve)에 tanh를 피팅해 얻는다. **노브 위치가 아니라 캡쳐된 소리에서 측정된다.** 클린 앰프는 낮은 drive로, 하이게인 앰프는 높은 drive로 피팅된다.
- **Output gain** — dry 대비 captured RMS 차이.
- **Tone (low/mid/high)** — 캡쳐 전 구간의 프레임 평균 파워 스펙트럼으로 계산한 밴드 델타(평균 레벨 성분 제거).
- **Anchor 일관성** — gain 단계 간 레벨이 역전되면 경고한다(노브를 잘못 맞춘 경우 감지).
- **측정하지 않는 것** — envelope attack/release 등 transient 특성. 스윕으로는 측정 불가능하므로 manifest에 `transientMeasured: false`로 기록된다. 전용 프로브는 로드맵 참조.

---

## 4. Cabinet 캡쳐 워크플로우

```
Setup Confirmed → Calibration → Center → Edge → Cone → Off-Axis → Finish / Build Cabinet
```

캐비넷 체인: 앱 출력 → 파워앰프/IR 소스 → 캐비넷+마이크(또는 로드박스) → 인터페이스 입력.

각 마이크 포지션 slot은 세 가지 방법으로 채운다:

| 방법 | slot 상태 | 설명 |
|---|---|---|
| 직접 캡쳐 | `captured-ir` | 스윕을 보내 IR 응답을 녹음 |
| 외부 WAV import | `imported-ir` | IR 파일이 `.hanso` 내부 chunk로 복사 저장 (외부 경로 참조 없음) |
| 자동 추정 | `estimated-compact-cab` | Finish 단계에서 real source들로부터 보간 |

**Export 규칙: 최소 1개 slot은 real source(`captured-ir` 또는 `imported-ir`)여야 한다.** 추정만으로 채운 캐비넷은 export할 수 없다.

### Finish / Build Cabinet에서 일어나는 일

1. 모든 real source slot의 IR에서 **compact tone profile**을 추출한다 — 6개 밴드(80–160 / 160–350 / 350–800 / 800–1800 / 1800–4000 / 4000–9000 Hz)의 상대 게인 + 레벨.
2. 비어 있는 slot은 마이크 포지션 축에서 인접 real source의 tone profile을 **보간**해 채운다. 추정 slot은 `estimated: true`로 표시되며, 라벨만이 아니라 **실제 렌더링 가능한 EQ 데이터**를 갖는다.
3. manifest의 `interpolationPlan.status`가 `computed`로 갱신된다.

이 tone profile이 경량 소비자(게임 등)가 사용하는 기본 페이로드다. IR convolution이 가능한 소비자는 real slot의 IR chunk를 그대로 쓰면 된다.

---

## 5. Tone Preview

Tone Preview는 **샘플 기반** 프리뷰다. 피드백 루프 위험 때문에 라이브 입력 프리뷰는 지원하지 않는다.

```
샘플 재생 → Compact HANSO Model → (Cabinet Preview Filter) → (Normalization) → 볼륨 → 출력
```

- Amp 패키지: Gain 파라미터로 anchor 사이를 보간하며 audition한다.
- Cabinet `.hanso`도 열 수 있다: IR 기반으로 재생되며 Mic Position 컨트롤로 전환된다. 추정 slot은 IR이 없으므로 현재 IR 프리뷰 대상이 아니다(tone profile 기반 EQ 프리뷰는 로드맵 참조).

---

## 6. `.hanso` 패키지 포맷

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

Guided 캡쳐 오디오 chunk (PCM16, dry는 anchor 간 공유):

```
capture/shared/dry-reference.pcm16
capture/gain-010/aligned-captured.pcm16
capture/gain-050/aligned-captured.pcm16
capture/gain-100/aligned-captured.pcm16
cabinet/positions/<slot>/ir.pcm16
model/compact.hanso-model
```

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

1. **ESS deconvolution (Farina)** — 이미 사용 중인 로그 스윕에서 선형 IR과 배음별 IR을 분리. EQ 추정의 왜곡 배음 바이어스를 제거하고 H2/H3 실측치를 얻는다.
2. **레벨 프로브 신호** — 다단계 진폭 버스트로 compression attack/release 실측 → `transientMeasured: true` 달성.
3. **Wiener–Hammerstein 구조** — pre-filter → waveshaper → post-filter. circuit-inspired DSP의 최소 단위.
4. **Neural residual correction** — residual dataset은 dry/aligned chunk에서 학습 시점에 재생성.
5. **Tone Preview의 Compact EQ 모드** — 추정 slot과 경량 소비자 경로를 Lab 안에서 audition.
6. Effector(Pedal) 캡쳐 워크플로우.

## 9. 개발 시 주의 (Protected areas)

작업 전 확인: `git status --short`, `git log --oneline -5`.

보호 대상(동작 변경 금지): calibration dBFS 정책, preview gain API split, `PreviewCabinetProcessor`, waveform seek, Lagrange resampling, cabinet `.hanso` IR preview, serializer의 `cabProfile`/`assetType` import.
