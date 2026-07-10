#include "Analysis/ModelFidelityEvaluator.h"

#include "Analysis/SpectrumUtils.h"
#include "Audio/PreviewModelProcessor.h"

namespace hanso
{
namespace
{
constexpr int renderBlockSize = 512;

float crestDb(const juce::AudioBuffer<float>& buffer) noexcept
{
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0)
        return 0.0f;

    const auto peak = buffer.getMagnitude(0, 0, buffer.getNumSamples());
    const auto rms = buffer.getRMSLevel(0, 0, buffer.getNumSamples());
    if (peak <= 1.0e-9f || rms <= 1.0e-9f)
        return 0.0f;

    return juce::Decibels::gainToDecibels(peak) - juce::Decibels::gainToDecibels(rms);
}

float meanEsrDb(const CompactHansoModelAnchor& anchor,
                const std::vector<const juce::AudioBuffer<float>*>& drySamples,
                const std::vector<const juce::AudioBuffer<float>*>& realSamples,
                double sampleRate)
{
    auto sum = 0.0f;
    auto count = 0;
    for (size_t i = 0; i < drySamples.size() && i < realSamples.size(); ++i)
    {
        if (drySamples[i] == nullptr || realSamples[i] == nullptr)
            continue;

        const auto render = ModelFidelityEvaluator::renderAnchor(*drySamples[i], anchor, sampleRate);
        sum += ModelFidelityEvaluator::esrDb(*realSamples[i], render);
        ++count;
    }

    return count > 0 ? sum / static_cast<float>(count) : 0.0f;
}
}

juce::AudioBuffer<float> ModelFidelityEvaluator::renderAnchor(const juce::AudioBuffer<float>& dry,
                                                              const CompactHansoModelAnchor& anchor,
                                                              double sampleRate)
{
    juce::AudioBuffer<float> rendered(1, dry.getNumSamples());
    rendered.clear();
    if (dry.getNumChannels() == 0 || dry.getNumSamples() <= 0 || sampleRate <= 0.0)
        return rendered;

    // A dedicated offline instance: never shared with the realtime preview
    // processor. Loading a single-anchor model pins the interpolation to the
    // anchor under evaluation.
    CompactHansoModel model;
    model.sampleRate = sampleRate;
    model.inputChannels = 1;
    model.outputChannels = 1;
    model.anchors.push_back(anchor);

    PreviewModelProcessor processor;
    processor.prepare(sampleRate, renderBlockSize, 1);
    processor.setPreviewGainPercent(anchor.parameterValue * 100.0f);
    if (! processor.loadModel(model))
        return rendered;

    juce::AudioBuffer<float> outputBlock(1, renderBlockSize);
    for (int start = 0; start < dry.getNumSamples(); start += renderBlockSize)
    {
        const auto blockSamples = juce::jmin(renderBlockSize, dry.getNumSamples() - start);
        outputBlock.setSize(1, blockSamples, false, false, true);
        const float* inputPointers[1] { dry.getReadPointer(0, start) };
        processor.process(inputPointers, 1, outputBlock);
        rendered.copyFrom(0, start, outputBlock, 0, 0, blockSamples);
    }

    return rendered;
}

float ModelFidelityEvaluator::esrDb(const juce::AudioBuffer<float>& reference,
                                    const juce::AudioBuffer<float>& estimate)
{
    const auto numSamples = juce::jmin(reference.getNumSamples(), estimate.getNumSamples());
    if (numSamples <= 0 || reference.getNumChannels() == 0 || estimate.getNumChannels() == 0)
        return 0.0f;

    const auto* ref = reference.getReadPointer(0);
    const auto* est = estimate.getReadPointer(0);
    auto errorEnergy = 0.0;
    auto referenceEnergy = 0.0;
    for (int i = 0; i < numSamples; ++i)
    {
        const auto difference = static_cast<double>(ref[i]) - static_cast<double>(est[i]);
        errorEnergy += difference * difference;
        referenceEnergy += static_cast<double>(ref[i]) * static_cast<double>(ref[i]);
    }

    if (referenceEnergy <= 1.0e-20)
        return 0.0f;

    return static_cast<float>(10.0 * std::log10(juce::jmax(errorEnergy, 1.0e-20) / referenceEnergy));
}

