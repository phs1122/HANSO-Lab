#include "Capture/CaptureRecipe.h"

#include "App/Utf8.h"

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
                             utf8("Gain 노브를 50% / 12시 위치에 맞추고, Phones 볼륨을 충분히 낮춘 뒤 Calibration Start를 누르세요.\n앱 출력 미터가 안전 범위에 있고 리턴 입력이 -36 ~ -8 dBFS에 3초 이상 머물면 자동 완료됩니다."),
                             CaptureStepStatus::NotStarted, {}, true });
    recipe.steps.push_back({ "gain-010", "Gain 10% Capture",
                             utf8("Gain 노브를 10% 위치에 맞춘 뒤 캡쳐하세요. 톤 노브는 모두 12시 방향을 유지합니다."),
                             CaptureStepStatus::NotStarted, { "gain", 0.1f, "Gain 10%" }, true });
    recipe.steps.push_back({ "gain-050", "Gain 50% Capture",
                             utf8("Gain 노브를 50% 위치에 맞춘 뒤 캡쳐하세요. 톤 노브는 모두 12시 방향을 유지합니다."),
                             CaptureStepStatus::NotStarted, { "gain", 0.5f, "Gain 50%" }, true });
    recipe.steps.push_back({ "gain-100", "Gain 100% Capture",
                             utf8("Gain 노브를 100% 위치에 맞춘 뒤 캡쳐하세요. 톤 노브는 모두 12시 방향을 유지합니다."),
                             CaptureStepStatus::NotStarted, { "gain", 1.0f, "Gain 100%" }, true });
    recipe.steps.push_back({ "final-validation", "Finish Capture",
                             utf8("필수 Gain anchor를 모두 캡쳐했다면 Finish Capture를 눌러 분석과 최종 검증을 실행하세요. 이 단계에서는 장비로 신호를 보내지 않습니다."),
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
                             utf8("Calibration Start를 누른 뒤 출력과 리턴 레벨이 안전 범위에 들어오도록 조절하세요.\n리턴 입력이 -36 ~ -8 dBFS에 3초 이상 머물면 자동 완료됩니다."),
                             CaptureStepStatus::NotStarted, {}, true });
    recipe.steps.push_back({ "cab-center", "Center",
                             utf8("마이크를 스피커 더스트캡 중심에 둔 위치입니다.\n직접 캡쳐하거나 외부 IR을 Import할 수 있습니다."),
                             CaptureStepStatus::NotStarted, { "cabinet-position", 0.0f, "Center" }, false });
    recipe.steps.push_back({ "cab-edge", "Edge",
                             utf8("마이크를 더스트캡과 콘의 경계 근처에 둔 위치입니다.\n비워두면 Finish 단계에서 실제 source를 기반으로 추정됩니다."),
                             CaptureStepStatus::NotStarted, { "cabinet-position", 0.33f, "Edge" }, false });
    recipe.steps.push_back({ "cab-cone", "Cone",
                             utf8("마이크를 스피커 콘 영역 쪽으로 이동한 위치입니다.\n직접 캡쳐, IR Import, 또는 자동 추정을 사용할 수 있습니다."),
                             CaptureStepStatus::NotStarted, { "cabinet-position", 0.66f, "Cone" }, false });
    recipe.steps.push_back({ "cab-off-axis", "Off-Axis",
                             utf8("마이크를 축에서 살짝 벗어난 각도로 둔 위치입니다.\n직접 캡쳐하거나 IR을 Import할 수 있습니다."),
                             CaptureStepStatus::NotStarted, { "cabinet-position", 1.0f, "Off-Axis" }, false });
    recipe.steps.push_back({ "final-validation", "Finish / Build Cabinet",
                             utf8("최소 1개 이상의 마이크 위치가 직접 캡쳐되었거나 IR로 Import되어야 합니다.\n비어 있는 위치는 HANSO가 실제 IR source를 기반으로 estimated compact cab 슬롯으로 채웁니다."),
                             CaptureStepStatus::NotStarted, {}, true });

    return recipe;
}
}
