#include "Analysis/CaptureQualityAnalyzer.h"

#include "App/Utf8.h"

namespace hanso
{
namespace
{
void addIssue(CaptureQualityReport& report,
              CaptureQualitySeverity severity,
              const juce::String& code,
              const juce::String& english,
              const juce::String& korean,
              const juce::String& actionEnglish,
              const juce::String& actionKorean)
{
    report.issues.push_back({ severity, code, english, korean, actionEnglish, actionKorean });
    if (severity == CaptureQualitySeverity::Error)
        report.overallSeverity = CaptureQualitySeverity::Error;
    else if (severity == CaptureQualitySeverity::Warning && report.overallSeverity != CaptureQualitySeverity::Error)
        report.overallSeverity = CaptureQualitySeverity::Warning;
}
}

float CaptureQualityAnalyzer::gainToDb(float gain) noexcept
{
    return juce::Decibels::gainToDecibels(gain, -120.0f);
}

CaptureQualityReport CaptureQualityAnalyzer::analyze(const juce::AudioBuffer<float>& captured,
                                                     int expectedSamples,
                                                     int latencySamples,
                                                     double latencyMs,
                                                     float alignmentConfidence,
                                                     bool easyModeRightMuted,
                                                     CaptureQualityTarget target) const
{
    CaptureQualityReport report;
    report.latencySamples = latencySamples;
    report.latencyMs = latencyMs;
    report.alignmentConfidence = alignmentConfidence;

    if (captured.getNumChannels() == 0 || captured.getNumSamples() == 0)
    {
        addIssue(report, CaptureQualitySeverity::Error, "no-capture-buffer",
                 "No captured audio buffer exists.",
                 utf8("캡쳐된 오디오 버퍼가 없습니다."),
                 "Record this step again.",
                 utf8("이 단계를 다시 녹음하세요."));
        return report;
    }

    auto peak = 0.0f;
    double sumSquares = 0.0;
    juce::int64 sampleCount = 0;
    int clipSamples = 0;

    for (int channel = 0; channel < captured.getNumChannels(); ++channel)
    {
        const auto* data = captured.getReadPointer(channel);
        for (int sample = 0; sample < captured.getNumSamples(); ++sample)
        {
            const auto value = std::abs(data[sample]);
            peak = juce::jmax(peak, value);
            sumSquares += static_cast<double>(value) * static_cast<double>(value);
            ++sampleCount;
            if (value >= 0.999f)
                ++clipSamples;
        }
    }

    const auto rms = sampleCount > 0 ? static_cast<float>(std::sqrt(sumSquares / static_cast<double>(sampleCount))) : 0.0f;
    report.peakDbfs = gainToDb(peak);
    report.rmsDbfs = gainToDb(rms);
    report.clipSampleCount = clipSamples;

    // The region before the estimated device latency contains no test-signal
    // response, so it measures the true noise floor of the return path.
    report.noiseFloorDbfs = -120.0f;
    const auto noiseRegion = juce::jmin(latencySamples - 64, captured.getNumSamples());
    if (noiseRegion >= 512)
    {
        auto noiseSum = 0.0;
        for (int sample = 0; sample < noiseRegion; ++sample)
        {
            const auto value = static_cast<double>(captured.getSample(0, sample));
            noiseSum += value * value;
        }

        report.noiseFloorDbfs = gainToDb(static_cast<float>(std::sqrt(noiseSum / noiseRegion)));
    }

    if (report.noiseFloorDbfs > -120.0f)
        report.signalToNoiseDb = report.rmsDbfs - report.noiseFloorDbfs;

    if (report.rmsDbfs <= -80.0f)
        addIssue(report, CaptureQualitySeverity::Error, "no-return",
                 "No return signal detected. Check that your gear output is connected to the audio interface input.",
                 utf8("리턴 신호가 감지되지 않았습니다. 장비 Output이 오디오 인터페이스 Input으로 연결되어 있는지 확인하세요."),
                 "Check the cable and input channel, then record again.",
                 utf8("케이블과 입력 채널을 확인한 뒤 다시 녹음하세요."));
    else if (report.rmsDbfs < target.minimumRmsDbfs)
    {
        if (target.lowGainAnchor)
            addIssue(report, CaptureQualitySeverity::Warning, "input-low-gain-quiet",
                     "Low-gain captures may be physically quiet. This capture is usable if SNR and alignment are acceptable.",
                     utf8("저게인 캡쳐는 물리적으로 작을 수 있습니다. SNR과 정렬이 양호하면 사용할 수 있습니다."),
                     "Only raise the capture level if the preview is noisy or alignment warnings appear.",
                     utf8("노이즈가 크거나 정렬 경고가 있을 때만 캡쳐 레벨을 올려 다시 시도하세요."));
        else
            addIssue(report, CaptureQualitySeverity::Warning, "input-too-quiet",
                     "Input too quiet. Raise your Phones volume slightly.",
                     utf8("입력 레벨이 너무 작습니다. 헤드폰 볼륨을 조금 올리세요."),
                     "Raise output or gear return level and re-capture if possible.",
                     utf8("이 단계의 권장 RMS 범위는 ")
                         + juce::String(target.minimumRmsDbfs, 0)
                         + " ~ "
                         + juce::String(target.maximumRmsDbfs, 0)
                         + utf8(" dBFS입니다. 출력 또는 장비 리턴 레벨을 조금 올린 뒤 가능하면 다시 캡쳐하세요."));
    }

    if (report.rmsDbfs > target.maximumRmsDbfs)
        addIssue(report, CaptureQualitySeverity::Warning, "input-too-hot",
                 "Input too hot. Lower your Phones volume or gear output level.",
                 utf8("입력 레벨이 너무 큽니다. 헤드폰 볼륨 또는 장비 출력 레벨을 낮추세요."),
                 "Lower output by about 3 dB and re-capture this step.",
                 utf8("이 단계의 권장 RMS 범위는 ")
                     + juce::String(target.minimumRmsDbfs, 0)
                     + " ~ "
                     + juce::String(target.maximumRmsDbfs, 0)
                     + utf8(" dBFS입니다. 클리핑이 없다면 캡쳐는 사용할 수 있지만, 필요하면 출력을 약 3 dB 낮춰 다시 캡쳐하세요."));

    if (clipSamples > 0 || report.peakDbfs > -0.2f)
        addIssue(report, CaptureQualitySeverity::Error, "clipping",
                 "Clipping detected. Please lower the output level and record again.",
                 utf8("클리핑이 감지되었습니다. 출력 레벨을 낮추고 다시 녹음하세요."),
                 "Lower Phones volume or gear output level.",
                 utf8("헤드폰 볼륨 또는 장비 출력 레벨을 낮추세요."));

    if (expectedSamples > 0 && std::abs(captured.getNumSamples() - expectedSamples) > expectedSamples / 10)
        addIssue(report, CaptureQualitySeverity::Warning, "length-mismatch",
                 "Recording length differs from the expected test signal length.",
                 utf8("녹음 길이가 예상 테스트 신호 길이와 다릅니다."),
                 "Re-capture if the result sounds incomplete.",
                 utf8("결과가 잘린 것 같으면 다시 캡쳐하세요."));

    if (latencySamples < 0 || latencyMs < 0.0)
        addIssue(report, CaptureQualitySeverity::Error, "invalid-latency",
                 "Estimated latency is invalid.",
                 utf8("추정 레이턴시가 올바르지 않습니다."),
                 "Re-capture this step.",
                 utf8("이 단계를 다시 캡쳐하세요."));

    if (report.rmsDbfs > -80.0f)
    {
        if (alignmentConfidence < 0.25f)
            addIssue(report, CaptureQualitySeverity::Error, "alignment-failed",
                     "The recording does not correlate with the test signal. The wrong input may be selected.",
                     utf8("녹음이 테스트 신호와 상관관계가 없습니다. 다른 입력 채널이 선택되었을 수 있습니다."),
                     "Check the input channel routing and re-capture this step.",
                     utf8("입력 채널 라우팅을 확인한 뒤 이 단계를 다시 캡쳐하세요."));
        else if (alignmentConfidence < 0.5f)
            addIssue(report, CaptureQualitySeverity::Warning, "alignment-low",
                     "Alignment confidence is low. The capture may contain heavy noise or dropouts.",
                     utf8("정렬 신뢰도가 낮습니다. 캡쳐에 노이즈나 드롭아웃이 섞였을 수 있습니다."),
                     "Re-capture if the preview sounds wrong.",
                     utf8("프리뷰 소리가 이상하면 다시 캡쳐하세요."));
    }

    if (report.noiseFloorDbfs > -120.0f && report.rmsDbfs - report.noiseFloorDbfs < target.minimumSnrDb)
        addIssue(report, CaptureQualitySeverity::Warning, "high-noise-floor",
                 "Signal-to-noise ratio is below " + juce::String(target.minimumSnrDb, 0) + " dB.",
                 utf8("신호 대 잡음비가 ") + juce::String(target.minimumSnrDb, 0) + utf8(" dB 미만입니다."),
                 "Reduce noise sources or raise the capture level, then re-capture.",
                 utf8("노이즈 원인을 줄이거나 캡쳐 레벨을 올린 뒤 다시 캡쳐하세요."));

    if (! easyModeRightMuted)
        addIssue(report, CaptureQualitySeverity::Error, "easy-right-not-muted",
                 "Easy Capture must output the test signal on Left only.",
                 utf8("간편 캡쳐에서는 테스트 신호가 Left 채널로만 출력되어야 합니다."),
                 "Switch routing to Mono Left Only.",
                 utf8("출력 라우팅을 Mono Left Only로 변경하세요."));

    return report;
}
}