ModelFidelityResult ModelFidelityEvaluator::evaluate(const juce::AudioBuffer<float>& dry,
                                                     const juce::AudioBuffer<float>& real,
                                                     const CompactHansoModelAnchor& anchor,
                                                     double sampleRate)
{
    ModelFidelityResult result;
    if (dry.getNumSamples() <= 0 || real.getNumSamples() <= 0 || sampleRate <= 0.0)
        return result;

    const auto render = renderAnchor(dry, anchor, sampleRate);
    result.esrDb = esrDb(real, render);

    const AveragedPowerSpectrum realSpectrum(real, sampleRate);
    const AveragedPowerSpectrum renderSpectrum(render, sampleRate);
    result.lowBandErrorDb = renderSpectrum.bandPowerDb(60.0, 250.0) - realSpectrum.bandPowerDb(60.0, 250.0);
    result.midBandErrorDb = renderSpectrum.bandPowerDb(250.0, 2500.0) - realSpectrum.bandPowerDb(250.0, 2500.0);
    result.highBandErrorDb = renderSpectrum.bandPowerDb(2500.0, 10000.0) - realSpectrum.bandPowerDb(2500.0, 10000.0);
    result.crestFactorDeltaDb = crestDb(render) - crestDb(real);
    result.valid = true;
    return result;
}

CompactHansoModelAnchor ModelFidelityEvaluator::refineAnchor(const CompactHansoModelAnchor& sweepFitAnchor,
                                                             const std::vector<const juce::AudioBuffer<float>*>& drySamples,
                                                             const std::vector<const juce::AudioBuffer<float>*>& realSamples,
                                                             double sampleRate,
                                                             float& esrBeforeDb,
                                                             float& esrAfterDb)
{
    esrBeforeDb = meanEsrDb(sweepFitAnchor, drySamples, realSamples, sampleRate);
    esrAfterDb = esrBeforeDb;
    if (drySamples.empty() || realSamples.empty())
        return sweepFitAnchor;

    auto best = sweepFitAnchor;
    auto bestEsr = esrBeforeDb;

    struct Dimension
    {
        float CompactHansoModelAnchor::* member;
        float range;
        float step;
        float minValue;
        float maxValue;
    };

    const Dimension dimensions[] {
        { &CompactHansoModelAnchor::drive, 4.0f, 0.25f, 1.0f, 16.0f },
        { &CompactHansoModelAnchor::outputGainDb, 3.0f, 0.5f, -36.0f, 24.0f },
        { &CompactHansoModelAnchor::lowShelfDb, 3.0f, 0.5f, -18.0f, 18.0f },
        { &CompactHansoModelAnchor::midPeakDb, 3.0f, 0.5f, -18.0f, 18.0f },
        { &CompactHansoModelAnchor::highShelfDb, 3.0f, 0.5f, -18.0f, 18.0f },
    };

    for (int pass = 0; pass < 2; ++pass)
    {
        for (const auto& dimension : dimensions)
        {
            const auto centre = best.*(dimension.member);
            auto bestValue = centre;
            for (auto value = centre - dimension.range; value <= centre + dimension.range + 1.0e-4f;
                 value += dimension.step)
            {
                const auto clamped = juce::jlimit(dimension.minValue, dimension.maxValue, value);
                auto candidate = best;
                candidate.*(dimension.member) = clamped;
                const auto esr = meanEsrDb(candidate, drySamples, realSamples, sampleRate);
                if (esr < bestEsr - 0.01f)
                {
                    bestEsr = esr;
                    bestValue = clamped;
                }
            }

            best.*(dimension.member) = bestValue;
        }
    }

    if (bestEsr >= esrBeforeDb)
        return sweepFitAnchor;

    esrAfterDb = bestEsr;
    return best;
}
}
