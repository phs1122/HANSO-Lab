#pragma once

#include <JuceHeader.h>

#include <functional>

namespace hanso
{
class PreviewWaveformComponent final : public juce::Component
{
public:
    std::function<void(double normalizedProgress)> onSeek;

    void setAudioBuffer(const juce::AudioBuffer<float>& buffer, double sampleRate);
    void setProgress(double newProgress);
    void clear();

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;

private:
    static juce::String formatTime(double seconds);
    juce::Rectangle<float> waveformBounds() const;
    double progressFromX(float x) const;
    void seekAt(const juce::MouseEvent& event);

    std::vector<float> peaks;
    double durationSeconds { 0.0 };
    double progress { 0.0 };
};
}
