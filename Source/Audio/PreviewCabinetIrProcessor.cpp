#include "Audio/PreviewCabinetIrProcessor.h"

#include "Serialization/HansoAudioChunkCodec.h"

namespace hanso
{
namespace
{
constexpr float positionEpsilon = 0.0005f;
}

float PreviewCabinetIrProcessor::normalizedPositionForId(const juce::String& id) noexcept
{
    if (id == "cab-center")
        return 0.0f;
    if (id == "cab-edge")
        return 0.33f;
    if (id == "cab-cone")
        return 0.66f;
    if (id == "cab-off-axis")
        return 1.0f;
    return 0.5f;
}

CabinetSlotSource PreviewCabinetIrProcessor::sourceFromString(const juce::String& text) noexcept
{
    if (text == "captured-ir")
        return CabinetSlotSource::CapturedIr;
    if (text == "imported-ir")
        return CabinetSlotSource::ImportedIr;
    if (text == "estimated-compact-cab")
        return CabinetSlotSource::EstimatedCompactCab;
    return CabinetSlotSource::Empty;
}

bool PreviewCabinetIrProcessor::loadPositionIr(const HansoPackage& package,
                                              const juce::String& chunkId,
                                              PositionIr& destination,
                                              juce::String& error)
{
    if (chunkId.isEmpty())
    {
        error = "Missing impulse response chunk id.";
        return false;
    }

    const auto* chunk = package.findChunk(chunkId);
    if (chunk == nullptr)
    {
        error = "Chunk not found: " + chunkId;
        return false;
    }

    double sampleRate = 0.0;
    if (chunk->mediaType == "audio/x-hanso-pcm16")
    {
        if (! HansoAudioChunkCodec::decodePcm16Audio(chunk->data, destination.impulseResponse, sampleRate, error))
            return false;
    }
    else if (chunk->mediaType == "audio/x-hanso-float32")
    {
        if (! HansoAudioChunkCodec::decodeFloat32Audio(chunk->data, destination.impulseResponse, sampleRate, error))
            return false;
    }
    else
    {
        error = "Unsupported IR media type: " + chunk->mediaType;
        return false;
    }

    juce::ignoreUnused(sampleRate);
    return destination.impulseResponse.getNumSamples() > 0;
}

void PreviewCabinetIrProcessor::prepare(double newSampleRate, int maximumBlockSize, int outputChannels) noexcept
{
    processSpec.sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    processSpec.maximumBlockSize = static_cast<juce::uint32>(juce::jmax(1, maximumBlockSize));
    processSpec.numChannels = static_cast<juce::uint32>(juce::jmax(1, outputChannels));
    convolution.prepare(processSpec);
    convolution.reset();
    loadedMicPosition = -1.0f;
}

void PreviewCabinetIrProcessor::reset() noexcept
{
    convolution.reset();
}

void PreviewCabinetIrProcessor::clear() noexcept
{
    positions.clear();
    modelLoaded = false;
    summaryText.clear();
    loadedMicPosition = -1.0f;
    convolution.reset();
}

bool PreviewCabinetIrProcessor::loadFromPackage(const HansoPackage& package, juce::String& error)
{
    clear();

    const auto* profileObject = package.cabinetProfile.getDynamicObject();
    if (profileObject == nullptr)
    {
        error = "Cabinet package does not contain cabProfile metadata.";
        return false;
    }

    const auto positionsVar = profileObject->getProperty("positions");
    if (! positionsVar.isArray())
    {
        error = "Cabinet cabProfile.positions is missing.";
        return false;
    }

    juce::String cabinetName = profileObject->getProperty("cabinetName").toString();
    if (cabinetName.isEmpty())
        cabinetName = package.metadata.name;

    for (const auto& entry : *positionsVar.getArray())
    {
        const auto* positionObject = entry.getDynamicObject();
        if (positionObject == nullptr)
            continue;

        const auto sourceText = positionObject->getProperty("source").toString();
        const auto source = sourceFromString(sourceText);
        if (source != CabinetSlotSource::CapturedIr && source != CabinetSlotSource::ImportedIr)
            continue;

        PositionIr positionIr;
        positionIr.id = positionObject->getProperty("id").toString();
        positionIr.label = positionObject->getProperty("label").toString();
        positionIr.normalizedPosition = normalizedPositionForId(positionIr.id);
        positionIr.source = source;

        const auto chunkId = positionObject->getProperty("impulseResponseChunkId").toString();
        if (! loadPositionIr(package, chunkId, positionIr, error))
            return false;

        positions.push_back(std::move(positionIr));
    }

    if (positions.empty())
    {
        error = "Cabinet package has no previewable IR positions (captured-ir / imported-ir).";
        return false;
    }

    std::sort(positions.begin(),
              positions.end(),
              [](const PositionIr& a, const PositionIr& b)
              {
                  return a.normalizedPosition < b.normalizedPosition;
              });

    modelLoaded = true;
    summaryText = cabinetName + " / " + juce::String(positions.size()) + " IR position(s)";
    loadedMicPosition = -1.0f;
    rebuildConvolutionIfNeeded();
    return true;
}

void PreviewCabinetIrProcessor::setMicPositionNormalized(float normalizedPosition) noexcept
{
    micPosition.store(juce::jlimit(0.0f, 1.0f, normalizedPosition));
}

bool PreviewCabinetIrProcessor::hasModel() const noexcept
{
    return modelLoaded;
}

juce::String PreviewCabinetIrProcessor::summary() const
{
    return summaryText;
}

void PreviewCabinetIrProcessor::buildBlendedImpulse(float normalizedPosition,
                                                    juce::AudioBuffer<float>& blended) const
{
    blended.setSize(0, 0);
    if (positions.empty())
        return;

    const auto clamped = juce::jlimit(0.0f, 1.0f, normalizedPosition);
    const auto& lower = positions.front();
    const auto& upper = positions.back();

    if (clamped <= lower.normalizedPosition)
    {
        blended.makeCopyOf(lower.impulseResponse, true);
        return;
    }

    if (clamped >= upper.normalizedPosition)
    {
        blended.makeCopyOf(upper.impulseResponse, true);
        return;
    }

    const PositionIr* left = &lower;
    const PositionIr* right = &upper;
    for (size_t i = 1; i < positions.size(); ++i)
    {
        if (positions[i].normalizedPosition >= clamped)
        {
            right = &positions[i];
            left = &positions[i - 1];
            break;
        }
    }

    const auto span = juce::jmax(0.0001f, right->normalizedPosition - left->normalizedPosition);
    const auto mix = (clamped - left->normalizedPosition) / span;
    const auto maxSamples = juce::jmax(left->impulseResponse.getNumSamples(),
                                       right->impulseResponse.getNumSamples());
    const auto channels = juce::jmax(left->impulseResponse.getNumChannels(),
                                     right->impulseResponse.getNumChannels());
    blended.setSize(channels, maxSamples, false, true, true);
    blended.clear();

    for (int channel = 0; channel < channels; ++channel)
    {
        const auto* leftSamples = left->impulseResponse.getReadPointer(juce::jmin(channel, left->impulseResponse.getNumChannels() - 1));
        const auto* rightSamples = right->impulseResponse.getReadPointer(juce::jmin(channel, right->impulseResponse.getNumChannels() - 1));
        auto* output = blended.getWritePointer(channel);

        for (int sample = 0; sample < maxSamples; ++sample)
        {
            const auto leftValue = sample < left->impulseResponse.getNumSamples() ? leftSamples[sample] : 0.0f;
            const auto rightValue = sample < right->impulseResponse.getNumSamples() ? rightSamples[sample] : 0.0f;
            output[sample] = leftValue + (rightValue - leftValue) * mix;
        }
    }
}

void PreviewCabinetIrProcessor::rebuildConvolutionIfNeeded() noexcept
{
    if (! modelLoaded)
        return;

    const auto targetPosition = micPosition.load();
    if (std::abs(targetPosition - loadedMicPosition) < positionEpsilon)
        return;

    juce::AudioBuffer<float> blended;
    buildBlendedImpulse(targetPosition, blended);
    if (blended.getNumSamples() <= 0)
        return;

  #if JUCE_MAJOR_VERSION >= 8
    convolution.loadImpulseResponse(std::move(blended),
                                    processSpec.sampleRate,
                                    juce::dsp::Convolution::Stereo::yes,
                                    juce::dsp::Convolution::Trim::yes,
                                    juce::dsp::Convolution::Normalise::yes);
  #else
    convolution.loadImpulseResponse(blended,
                                    processSpec.sampleRate,
                                    juce::dsp::Convolution::Stereo::yes,
                                    juce::dsp::Convolution::Trim::yes,
                                    juce::dsp::Convolution::Normalise::yes);
  #endif

    loadedMicPosition = targetPosition;
}

void PreviewCabinetIrProcessor::process(const float* const* inputChannels,
                                        int numInputChannels,
                                        juce::AudioBuffer<float>& outputBuffer,
                                        int numSamples) noexcept
{
    if (! modelLoaded || numSamples <= 0)
        return;

    rebuildConvolutionIfNeeded();

    const auto channelsToWrite = juce::jmin(outputBuffer.getNumChannels(), numInputChannels);
    for (int channel = 0; channel < channelsToWrite; ++channel)
        outputBuffer.copyFrom(channel, 0, inputChannels[channel], numSamples);

    if (channelsToWrite == 1 && outputBuffer.getNumChannels() > 1)
    {
        for (int channel = 1; channel < outputBuffer.getNumChannels(); ++channel)
            outputBuffer.copyFrom(channel, 0, outputBuffer, 0, 0, numSamples);
    }

    juce::dsp::AudioBlock<float> block(outputBuffer);
    auto processBlock = block.getSubBlock(0, static_cast<size_t>(numSamples));
    juce::dsp::ProcessContextReplacing<float> context(processBlock);
    convolution.process(context);
}
}
