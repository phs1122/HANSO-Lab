#include <JuceHeader.h>

#include "App/MainComponent.h"

namespace hanso
{
class HansoLabApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "HANSO Lab"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String&) override
    {
        mainWindow = std::make_unique<MainWindow>(getApplicationName());
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override
    {
        if (mainWindow != nullptr)
        {
            mainWindow->requestClose();
            return;
        }

        quit();
    }

    void anotherInstanceStarted(const juce::String&) override {}

private:
    class MainWindow final : public juce::DocumentWindow
    {
    public:
        explicit MainWindow(juce::String name)
            : DocumentWindow(std::move(name),
                             juce::Desktop::getInstance().getDefaultLookAndFeel()
                                 .findColour(juce::ResizableWindow::backgroundColourId),
                             DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(), true);
            setResizable(true, true);
            centreWithSize(1600, 1000);
            setVisible(true);
        }

        void closeButtonPressed() override
        {
            requestClose();
        }

        void requestClose()
        {
            auto closeApplication = []
            {
                juce::JUCEApplication::getInstance()->quit();
            };

            if (auto* mainComponent = dynamic_cast<MainComponent*>(getContentComponent()))
            {
                mainComponent->requestCloseWithWarning(std::move(closeApplication));
                return;
            }

            closeApplication();
        }
    };

    std::unique_ptr<MainWindow> mainWindow;
};
}

START_JUCE_APPLICATION(hanso::HansoLabApplication)
