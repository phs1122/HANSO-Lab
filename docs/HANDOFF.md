# HANSO Lab 인계 문서 (Codex / Composer / 후속 에이전트용)

최종 갱신: 2026-07-05. 이 문서는 "하이게인 우선 캡쳐 + 샘플 실주입 fidelity" 작업의 완료 상태와
남은 일을 기록한다. 작업 전 반드시: `git status --short`, `git log --oneline -5`, [AGENTS.md](../AGENTS.md) 확인.

## 빌드 / 테스트

```bash
cd "HANSO Lab"
cmake --build build                                        # 앱
cmake --build build --target HANSO_Lab_Tests               # 테스트
"./build/HANSO_Lab_Tests_artefacts/Debug/HANSO_Lab_Tests"  # 46 checks, 전부 PASS 상태가 기준선
```

## 완료된 것 (이 세션, 미커밋 워킹트리)

### 1. 캡쳐 순서: 하이게인 우선
- 순서: Setup → **Calibration(gain 100%)** → gain-100 → gain-050 → gain-010 → Finish.
- 근거: 최대 스트레스 상태에서 레벨/노이즈를 먼저 검증하면 이후 앵커에서 클리핑 서프라이즈가 없다.
- 코드: `CaptureRecipe.cpp`(순서·안내문), `CaptureEngine.cpp`의 `unlockPostCalibrationSteps()`(gain-100 우선)와
  `refresh()` 잠금해제 체인(100→050→010→final). `qualityTargetForStep`은 normalizedValue 기반이라 무수정.
- gain-100 앵커 SNR 경고 문턱 30→**20dB** (`qualityTargetForStep`의 `minimumSnrDb`). 하이게인 유휴 노이즈는 장비 고유 특성.

### 2. 뮤트-드롭 정체성 검사 (왜곡-강건 캘리브레이션)
- 문제: 하이게인 왜곡이 MultiSine 프로브 에너지를 배음/IMD로 퍼뜨려 Goertzel tone-dominance가 정직한 신호를 거부.
- 해법: Goertzel 실패가 지속되면 ~2.5초 주기로 **300ms 출력 뮤트** → 리턴이 6dB 이상 하강하면 "내 신호"로 인정.
- 코드: `CalibrationValidator::checkMuteDropIdentity()`(순수 함수), `validateProbe(..., bool muteDropIdentityConfirmed=false)`
  꼬리 기본값 파라미터(기존 호출부 무수정), 결과에 `muteDropRescuedIdentity`.
  스케줄링은 `CaptureEngine::updateLiveCalibration()`(UI 타이머). 뮤트 창 동안 safe tick **동결**(리셋 아님),
  해제 후 100ms 정착. 확인 유효기간 15초. `CaptureAudioSource::setOutputForceMuted()`
  (atomic, 오디오 콜백에서 출력만 clear — 녹음/플레이헤드 유지, realtime-safe).
- 상수: `CaptureEngine.cpp` 상단 `calibrationMute*` 4종.

### 3. 샘플 실주입 캡쳐 (composite buffer)
- gain 앵커 캡쳐 시 출력 = `스윕(5s) + 0.5s 무음 + 샘플1 + 0.5s 무음 + 샘플2 + 0.5s 꼬리`.
- 샘플: `PreviewSampleLibrary` 파일 알파벳순 최대 2개, 최대 6초, 모노 합산, 세션 SR로 리샘플
  (`resampleMonoToRate`, Lagrange — 보호 대상인 `TonePreviewPanel::resampleLinear`와 별개 구현),
  피크를 스윕 진폭(캘리브레이션 파생)에 정규화 — **절대 스윕보다 크지 않음**.
- 라이브러리가 비어 있으면 스윕 단독 + 로그 안내(기존 흐름과 동일).
- chunk 저장:
  - dry 스윕: `capture/gain-XXX/dry-reference.pcm16` (**스윕만** — 모델 추출 입력 오염 방지)
  - dry 샘플(1회): `capture/shared/sample-<id>.pcm16`
  - 실녹음(정렬 후 분할): `capture/gain-XXX/sample-<id>.pcm16`
  - id 규칙: `sanitizePreviewSampleId()` in `CaptureStepUtils.h` (소문자, 비영숫자→'-') — 캡쳐/프리뷰 공용.
- 분할: 정렬은 기존 단일 오프셋(SignalAlignment) 그대로, `CaptureEngine.h`의 `activeSampleSegments`/`activeSweepSamples`
  레이아웃으로 `refresh()`에서 슬라이스. 정렬 후 길이 부족 시 해당 샘플 스킵+경고.
- 품질 검사는 composite 전체에 그대로 적용(계획은 스윕 구간 한정이었으나 RMS 희석이 1dB 미만이라 단순화 — 의도된 편차).

