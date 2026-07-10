#pragma once

#include <JuceHeader.h>

#include "Model/CompactHansoModel.h"

namespace hanso
{
// Quantifies how closely the extracted compact model reproduces a real
// amp-processed recording of the same dry material, and optionally refines
// the anchor parameters to minimise that error.
struct ModelFidelityResult
{
    bool valid { false };
    float esrDb { 0.0f };
    float lowBandErrorDb { 0.0f };
    float midBandErrorDb { 0.0f };
    float highBandErrorDb { 0.0f };
    float crestFactorDeltaDb { 0.0f };
};

class ModelFidelityEvaluator final
{
public:
    // Renders dry through the anchor's DSP offline and compares against the
    // real recording. Buffers are expected time-aligned; the shorter length
    // wins.
    static ModelFidelityResult evaluate(const juce::AudioBuffer<float>& dry,
                                        const juce::AudioBuffer<float>& real,
                                        const CompactHansoModelAnchor& anchor,
                                        double sampleRate);

    // Strict error-to-signal ratio in dB between two time-aligned buffers.
    // No gain matching: level differences count as error, which keeps the
    // output-gain dimension of the refinement meaningful.
    static float esrDb(const juce::AudioBuffer<float>& reference,
                       const juce::AudioBuffer<float>& estimate);

    // Coordinate-descent refinement of anchor drive / output gain / tone
    // around the sweep-fit starting point, minimising mean ESR across the
    // provided dry/real pairs. Returns the refined anchor; if no candidate
    // improves on the starting point the input anchor is returned unchanged.
    static CompactHansoModelAnchor refineAnchor(const CompactHansoModelAnchor& sweepFitAnchor,
                                                const std::vector<const juce::AudioBuffer<float>*>& drySamples,
                                                const std::vector<const juce::AudioBuffer<float>*>& realSamples,
                                                double sampleRate,
                                                float& esrBeforeDb,
                                                float& esrAfterDb);

    // Offline render of dry material through a single anchor's DSP chain.
    static juce::AudioBuffer<float> renderAnchor(const juce::AudioBuffer<float>& dry,
                                                 const CompactHansoModelAnchor& anchor,
                                                 double sampleRate);
};
}
