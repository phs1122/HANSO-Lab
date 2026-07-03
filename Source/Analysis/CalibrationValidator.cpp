#include "Analysis/CalibrationValidator.h"

#include "App/Utf8.h"

namespace hanso
{
namespace
{
constexpr auto minAnalysisDb = -120.0f;

float powerToDbfs(double power) noexcept
{
    if (power <= 1.0e-24)
        return minAnalysisDb;

    return juce::Decibels::gainToDecibels(static_cast<float>(std::sqrt(power)), minAnalysisDb);
}

double dbToPower(float db) noexcept
{
    if (db <= minAnalysisDb)
        return 0.0;

    const auto gain = juce::Decibels::decibelsToGain(db);
    return static_cast<double>(gain) * static_cast<double>(gain);
}

float ratioToDb(double numerator, double denominator) noexcept
{
    if (numerator <= 1.0e-24)
        return minAnalysisDb;

    return static_cast<float>(10.0 * std::log10(numerator / juce::jmax(denominator, 1.0e-24)));
}

void setLoopbackMessage(CalibrationValidationResult& result)
{
    result.status = CalibrationValidationStatus::LoopbackSuspected;
    result.loopbackSuspected = true;
    result.code = "silent-return-detected";
    result.messageEnglish = "Return signal is present while HANSO Lab output is silent. Mute interface input-to-output monitoring loops and check hardware noise.";
    result.messageKorean = utf8("앱이 신호를 보내지 않는데 리턴이 감지됩니다. 인터페이스 믹서(TotalMix 등)에서 입력→출력 모니터링 루프백을 뮤트하고, 장비 노이즈를 확인하세요.");
}

void setIdentityMessage(CalibrationValidationResult& result)
{
    result.status = CalibrationValidationStatus::IdentityFailed;
    result.code = "probe-identity-failed";
    result.messageEnglish = "The return level is high, but it does not match the HANSO Lab calibration probe. Check for feedback, oscillation, hum, or mixer loopback.";
    result.messageKorean = utf8("리턴이 노이즈/발진으로 판정되었습니다. 인터페이스 믹서의 입력→출력 모니터링 루프백, 험, 자기발진을 확인하세요.");
}

void setLevelMessage(CalibrationValidationResult& result)
{
    result.status = CalibrationValidationStatus::SignalTooLow;
    result.code = "probe-level-too-low";
    result.messageEnglish = "The probe is detected, but the return is not high enough above the measured noise floor or is outside the calibration range.";
    result.messageKorean = utf8("신호는 감지되나 레벨이 부족합니다. 앱 출력 슬라이더를 안전 범위 안에서 조정하거나 장비의 리턴/출력 볼륨을 올려 주세요.");
}
}

float CalibrationValidator::rmsDbfs(const juce::AudioBuffer<float>& buffer) noexcept
{
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0)
        return minAnalysisDb;

    auto sumSquares = 0.0;
    auto count = 0;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const auto* samples = buffer.getReadPointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const auto value = static_cast<double>(samples[sample]);
            sumSquares += value * value;
            ++count;
        }
    }

    if (count == 0)
        return minAnalysisDb;

    return powerToDbfs(sumSquares / static_cast<double>(count));
}

CalibrationValidationResult CalibrationValidator::evaluateSilentReturn(const juce::AudioBuffer<float>& recentReturn,
                                                                       const CalibrationValidationConfig& config)
{
    CalibrationValidationResult result;
    result.returnRmsDbfs = rmsDbfs(recentReturn);
    result.returnLevelDbfs = result.returnRmsDbfs;
    result.noiseFloorDbfs = result.returnRmsDbfs;
    result.status = CalibrationValidationStatus::MeasuringNoise;
    result.code = "measuring-noise-floor";
    result.messageEnglish = "Measuring return noise floor with HANSO Lab output muted.";
    result.messageKorean = utf8("앱 출력을 무음으로 둔 상태에서 리턴 노이즈 플로어를 측정 중입니다.");

    if (result.returnRmsDbfs >= config.silentReturnLoopbackThresholdDbfs)
        setLoopbackMessage(result);

    return result;
}

