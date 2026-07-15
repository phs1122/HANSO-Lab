#include "Capture/CaptureRecipe.h"

#include "App/Utf8.h"
#include "Capture/CabinetMicPositions.h"

namespace hanso
{
CaptureRecipe CaptureRecipe::createBasicAmpLiquidGain()
{
    CaptureRecipe recipe;
    recipe.recipeId = "amp-liquid-gain-basic";
    recipe.displayName = "Basic Amp Liquid Gain Capture";
    recipe.category = HansoCategory::Amp;

    recipe.fixedControls.push_back({ "Bass", "12 o'clock", 0.5f });
    recipe.fixedControls.push_back({ "Mid", "12 o'clock", 0.5f });
    recipe.fixedControls.push_back({ "Treble", "12 o'clock", 0.5f });
    recipe.fixedControls.push_back({ "Presence / Tone", "12 o'clock", 0.5f });

    recipe.steps.push_back({ "setup", "Setup Confirmed",
                             utf8("앰프의 Bass / Mid / Treble / Presence / Tone 노브를 12시 방향에 맞추세요.\nGain 단계만 안내에 따라 변경합니다.\nMaster Volume은 안전하고 적절한 레벨로 설정하세요."),
                             CaptureStepStatus::Ready, {}, true });
    recipe.steps.push_back({ "calibration", "Calibration Complete",
                             utf8("Gain 노브를 100% 위치에 맞추고, Phones 볼륨을 충분히 낮춘 뒤 Calibration Start를 누르세요.\n최대 게인(최대 스트레스) 상태에서 레벨과 노이즈를 먼저 검증해야 이후 모든 캡쳐 단계가 안전해집니다.\n장비 입력 레벨(기타 레벨)을 먼저 맞추고, 리턴 타겟은 장비의 출력 볼륨으로 조절하세요. 앱 출력을 과도하게 낮춰 입력을 굶기지 마세요.\n앱 출력 미터가 안전 범위에 있고 리턴 입력이 -36 ~ -8 dBFS에 3초 이상 머물면 자동 완료됩니다."),
                             CaptureStepStatus::NotStarted, {}, true });
    recipe.steps.push_back({ "gain-100", "Gain 100% Capture",
                             utf8("Gain 노브를 100% 위치 그대로 두고 캡쳐하세요. 톤 노브는 모두 12시 방향을 유지합니다."),
                             CaptureStepStatus::NotStarted,
                             { "gain", 1.0f, "Gain 100%", CaptureProbeVariant::HansoProbeA1Full }, true });
    recipe.steps.push_back({ "gain-050", "Gain 50% Capture",
                             utf8("Gain 노브를 50% 위치에 맞춘 뒤 캡쳐하세요. 톤 노브는 모두 12시 방향을 유지합니다."),
                             CaptureStepStatus::NotStarted,
                             { "gain", 0.5f, "Gain 50%", CaptureProbeVariant::HansoProbeA1Delta }, true });
    recipe.steps.push_back({ "gain-010", "Gain 10% Capture",
                             utf8("Gain 노브를 10% 위치에 맞춘 뒤 캡쳐하세요. 톤 노브는 모두 12시 방향을 유지합니다."),
                             CaptureStepStatus::NotStarted,
                             { "gain", 0.1f, "Gain 10%", CaptureProbeVariant::HansoProbeA1Delta }, true });
    recipe.steps.push_back({ "final-validation", "Finish Capture",
                             utf8("필수 Gain anchor를 모두 캡쳐했다면 Finish Capture를 눌러 분석과 최종 검증을 실행하세요. 이 단계에서는 장비로 신호를 보내지 않습니다."),
                             CaptureStepStatus::NotStarted, {}, true });

    return recipe;
}

CaptureRecipe CaptureRecipe::createStaticPedalCapture()
{
    CaptureRecipe recipe;
    recipe.recipeId = "pedal-static-nonlinear-basic";
    recipe.displayName = "Static Pedal Capture";
    recipe.category = HansoCategory::Pedal;

    recipe.fixedControls.push_back({ "Tone / EQ", "12 o'clock", 0.5f });
    recipe.fixedControls.push_back({ "Level", "Unity gain", 0.5f });

    recipe.steps.push_back({ "setup", "Setup Confirmed",
                             utf8("지원 대상은 Distortion / Overdrive / Fuzz / Boost / 고정 EQ처럼 시간에 따라 변하지 않는 페달입니다.\nModulation / Delay / Reverb와 자동으로 움직이는 파라미터는 현재 캡쳐하지 않습니다.\n앱 출력 → 리앰프 박스(권장) → 페달 Input, 페달 Output → 인터페이스 Instrument/Line Input으로 연결하세요."),
                             CaptureStepStatus::Ready, {}, true });
    recipe.steps.push_back({ "calibration", "Calibration Complete",
                             utf8("Drive를 100%로, Tone/EQ는 12시로 맞추고 Level은 바이패스와 비슷한 크기에서 시작하세요.\nCalibration Start 후 페달 Level 또는 인터페이스 입력 게인을 조절해 리턴이 -36 ~ -8 dBFS에 3초 이상 머물게 하세요."),
                             CaptureStepStatus::NotStarted, {}, true });
    recipe.steps.push_back({ "gain-100", "Drive 100% Capture",
                             utf8("Drive를 100%로 유지하고 캡쳐하세요. Tone/EQ와 Level은 Calibration 때 위치를 바꾸지 마세요."),
                             CaptureStepStatus::NotStarted,
                             { "gain", 1.0f, "Drive 100%", CaptureProbeVariant::HansoProbeA1Full }, true });
    recipe.steps.push_back({ "gain-050", "Drive 50% Capture",
                             utf8("Drive만 50%로 낮춘 뒤 캡쳐하세요. Tone/EQ와 Level은 그대로 유지합니다."),
                             CaptureStepStatus::NotStarted,
                             { "gain", 0.5f, "Drive 50%", CaptureProbeVariant::HansoProbeA1Delta }, true });
    recipe.steps.push_back({ "gain-010", "Drive 10% Capture",
                             utf8("Drive만 10%로 낮춘 뒤 캡쳐하세요. Tone/EQ와 Level은 그대로 유지합니다."),
                             CaptureStepStatus::NotStarted,
                             { "gain", 0.1f, "Drive 10%", CaptureProbeVariant::HansoProbeA1Delta }, true });
    recipe.steps.push_back({ "final-validation", "Finish Capture",
                             utf8("세 Drive anchor를 모두 캡쳐했다면 분석과 정적 비선형 모델 추출을 실행합니다. 이 단계에서는 페달로 신호를 보내지 않습니다."),
                             CaptureStepStatus::NotStarted, {}, true });

    return recipe;
}

CaptureRecipe CaptureRecipe::createCabinetMicPositions()
{
    CaptureRecipe recipe;
    recipe.recipeId = "cabinet-mic-position-basic";
    recipe.displayName = "Cabinet Mic Position Capture";
    recipe.category = HansoCategory::Cabinet;

    recipe.steps.push_back({ "setup", "Setup Confirmed",
                             utf8("캐비넷 캡쳐 체인을 확인하세요.\n앱 출력 → 파워앰프/IR 소스 → 캐비넷/마이크 또는 안전한 로드박스 → 오디오 인터페이스 입력으로 연결합니다.\nSpeaker Out을 오디오 인터페이스에 직접 연결하지 마세요."),
                             CaptureStepStatus::Ready, {}, true });
    recipe.steps.push_back({ "calibration", "Calibration Complete",
                             utf8("Calibration Start를 누른 뒤 출력과 리턴 레벨이 안전 범위에 들어오도록 조절하세요.\n장비 입력 레벨(기타 레벨)을 먼저 맞추고, 리턴 타겟은 장비의 출력 볼륨으로 조절하세요. 앱 출력을 과도하게 낮춰 입력을 굶기지 마세요.\n리턴 입력이 -36 ~ -8 dBFS에 3초 이상 머물면 자동 완료됩니다."),
                             CaptureStepStatus::NotStarted, {}, true });
    for (const auto& position : kCabinetMicPositions)
    {
        recipe.steps.push_back({ position.id, position.label,
                                 utf8(position.instruction),
                                 CaptureStepStatus::NotStarted,
                                 { "cabinet-position", position.normalizedPosition, position.label,
                                   CaptureProbeVariant::CabinetProbeC1 },
                                 false });
    }
    recipe.steps.push_back({ "final-validation", "Finish / Build Cabinet",
                             utf8("최소 1개 이상의 마이크 위치가 직접 캡쳐되었거나 IR로 Import되어야 합니다.\n비어 있는 위치는 HANSO가 실제 IR source를 기반으로 estimated compact cab 슬롯으로 채웁니다."),
                             CaptureStepStatus::NotStarted, {}, true });

    return recipe;
}
}
