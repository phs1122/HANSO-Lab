#include "UI/OnboardingPanel.h"

#include "App/Utf8.h"

namespace hanso
{
namespace
{
const auto cPanel   = juce::Colour::fromRGB(31, 33, 36);
const auto cPanel2  = juce::Colour::fromRGB(38, 40, 43);
const auto cBorder  = juce::Colour::fromRGB(58, 62, 66);
const auto cText    = juce::Colour::fromRGB(233, 234, 236);
const auto cTextDim = juce::Colour::fromRGB(154, 160, 166);
const auto cBlue    = juce::Colour::fromRGB(73, 151, 191);
const auto cBlueBtn = juce::Colour::fromRGB(53, 111, 176);
const auto cBlueSoft= juce::Colour::fromRGB(31, 52, 68);
const auto cGreen   = juce::Colour::fromRGB(80, 190, 118);
const auto cGreenSft= juce::Colour::fromRGB(20, 38, 26);
const auto cRed     = juce::Colour::fromRGB(210, 80, 58);
const auto cRedSoft = juce::Colour::fromRGB(44, 24, 21);
}

void OnboardingChoice::paintButton(juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced(0.5f);
    constexpr float radius = 12.0f;

    auto bg = cPanel;
    if (selected)                     bg = cBlueSoft;
    else if (isEnabled() && (highlighted || down)) bg = cPanel2;
    if (! isEnabled())                bg = juce::Colour::fromRGB(25, 26, 28);

    g.setColour(bg);
    g.fillRoundedRectangle(r, radius);
    g.setColour(selected ? cBlueBtn : cBorder.withAlpha(isEnabled() ? 1.0f : 0.4f));
    g.drawRoundedRectangle(r, radius, selected ? 1.6f : 1.0f);

    auto inner = r.reduced(20.0f, 16.0f);
    g.setColour(juce::Colours::white.withAlpha(isEnabled() ? 1.0f : 0.45f));
    g.setFont(juce::Font(18.0f, juce::Font::bold));
    auto titleArea = inner.removeFromTop(descText.isEmpty() ? inner.getHeight() : 24.0f);
    g.drawText(titleText, titleArea, descText.isEmpty() ? juce::Justification::centredLeft
                                                        : juce::Justification::topLeft);

    if (descText.isNotEmpty())
    {
        g.setColour(cTextDim.withAlpha(isEnabled() ? 1.0f : 0.45f));
        g.setFont(juce::Font(15.0f));
        g.drawFittedText(descText, inner.toNearestInt(), juce::Justification::topLeft, 3);
    }

    if (selected)
    {
        g.setColour(cBlue);
        g.setFont(juce::Font(18.0f, juce::Font::bold));
        g.drawText(juce::String::fromUTF8("\xE2\x9C\x93"),
                   getLocalBounds().removeFromRight(40).removeFromTop(40), juce::Justification::centred);
    }
}

OnboardingPanel::OnboardingPanel()
{
    auto styleLabel = [this](juce::Label& l, float size, bool bold, juce::Justification j, juce::Colour col)
    {
        l.setFont(juce::Font(size, bold ? juce::Font::bold : juce::Font::plain));
        l.setJustificationType(j);
        l.setColour(juce::Label::textColourId, col);
        l.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(l);
    };

    styleLabel(kicker, 12.0f, true, juce::Justification::centred, cBlue);
    styleLabel(titleLabel, 28.0f, true, juce::Justification::centred, cText);
    styleLabel(subtitle, 15.0f, false, juce::Justification::centred, cTextDim);
    styleLabel(note, 13.0f, false, juce::Justification::topLeft, cText);
    styleLabel(footer, 12.0f, false, juce::Justification::centred, cTextDim);
    subtitle.setMinimumHorizontalScale(1.0f);
    note.setJustificationType(juce::Justification::topLeft);

    for (int i = 0; i < (int) choices.size(); ++i)
    {
        choices[(size_t) i].onClick = [this, i] { applyChoice(i); };
        addAndMakeVisible(choices[(size_t) i]);
        choices[(size_t) i].setVisible(false);
    }

    prevButton.setButtonText(utf8("이전"));
    prevButton.onClick = [this] { goPrev(); };
    addAndMakeVisible(prevButton);

    nextButton.setButtonText(utf8("다음"));
    nextButton.setColour(juce::TextButton::buttonColourId, cBlueBtn);
    nextButton.onClick = [this] { goNext(); };
    addAndMakeVisible(nextButton);

    skipButton.setButtonText(utf8("건너뛰기"));
    skipButton.onClick = [this] { if (onSkip) onSkip(); };
    addAndMakeVisible(skipButton);

    restart();
}

void OnboardingPanel::restart()
{
    step = Step::Welcome;
    hasInterface = true;
    target = CaptureType::Amp;
    hasReamp = false;
    outType = -1;
    outCapture = -1;
    interfaceAnswered = false;
    targetAnswered = false;
    reampAnswered = false;
    render();
}

int OnboardingPanel::visibleChoiceCount() const
{
    int n = 0;
    for (const auto& c : choices)
        if (c.isVisible())
            ++n;
    return n;
}

void OnboardingPanel::computeVerdict()
{
    // The last choice in each output-capture set is always "없음" (no path).
    const int noneIndex = (target == CaptureType::Cabinet || target == CaptureType::PreAmp) ? 1 : 2;
    feasible = (outCapture >= 0 && outCapture != noneIndex);
}

void OnboardingPanel::render()
{
    for (auto& c : choices) { c.setVisible(false); c.setSelected(false); }
    note.setVisible(false);
    subtitle.setVisible(true);
    prevButton.setVisible(step != Step::Welcome);
    nextButton.setButtonText(utf8("다음"));

    auto choice = [this](int i, const juce::String& t, const juce::String& d, bool enabled)
    {
        choices[(size_t) i].configure(t, d, enabled);
    };
    auto select = [this](int i) { if (i >= 0 && i < (int) choices.size()) choices[(size_t) i].setSelected(true); };

    switch (step)
    {
        case Step::Welcome:
            kicker.setText("HANSO LAB", juce::dontSendNotification);
            titleLabel.setText(utf8("HANSO Lab에 오신 것을 환영합니다"), juce::dontSendNotification);
            subtitle.setText(utf8("기타 앰프·캐비닛의 사운드를 캡쳐해 .hanso 모델로 만듭니다.\n시작 전에 장비 몇 가지만 확인할게요."), juce::dontSendNotification);
            nextButton.setButtonText(utf8("시작하기"));
            nextEnabled = true;
            break;

        case Step::Interface:
            kicker.setText(utf8("장비 확인"), juce::dontSendNotification);
            titleLabel.setText(utf8("오디오 인터페이스가 있나요?"), juce::dontSendNotification);
            subtitle.setText(utf8("인터페이스로 장비에 신호를 보내고 되돌아오는 소리를 녹음합니다."), juce::dontSendNotification);
            choice(0, utf8("예, 있습니다"), {}, true);
            choice(1, utf8("아니오, 없습니다"), {}, true);
            if (interfaceAnswered) select(hasInterface ? 0 : 1);
            nextEnabled = interfaceAnswered && hasInterface;
            if (interfaceAnswered && ! hasInterface)
            {
                note.setText(utf8("⛔  캡쳐에는 오디오 인터페이스가 필요합니다. 준비 후 다시 시작하세요."), juce::dontSendNotification);
                note.setColour(juce::Label::textColourId, cRed);
                note.setVisible(true);
            }
            break;

        case Step::Target:
            kicker.setText(utf8("캡쳐 대상"), juce::dontSendNotification);
            titleLabel.setText(utf8("무엇을 캡쳐하나요?"), juce::dontSendNotification);
            subtitle.setText(utf8("대상에 따라 필요한 장비와 캡쳐 방식이 달라집니다."), juce::dontSendNotification);
            choice(0, "Full Rig", utf8("일체형 콤보 앰프, 헤드/캐비넷 분리형 스택 앰프. Line Out이나 마이킹을 통해 캡쳐합니다."), true);
            choice(1, "Amp Head", utf8("헤드 앰프 (스피커 없음). 라인/DI/로드박스로 출력을 받습니다."), true);
            choice(2, "Pedal / Effect", utf8("드라이브/이펙터. 아직 지원하지 않습니다."), false);
            choice(3, "Pre Amp Only", utf8("프리앰프 단만 캡쳐 (FX 센드/프리앰프 아웃, 캐비넷 없음)."), true);
            choice(4, "Cabinet", utf8("스피커 IR을 캡쳐합니다. 마이크 + 신호원이 필요합니다."), true);
            if (targetAnswered)
                select(target == CaptureType::Amp ? 1 : target == CaptureType::PreAmp ? 3
                       : target == CaptureType::Cabinet ? 4 : 0);   // Full Rig → 0
            nextEnabled = targetAnswered;
            break;

        case Step::Reamp:
            kicker.setText(utf8("입력 경로"), juce::dontSendNotification);
            titleLabel.setText(utf8("리앰프 박스가 있나요?"), juce::dontSendNotification);
            subtitle.setText(utf8("리앰프 박스는 인터페이스 출력을 앰프가 기대하는 신호로 바꿔줍니다. 있으면 더 정확합니다."), juce::dontSendNotification);
            choice(0, utf8("예, 있습니다"), utf8("일반 캡쳐 — Line Out → 리앰프 박스 → 장비"), true);
            choice(1, utf8("아니오, 없습니다"), utf8("간편 캡쳐 — 헤드폰/라인 직결"), true);
            if (reampAnswered) select(hasReamp ? 0 : 1);
            nextEnabled = reampAnswered;
            break;

        case Step::OutputType:
            kicker.setText(utf8("입력 경로"), juce::dontSendNotification);
            titleLabel.setText(utf8("인터페이스 출력은 무엇인가요?"), juce::dontSendNotification);
            subtitle.setText(utf8("리앰프 박스가 없으니 앱 출력이 유일한 게인 레버입니다."), juce::dontSendNotification);
            choice(0, utf8("라인 아웃 있음"), utf8("더 나은 경로. 스테레오 가능. 험/그라운드 루프에 주의하세요."), true);
            choice(1, utf8("헤드폰만 있음"), utf8("권장: TS 기타 케이블 (앱이 Left만 출력). 3.5mm면 3.5→6.35 어댑터."), true);
            if (outType >= 0) select(outType);
            nextEnabled = (outType >= 0);
            break;

        case Step::OutputCapture:
            kicker.setText(utf8("출력 경로"), juce::dontSendNotification);
            if (target == CaptureType::Cabinet)
            {
                titleLabel.setText(utf8("마이크가 있나요?"), juce::dontSendNotification);
                subtitle.setText(utf8("스피커를 마이킹해 캡쳐합니다."), juce::dontSendNotification);
                choice(0, utf8("마이크 있음"), utf8("스피커를 마이킹해 IR을 캡쳐합니다."), true);
                choice(1, utf8("없음"), utf8("마이크가 필요합니다."), true);
            }
            else if (target == CaptureType::FullRig)
            {
                titleLabel.setText(utf8("장비 소리를 어떻게 받나요?"), juce::dontSendNotification);
                subtitle.setText(utf8("일체형 콤보는 Line Out, 스택 앰프는 마이킹으로 받습니다."), juce::dontSendNotification);
                choice(0, "Line Out", utf8("콤보/앰프의 라인·이뮬레이티드 아웃."), true);
                choice(1, utf8("마이크"), utf8("스피커를 마이킹. 스택 앰프에 권장."), true);
                choice(2, utf8("없음"), utf8("출력 캡쳐 경로가 없습니다."), true);
                note.setColour(juce::Label::textColourId, cBlue);
                note.setText(utf8("💡 스택 앰프는 Line Out이 없는 경우가 많습니다 — 마이크 캡쳐를 권장합니다."), juce::dontSendNotification);
                note.setVisible(true);
            }
            else if (target == CaptureType::PreAmp)
            {
                titleLabel.setText(utf8("프리앰프 출력을 어떻게 받나요?"), juce::dontSendNotification);
                subtitle.setText(utf8("프리앰프 아웃 / FX 루프 센드에서 라인 레벨로 받습니다."), juce::dontSendNotification);
                choice(0, utf8("라인 · DI · FX 센드"), utf8("프리앰프 아웃 또는 이펙트 루프 센드."), true);
                choice(1, utf8("없음"), utf8("출력 캡쳐 경로가 없습니다."), true);
            }
            else
            {
                titleLabel.setText(utf8("앰프 소리를 어떻게 받나요?"), juce::dontSendNotification);
                subtitle.setText(utf8("스피커 아웃을 인터페이스에 직결하지 마세요."), juce::dontSendNotification);
                choice(0, utf8("로드박스"), utf8("스피커 부하 + 라인 레벨 출력. 가장 안전합니다."), true);
                choice(1, utf8("라인 · DI · 센드"), utf8("앰프의 라인/DI 아웃 또는 이펙트 루프 센드."), true);
                choice(2, utf8("없음"), utf8("출력 캡쳐 장비가 없습니다."), true);
            }
            if (outCapture >= 0) select(outCapture);
            nextEnabled = (outCapture >= 0);
            break;

        case Step::Verdict:
        {
            computeVerdict();
            kicker.setText(utf8("결과"), juce::dontSendNotification);
            subtitle.setVisible(false);
            note.setVisible(true);
            const auto modeText = hasReamp ? utf8("일반 캡쳐") : utf8("간편 캡쳐");
            if (feasible)
            {
                titleLabel.setText(utf8("이 방식으로 캡쳐할 수 있어요 🎉"), juce::dontSendNotification);
                juce::String path = hasReamp
                    ? utf8("Line Out → 리앰프 박스 → 장비 (스테레오, 출력 -12 dBFS 고정)")
                    : (outType == 0 ? utf8("라인 아웃 직결 (스테레오, 출력 슬라이더)")
                                    : utf8("헤드폰 직결 · TS 케이블 (Left-only, 출력 슬라이더)"));
                note.setText(utf8("✅  ") + modeText + "\n" + path, juce::dontSendNotification);
                note.setColour(juce::Label::textColourId, cGreen);
                nextButton.setButtonText(utf8("튜토리얼로"));
                nextEnabled = true;
            }
            else
            {
                titleLabel.setText(utf8("장비가 더 필요해요"), juce::dontSendNotification);
                const auto need = target == CaptureType::Cabinet ? utf8("스피커를 마이킹할 마이크가 필요합니다.")
                    : target == CaptureType::FullRig ? utf8("Line Out 또는 마이크가 필요합니다.")
                    : target == CaptureType::PreAmp ? utf8("프리앰프 아웃 / FX 센드(라인 레벨) 출력이 필요합니다.")
                    : utf8("앰프 출력을 받을 로드박스 또는 라인/DI 아웃이 필요합니다. (스피커 아웃 직결 금지)");
                note.setText(utf8("⚠️  ") + need + utf8("\n\n'이전'으로 돌아가 대상이나 답을 바꿀 수 있어요."), juce::dontSendNotification);
                note.setColour(juce::Label::textColourId, cRed);
                nextEnabled = false;
            }
            break;
        }

        case Step::Tutorial:
            kicker.setText(utf8("튜토리얼"), juce::dontSendNotification);
            titleLabel.setText(utf8("캡쳐 전에 알아두기"), juce::dontSendNotification);
            subtitle.setVisible(false);
            note.setVisible(true);
            note.setColour(juce::Label::textColourId, cText);
            note.setText(utf8(
                "원리 — 앱이 테스트 신호(스윕 등)를 장비로 보내고 되돌아온 소리를 녹음해, 그 전달 특성을 모델로 만듭니다.\n\n"
                "오디오 인터페이스 세팅\n"
                "  • 샘플레이트 48kHz · 버퍼는 안정적인 값\n"
                "  • 앱 출력 → 장비, 장비 리턴 → 캡쳐 입력 채널로 지정\n"
                "  • 리턴이 -36 ~ -8 dBFS에 들도록 입력 게인 조절 (클리핑 금지)\n"
                "  • 인터페이스 믹서의 다이렉트 모니터/루프백은 꺼서 이중 경로 방지"),
                juce::dontSendNotification);
            nextButton.setButtonText(utf8("캡쳐 시작"));
            nextEnabled = true;
            break;
    }

    nextButton.setEnabled(nextEnabled);
    resized();
    repaint();
}

void OnboardingPanel::applyChoice(int index)
{
    switch (step)
    {
        case Step::Interface:      interfaceAnswered = true; hasInterface = (index == 0); break;
        case Step::Target:         targetAnswered = true;    target = (index == 1 ? CaptureType::Amp : index == 3 ? CaptureType::PreAmp : index == 4 ? CaptureType::Cabinet : CaptureType::FullRig); break;
        case Step::Reamp:          reampAnswered = true;     hasReamp = (index == 0); break;
        case Step::OutputType:     outType = index; break;
        case Step::OutputCapture:  outCapture = index; break;
        default: break;
    }
    render();
}

OnboardingPanel::Step OnboardingPanel::nextStep(Step s) const
{
    switch (s)
    {
        case Step::Welcome:       return Step::Interface;
        case Step::Interface:     return Step::Target;
        case Step::Target:        return Step::Reamp;
        case Step::Reamp:         return hasReamp ? Step::OutputCapture : Step::OutputType;
        case Step::OutputType:    return Step::OutputCapture;
        case Step::OutputCapture: return Step::Verdict;
        case Step::Verdict:       return Step::Tutorial;
        case Step::Tutorial:      return Step::Tutorial;
    }
    return Step::Welcome;
}

OnboardingPanel::Step OnboardingPanel::prevStep(Step s) const
{
    switch (s)
    {
        case Step::Interface:     return Step::Welcome;
        case Step::Target:        return Step::Interface;
        case Step::Reamp:         return Step::Target;
        case Step::OutputType:    return Step::Reamp;
        case Step::OutputCapture: return hasReamp ? Step::Reamp : Step::OutputType;
        case Step::Verdict:       return Step::OutputCapture;
        case Step::Tutorial:      return Step::Verdict;
        case Step::Welcome:       return Step::Welcome;
    }
    return Step::Welcome;
}

void OnboardingPanel::goNext()
{
    if (step == Step::Tutorial)
    {
        if (onComplete)
            onComplete(target, hasReamp ? CaptureMode::Standard : CaptureMode::Easy);
        return;
    }

    if (! nextEnabled)
        return;

    step = nextStep(step);
    render();
}

void OnboardingPanel::goPrev()
{
    if (step == Step::Welcome)
        return;
    step = prevStep(step);
    render();
}

void OnboardingPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(24, 25, 27));

    if (note.isVisible())
    {
        auto r = note.getBounds().toFloat().expanded(18.0f, 14.0f);
        g.setColour(cPanel);
        g.fillRoundedRectangle(r, 10.0f);
        g.setColour(cBorder.withAlpha(0.55f));
        g.drawRoundedRectangle(r, 10.0f, 1.0f);
    }

    int idx = -1;
    switch (step)
    {
        case Step::Interface:     idx = 0; break;
        case Step::Target:        idx = 1; break;
        case Step::Reamp:
        case Step::OutputType:    idx = 2; break;
        case Step::OutputCapture: idx = 3; break;
        case Step::Verdict:       idx = 4; break;
        default: break;
    }
    if (idx >= 0)
    {
        constexpr int total = 5;
        const float gap = 22.0f, radius = 4.0f;
        const float startX = getWidth() * 0.5f - (total - 1) * gap * 0.5f;
        const float cy = 34.0f;
        for (int i = 0; i < total; ++i)
        {
            g.setColour(i <= idx ? cBlueBtn : cBorder);
            g.fillEllipse(startX + i * gap - radius, cy - radius, radius * 2.0f, radius * 2.0f);
        }
    }
}