CalibrationValidationResult CalibrationValidator::validateProbe(const juce::AudioBuffer<float>& recentReturn,
                                                                double sampleRate,
                                                                const double* probeFrequencies,
                                                                int numProbeFrequencies,
                                                                float returnLevelDbfs,
                                                                float outputLevelDbfs,
                                                                float noiseFloorDbfs,
                                                                const CalibrationValidationConfig& config)
{
    CalibrationValidationResult result;
    result.returnLevelDbfs = returnLevelDbfs;
    result.outputLevelDbfs = outputLevelDbfs;
    result.returnRmsDbfs = rmsDbfs(recentReturn);
    result.noiseFloorDbfs = noiseFloorDbfs;
    result.inputLevelInRange = returnLevelDbfs >= config.inputMinDbfs && returnLevelDbfs <= config.inputMaxDbfs;
    result.outputLevelInRange = outputLevelDbfs >= config.outputMinDbfs && outputLevelDbfs <= config.outputMaxDbfs;

    const auto returnPower = dbToPower(result.returnRmsDbfs);
    const auto noisePower = dbToPower(noiseFloorDbfs);
    const auto signalPower = juce::jmax(returnPower - noisePower, 0.0);
    result.signalToNoiseDb = ratioToDb(signalPower, noisePower);
    result.snrOk = result.returnRmsDbfs >= noiseFloorDbfs + config.requiredSnrDb;

    auto inBandPower = 0.0;
    auto outOfBandPower = 0.0;
    if (recentReturn.getNumChannels() > 0
        && recentReturn.getNumSamples() > 0
        && sampleRate > 0.0
        && probeFrequencies != nullptr
        && numProbeFrequencies > 0)
    {
        const auto* samples = recentReturn.getReadPointer(0);
        const auto numSamples = recentReturn.getNumSamples();
        for (int i = 0; i < numProbeFrequencies; ++i)
        {
            const auto frequency = probeFrequencies[i];
            if (frequency <= 0.0 || frequency >= sampleRate * 0.45)
                continue;

            inBandPower += goertzelPower(samples, numSamples, sampleRate, frequency);
            outOfBandPower += goertzelPower(samples, numSamples, sampleRate, frequency * 0.92);
            outOfBandPower += goertzelPower(samples, numSamples, sampleRate, frequency * 1.08);
        }

        outOfBandPower *= 0.5;
    }

    result.inBandDbfs = powerToDbfs(inBandPower);
    result.outOfBandDbfs = powerToDbfs(outOfBandPower);
    result.toneDominanceDb = ratioToDb(inBandPower, outOfBandPower);
    result.identityOk = result.toneDominanceDb >= config.requiredToneDominanceDb;

    if (! result.inputLevelInRange || ! result.outputLevelInRange || ! result.snrOk)
    {
        setLevelMessage(result);
        return result;
    }

    if (! result.identityOk)
    {
        setIdentityMessage(result);
        return result;
    }

    result.status = CalibrationValidationStatus::Passed;
    result.code = "probe-valid";
    result.messageEnglish = "Calibration probe verified.";
    result.messageKorean = utf8("캘리브레이션 프로브 신호가 정상적으로 리턴되었습니다.");
    return result;
}

double CalibrationValidator::goertzelPower(const float* samples,
                                           int numSamples,
                                           double sampleRate,
                                           double frequency) noexcept
{
    if (samples == nullptr || numSamples <= 0 || sampleRate <= 0.0 || frequency <= 0.0)
        return 0.0;

    const auto omega = juce::MathConstants<double>::twoPi * frequency / sampleRate;
    const auto coeff = 2.0 * std::cos(omega);
    auto q0 = 0.0;
    auto q1 = 0.0;
    auto q2 = 0.0;
    auto windowPower = 0.0;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto window = 0.5 - 0.5 * std::cos(juce::MathConstants<double>::twoPi
                                                 * static_cast<double>(i)
                                                 / static_cast<double>(juce::jmax(1, numSamples - 1)));
        q0 = window * static_cast<double>(samples[i]) + coeff * q1 - q2;
        q2 = q1;
        q1 = q0;
        windowPower += window * window;
    }

    const auto rawPower = q1 * q1 + q2 * q2 - coeff * q1 * q2;
    return rawPower / juce::jmax(windowPower, 1.0e-12);
}
}
