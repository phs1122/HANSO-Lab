#pragma once

#include <JuceHeader.h>

#include "Capture/CaptureMode.h"
#include "Capture/CaptureType.h"

namespace hanso
{
// A selectable card (title + description) used for onboarding questionnaire answers.
class OnboardingChoice final : public juce::Button
{
public:
    OnboardingChoice() : juce::Button({}) {}

    void configure(const juce::String& title, const juce::String& desc, bool enabled)
    {
        titleText = title;
        descText = desc;
        setEnabled(enabled);
        selected = false;
        setVisible(true);
        repaint();
    }

    void setSelected(bool s) { selected = s; repaint(); }
    bool isSelected() const noexcept { return selected; }

    void paintButton(juce::Graphics&, bool highlighted, bool down) override;

private:
    juce::String titleText, descText;
    bool selected = false;
};

// Everything the questionnaire learned about the user's rig, handed to the
// capture panel so metadata and cable guidance start from the real answers.
struct OnboardingResult
{
    CaptureType captureType { CaptureType::Amp };
    CaptureMode mode { CaptureMode::Easy };
    ExcitationPath excitationPath { ExcitationPath::DirectLine };
    ReturnPath returnPath { ReturnPath::Unknown };
    CableGuideType cableGuide { CableGuideType::TrsToDualTsYCable };
};

// First-run onboarding: welcome -> equipment questionnaire -> verdict -> tutorial.
// On completion it reports the questionnaire result to the host.
class OnboardingPanel final : public juce::Component
{
public:
    OnboardingPanel();

    std::function<void(const OnboardingResult&)> onComplete;
    std::function<void()> onSkip;

    void restart();
    void resized() override;
    void paint(juce::Graphics&) override;

private:
    enum class Step { Welcome, Interface, Target, Reamp, OutputType, CableCheck,
                      ExperimentalConsent, OutputCapture, Verdict, Tutorial };

    void render();
    void applyChoice(int index);
    void goNext();
    void goPrev();
    Step nextStep(Step) const;
    Step prevStep(Step) const;
    void computeVerdict();
    int visibleChoiceCount() const;

    Step step = Step::Welcome;

    // answers
    bool hasInterface = true;
    CaptureType target = CaptureType::Amp;
    bool hasReamp = false;
    int outType = -1;        // 0 = line out, 1 = headphone only
    int cableChoice = -1;    // headphone only: 0 = TRS->dual-TS breakout, 1 = single TS (experimental)
    int outCapture = -1;     // amp: 0 loadbox / 1 line-di-send / 2 none ; cab: 0 mic / 1 none
    bool feasible = true;
    bool interfaceAnswered = false;
    bool targetAnswered = false;
    bool reampAnswered = false;

    int selectedChoice = -1;
    bool nextEnabled = false;

    juce::Label kicker, titleLabel, subtitle, note, footer;
    std::array<OnboardingChoice, 6> choices;
    juce::TextButton prevButton, nextButton, skipButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OnboardingPanel)
};
}
