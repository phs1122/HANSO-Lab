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
}