void OnboardingPanel::resized()
{
    auto area = getLocalBounds();

    // top-right skip
    skipButton.setBounds(area.getRight() - 118, 16, 100, 30);

    // bottom nav bar
    auto nav = area.removeFromBottom(78).reduced(0, 20);
    const int contentW = juce::jmin(720, area.getWidth() - 48);
    auto navInner = nav.withSizeKeepingCentre(contentW, nav.getHeight());
    prevButton.setBounds(navInner.removeFromLeft(96).withSizeKeepingCentre(96, 42));
    nextButton.setBounds(navInner.removeFromRight(140).withSizeKeepingCentre(140, 42));

    // centered content column
    auto col = area.withSizeKeepingCentre(contentW, area.getHeight()).reduced(0, 16);
    col.removeFromTop(48);   // room for the step-indicator dots painted at the top
    kicker.setBounds(col.removeFromTop(18));
    col.removeFromTop(10);
    titleLabel.setBounds(col.removeFromTop(40));
    col.removeFromTop(8);
    subtitle.setBounds(col.removeFromTop(subtitle.isVisible() ? 52 : 0));
    col.removeFromTop(subtitle.isVisible() ? 24 : 8);

    const int n = visibleChoiceCount();
    if (n > 0)
    {
        for (int i = 0; i < (int) choices.size(); ++i)
        {
            if (! choices[(size_t) i].isVisible())
                continue;
            choices[(size_t) i].setBounds(col.removeFromTop(76));
            col.removeFromTop(10);
        }
    }

    if (note.isVisible())
        note.setBounds(col.removeFromTop(juce::jmin(200, col.getHeight())));
}
}
