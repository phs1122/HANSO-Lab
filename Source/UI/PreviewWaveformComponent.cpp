#include "UI/PreviewWaveformComponent.h"

namespace hanso
{
void PreviewWaveformComponent::setAudioBuffer(const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    peaks.clear();
    progress = 0.0;
    durationSeconds = sampleRate > 0.0 ? static_cast<double>(buffer.getNumSamples()) / sampleRate : 0.0;

    if (buffer.getNumSamples() <= 0 || buffer.getNumChannels() <= 0)
    {
        repaint();
        return;
    }

    constexpr auto bucketCount = 360;
    peaks.resize(bucketCount, 0.0f);
    const auto samplesPerBucket = juce::jmax(1, buffer.getNumSamples() / bucketCount);

    for (int bucket = 0; bucket < bucketCount; ++bucket)
    {
        const auto start = bucket * samplesPerBucket;
        const auto count = juce::jmin(samplesPerBucket, buffer.getNumSamples() - start);
        auto peak = 0.0f;

        if (count > 0)
        {
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                peak = juce::jmax(peak, buffer.getMagnitude(channel, start, count));
        }

        peaks[static_cast<size_t>(bucket)] = juce::jlimit(0.0f, 1.0f, peak);
    }

    repaint();
}

void PreviewWaveformComponent::setProgress(double newProgress)
{
    const auto clamped = juce::jlimit(0.0, 1.0, newProgress);
    if (std::abs(progress - clamped) < 0.001)
        return;

    progress = clamped;
    repaint();
}

void PreviewWaveformComponent::clear()
{
    peaks.clear();
    progress = 0.0;
    durationSeconds = 0.0;
    repaint();
}

void PreviewWaveformComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colour::fromRGB(19, 21, 23));
    g.fillRoundedRectangle(bounds, 5.0f);
    g.setColour(juce::Colour::fromRGB(78, 91, 98));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 5.0f, 1.0f);

    auto timeArea = getLocalBounds().reduced(10, 4).removeFromTop(18);
    const auto currentSeconds = progress * durationSeconds;
    g.setColour(juce::Colours::lightgrey);
    g.setFont(15.5f);
    g.drawText(formatTime(currentSeconds), timeArea.removeFromLeft(54), juce::Justification::centredLeft);
    g.drawText(formatTime(durationSeconds), timeArea.removeFromRight(54), juce::Justification::centredRight);

    auto wave = waveformBounds();
    if (wave.getHeight() <= 2.0f)
        return;

    const auto centerY = wave.getCentreY();
    const auto halfHeight = wave.getHeight() * 0.46f;
    const auto progressX = wave.getX() + wave.getWidth() * static_cast<float>(progress);

    g.setColour(juce::Colour::fromRGB(48, 58, 62));
    g.drawHorizontalLine(static_cast<int>(std::round(centerY)), wave.getX(), wave.getRight());

    if (peaks.empty())
    {
        g.setColour(juce::Colours::grey);
        g.drawText("No sample loaded", wave.toNearestInt(), juce::Justification::centred);
        return;
    }

    const auto barWidth = juce::jmax(1.0f, wave.getWidth() / static_cast<float>(peaks.size()));
    for (size_t i = 0; i < peaks.size(); ++i)
    {
        const auto x = wave.getX() + static_cast<float>(i) * barWidth;
        const auto peak = peaks[i];
        const auto height = juce::jmax(1.0f, peak * halfHeight);
        const auto bar = juce::Rectangle<float>(x, centerY - height, juce::jmax(1.0f, barWidth - 1.0f), height * 2.0f);

        g.setColour(x <= progressX ? juce::Colour::fromRGB(116, 197, 220)
                                   : juce::Colour::fromRGB(72, 87, 94));
        g.fillRect(bar);
    }

    g.setColour(juce::Colours::white.withAlpha(0.85f));
    g.drawVerticalLine(static_cast<int>(std::round(progressX)), wave.getY(), wave.getBottom());
}

void PreviewWaveformComponent::mouseDown(const juce::MouseEvent& event)
{
    seekAt(event);
}

void PreviewWaveformComponent::mouseDrag(const juce::MouseEvent& event)
{
    seekAt(event);
}

juce::Rectangle<float> PreviewWaveformComponent::waveformBounds() const
{
    return getLocalBounds().reduced(10, 24).toFloat();
}

double PreviewWaveformComponent::progressFromX(float x) const
{
    const auto wave = waveformBounds();
    if (wave.getWidth() <= 1.0f)
        return 0.0;

    return juce::jlimit(0.0, 1.0, static_cast<double>((x - wave.getX()) / wave.getWidth()));
}

void PreviewWaveformComponent::seekAt(const juce::MouseEvent& event)
{
    if (peaks.empty() || onSeek == nullptr)
        return;

    const auto normalized = progressFromX(event.position.x);
    setProgress(normalized);
    onSeek(normalized);
}

juce::String PreviewWaveformComponent::formatTime(double seconds)
{
    const auto totalSeconds = juce::jmax(0, static_cast<int>(std::floor(seconds)));
    const auto minutes = totalSeconds / 60;
    const auto remainingSeconds = totalSeconds % 60;
    return juce::String(minutes).paddedLeft('0', 2) + ":" + juce::String(remainingSeconds).paddedLeft('0', 2);
}
}