### 4. Fidelity 평가 + ESR 보정
- `Source/Analysis/ModelFidelityEvaluator.h/.cpp` (신규):
  - `renderAnchor()`: 단일 anchor 모델을 **전용 PreviewModelProcessor 인스턴스**로 오프라인 렌더(공개 API 무변경, 실시간 인스턴스와 비공유).
  - `esrDb()`: **strict ESR**(게인 매칭 없음 — outputGain 차원 유지 목적), `evaluate()`: ESR + 3밴드 오차 + 크레스트 차.
  - `refineAnchor()`: 좌표하강 2패스 — drive ±4(0.25), outputGainDb ±3(0.5), 3밴드 ±3(0.5). 개선 없으면 스윕 피팅 유지.
- 실행 지점: `LabWorkflow::extractCompactModel()` 성공 후 `runFidelityEvaluation()` —
  패키지에서 `capture/shared/sample-*` 스캔 → anchor별(prefix는 anchor.sourceChunkId에서 유도) dry/real 쌍 수집 →
  보정 → 보정 시 compact model chunk 재인코딩 → manifest `captureFidelity` 기록 + 로그 요약.
- manifest 구조: `captureFidelity: { metric: "esr-strict", anchors: [ { parameterValue, esrBeforeDb, esrAfterDb, corrected, adoptedSource, samples: [ { sampleId, esrDb, low/mid/highBandErrorDb, crestFactorDeltaDb } ] } ] }`
  (`HansoPackage.captureFidelity` raw var — captureWorkflow/cabProfile와 같은 패턴).

### 5. Tone Preview A/B ("Real" 토글)
- `TonePreviewPanel`: `realCaptureButton` — ON이면 gain 슬라이더에 가장 가까운 앵커(10/50/100)의
  `capture/gain-XXX/sample-<선택샘플id>.pcm16` 실녹음을 재생. 재생 전 `clearPreviewModel()`로 모델 우회
  (녹음은 이미 앰프를 거쳤으므로 이중 앰핑 방지 — 모델 없으면 preview 경로는 passthrough임을 확인함).
  OFF로 돌아오면 `loadModelFromPackage()`로 모델 복원. 해당 chunk 없으면 토글 비활성/모델 폴백.
- 보호 대상 preview API는 기존 public 함수 호출만 사용, 무수정.

### 테스트 (TestsMain.cpp, 46 checks)
신규: 뮤트-드롭 판정 3종 + off-band 신호 구제/노이즈 비구제, SNR 게이트 우회 불가,
ESR 단위 2종, 보정 ground-truth 회복 2종, 앵커 순서/캘리브레이션 안내문 2종.

## 남은 일 (우선순위)

1. **실장비 검증** (필수, 사람 필요): Kemper 체인에서 새 순서로 전체 캡쳐 —
   ① Calibration(gain 100)이 뮤트-드롭 경로로 통과하는지 ② 로그에 anchor별 ESR 출력 확인
   ③ Real/Model A/B 청취 ④ 보정 후 gain 100% 왜곡 체감 개선 여부. 주의: 이전 캡쳐 데이터는 발진 오염본 → 폐기.
2. **AnalysisPanel fidelity 라벨**: 현재 ESR은 로그+manifest에만 표시. `AnalysisPanel`에 요약 라벨 1개 추가(소규모).
3. **Real 모드 UX**: 토글 상태에서 gain 슬라이더 변경 시 재생 중 소스 전환 없음(다음 Play에 반영). 필요 시 개선.
4. 로드맵(변경 없음): ESS deconvolution → 레벨 프로브(transient 실측) → Wiener–Hammerstein → neural residual.
   Cabinet의 Compact EQ 프리뷰 모드, Unity 소비자 연동은 별도 단계.

## 보호 영역 (동작 변경 금지)

calibration dBFS 수치(입력 −36~−8 / 출력 −42~−24 / 슬라이더 −50~−18, `CaptureEngine.cpp` 상단 상수),
preview gain API split, `PreviewCabinetProcessor`, waveform seek, Lagrange resampling(`TonePreviewPanel::resampleLinear`),
cabinet `.hanso` IR preview, serializer의 `cabProfile`/`assetType` import. **git commit/push/branch는 사용자 지시 시에만.**

## 변경 파일 목록 (이 세션)

수정: CMakeLists.txt, Analysis/CalibrationValidator.{h,cpp}, App/LabWorkflow.{h,cpp},
Audio/CaptureAudioSource.{h,cpp}, Capture/CaptureEngine.{h,cpp}, Capture/CaptureRecipe.cpp,
Capture/CaptureStepUtils.h, Model/HansoPackage.{h,cpp}, Tests/TestsMain.cpp, UI/TonePreviewPanel.{h,cpp}
신규: Analysis/ModelFidelityEvaluator.{h,cpp}
