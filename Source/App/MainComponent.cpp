#include "App/MainComponent.h"

#include "App/Utf8.h"

namespace hanso
{
namespace
{
class AudioSettingsDialogContent final : public juce::Component
{
public:
    AudioSettingsDialogContent(ApplicationState& state,
                               AudioEngine& audioEngine,
                               CaptureEngine& captureEngine)
        : audioPanel(state, audioEngine, captureEngine)
    {
        addAndMakeVisible(audioPanel);
        audioPanel.startUpdating();
        setSize(760, 560);
    }

    void resized() override
    {
        audioPanel.setBounds(getLocalBounds());
    }

private:
    AudioSettingsPanel audioPanel;
};

class LogDialogContent final : public juce::Component,
                               private juce::Timer
{
public:
    explicit LogDialogContent(ApplicationState& state)
        : logPanel(state)
    {
        addAndMakeVisible(logPanel);
        startTimerHz(12);
        setSize(760, 560);
    }

    void resized() override
    {
        logPanel.setBounds(getLocalBounds());
    }

private:
    void timerCallback() override
    {
        logPanel.refresh();
    }

    LogPanel logPanel;
};
}

MainComponent::MainComponent()
    : audioEngine(state),
      captureEngine(state, audioEngine),
      analysisEngine(state),
      workflow(state, captureEngine, analysisEngine),
      capturePanel(state, captureEngine, workflow)
{
    setLookAndFeel(&lookAndFeel);

    lookAndFeel.setColour(juce::ResizableWindow::backgroundColourId, juce::Colour::fromRGB(24, 25, 27));
    lookAndFeel.setColour(juce::TabbedComponent::backgroundColourId, juce::Colour::fromRGB(24, 25, 27));
    lookAndFeel.setColour(juce::TabbedButtonBar::tabTextColourId, juce::Colours::white);

    appTitle.setText("HANSO Lab", juce::dontSendNotification);
    appTitle.setJustificationType(juce::Justification::centred);
    appTitle.setFont(juce::Font(20.0f, juce::Font::bold));
    addAndMakeVisible(appTitle);

    settingsButton.setButtonText(utf8("⚙"));
    settingsButton.setTooltip("Settings");
    settingsButton.onClick = [this] { showSettingsDialog(); };
    addAndMakeVisible(settingsButton);

    addAndMakeVisible(capturePanel);
    capturePanel.startUpdating();

    audioEngine.initialise();
    state.appendLog("HANSO Lab initialized.");
    setSize(1440, 820);
    startTimerHz(12);
}

MainComponent::~MainComponent()
{
    stopTimer();
    audioEngine.shutdown();
    setLookAndFeel(nullptr);
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(24, 25, 27));
    g.setColour(juce::Colour::fromRGB(18, 19, 21));
    g.fillRect(getLocalBounds().removeFromTop(54));
    g.setColour(juce::Colour::fromRGB(56, 60, 64));
    g.drawHorizontalLine(54, 0.0f, static_cast<float>(getWidth()));
}

void MainComponent::resized()
{
    auto area = getLocalBounds();
    auto top = area.removeFromTop(54).reduced(12, 8);
    settingsButton.setBounds(top.removeFromRight(42));
    appTitle.setBounds(top);
    capturePanel.setBounds(area.reduced(10));
}

void MainComponent::timerCallback()
{
}

void MainComponent::requestCloseWithWarning(std::function<void()> closeApplication)
{
    if (! state.hasUnsavedCaptureData())
    {
        closeApplication();
        return;
    }

    auto options = juce::MessageBoxOptions()
        .withIconType(juce::MessageBoxIconType::WarningIcon)
        .withTitle(utf8("저장되지 않은 캡쳐 데이터"))
        .withMessage(utf8("프로그램을 종료하면 저장되지 않은 캡쳐 데이터가 삭제됩니다.\n종료할까요?"))
        .withButton(utf8("종료"))
        .withButton(utf8("취소"))
        .withAssociatedComponent(this);

    juce::AlertWindow::showAsync(options, [close = std::move(closeApplication)](int result)
    {
        if (result == 1)
            close();
    });
}

void MainComponent::showSettingsDialog()
{
    juce::PopupMenu menu;
    menu.addItem(1, "Audio Settings");
    menu.addItem(2, "Log");
    menu.addSeparator();
    menu.addItem(3, layoutDebugEnabled ? "Hide Layout Debug" : "Show Layout Debug");

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(settingsButton),
                       [this](int result)
                       {
                           if (result == 1)
                               showAudioSettingsDialog();
                           else if (result == 2)
                               showLogDialog();
                           else if (result == 3)
                               toggleLayoutDebugMode();
                       });
}

void MainComponent::toggleLayoutDebugMode()
{
    layoutDebugEnabled = ! layoutDebugEnabled;
    capturePanel.setLayoutDebugEnabled(layoutDebugEnabled);
    state.appendLog(layoutDebugEnabled ? "Layout debug mode enabled." : "Layout debug mode disabled.");
}

void MainComponent::showAudioSettingsDialog()
{
    audioEngine.deviceController().showSettingsDialog(this);
    state.appendLog("Audio device settings opened.");
}

void MainComponent::showLogDialog()
{
    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "Log";
    options.dialogBackgroundColour = juce::Colour::fromRGB(24, 25, 27);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = true;
    options.content.setOwned(new LogDialogContent(state));

    if (auto* window = options.launchAsync())
        window->centreWithSize(760, 560);
}
}
