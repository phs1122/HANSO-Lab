#include "Analysis/MicColorationProfiles.h"

#include <cmath>

namespace hanso
{
namespace
{
// Coloration curves are evaluated at a fixed reference rate so the band values
// are deterministic regardless of the capture session sample rate.
constexpr double referenceSampleRate = 48000.0;
constexpr int samplesPerBand = 32;

// RBJ cookbook biquad, double precision, magnitude evaluation only. The
// coefficient formulas match hst::dsp::Biquad (HANSO TONE HstDspCommon.h) so
// the analytic curves equal what the FX cabinet stage renders at 48 kHz.
struct BiquadSpec
{
    enum class Type { LowPass, Peaking, LowShelf, HighShelf };

    Type type;
    double freqHz;
    double q;
    double gainDb;
};

struct BiquadCoefficients
{
    double b0, b1, b2, a1, a2;
};

BiquadCoefficients coefficientsFor(const BiquadSpec& spec)
{
    const auto w0 = 2.0 * 3.141592653589793 * spec.freqHz / referenceSampleRate;
    const auto cosW0 = std::cos(w0);
    const auto sinW0 = std::sin(w0);
    const auto alpha = sinW0 / (2.0 * std::max(spec.q, 0.1));
    const auto A = std::pow(10.0, spec.gainDb / 40.0);

    double b0 = 1.0, b1 = 0.0, b2 = 0.0;
    double a0 = 1.0, a1 = 0.0, a2 = 0.0;

    switch (spec.type)
    {
        case BiquadSpec::Type::LowPass:
            b0 = (1.0 - cosW0) * 0.5;
            b1 = 1.0 - cosW0;
            b2 = (1.0 - cosW0) * 0.5;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cosW0;
            a2 = 1.0 - alpha;
            break;

        case BiquadSpec::Type::LowShelf:
        {
            const auto sqrtA = std::sqrt(A);
            const auto twoSqrtAAlpha = 2.0 * sqrtA * alpha;
            b0 = A * ((A + 1.0) - (A - 1.0) * cosW0 + twoSqrtAAlpha);
            b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cosW0);
            b2 = A * ((A + 1.0) - (A - 1.0) * cosW0 - twoSqrtAAlpha);
            a0 = (A + 1.0) + (A - 1.0) * cosW0 + twoSqrtAAlpha;
            a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cosW0);
            a2 = (A + 1.0) + (A - 1.0) * cosW0 - twoSqrtAAlpha;
            break;
        }

        case BiquadSpec::Type::HighShelf:
        {
            const auto sqrtA = std::sqrt(A);
            const auto twoSqrtAAlpha = 2.0 * sqrtA * alpha;
            b0 = A * ((A + 1.0) + (A - 1.0) * cosW0 + twoSqrtAAlpha);
            b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cosW0);
            b2 = A * ((A + 1.0) + (A - 1.0) * cosW0 - twoSqrtAAlpha);
            a0 = (A + 1.0) - (A - 1.0) * cosW0 + twoSqrtAAlpha;
            a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cosW0);
            a2 = (A + 1.0) - (A - 1.0) * cosW0 - twoSqrtAAlpha;
            break;
        }

        case BiquadSpec::Type::Peaking:
            b0 = 1.0 + alpha * A;
            b1 = -2.0 * cosW0;
            b2 = 1.0 - alpha * A;
            a0 = 1.0 + alpha / A;
            a1 = -2.0 * cosW0;
            a2 = 1.0 - alpha / A;
            break;
    }

    return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
}

double magnitudeSquaredAt(const BiquadCoefficients& c, double hz)
{
    const auto w = 2.0 * 3.141592653589793 * hz / referenceSampleRate;
    const auto cosW = std::cos(w);
    const auto cos2W = std::cos(2.0 * w);

    const auto numerator = c.b0 * c.b0 + c.b1 * c.b1 + c.b2 * c.b2
                           + 2.0 * (c.b0 * c.b1 + c.b1 * c.b2) * cosW
                           + 2.0 * c.b0 * c.b2 * cos2W;
    const auto denominator = 1.0 + c.a1 * c.a1 + c.a2 * c.a2
                             + 2.0 * (c.a1 + c.a1 * c.a2) * cosW
                             + 2.0 * c.a2 * cos2W;

    return numerator / std::max(denominator, 1e-12);
}

template <size_t StageCount>
CabinetToneBandArray bandAveragedResponseDb(const std::array<BiquadSpec, StageCount>& stages)
{
    std::array<BiquadCoefficients, StageCount> coefficients {};
    for (size_t stage = 0; stage < StageCount; ++stage)
        coefficients[stage] = coefficientsFor(stages[stage]);

    CabinetToneBandArray bands {};
    for (int band = 0; band < cabinetToneBandCount; ++band)
    {
        const auto lowHz = kCabinetToneBands[band].lowHz;
        const auto highHz = kCabinetToneBands[band].highHz;
        const auto logStep = std::log(highHz / lowHz) / static_cast<double>(samplesPerBand);

        auto powerSum = 0.0;
        for (int sample = 0; sample < samplesPerBand; ++sample)
        {
            const auto hz = lowHz * std::exp((static_cast<double>(sample) + 0.5) * logStep);
            auto magnitudeSquared = 1.0;
            for (const auto& c : coefficients)
                magnitudeSquared *= magnitudeSquaredAt(c, hz);
            powerSum += magnitudeSquared;
        }

        const auto meanPower = powerSum / static_cast<double>(samplesPerBand);
        bands[static_cast<size_t>(band)] = static_cast<float>(10.0 * std::log10(std::max(meanPower, 1e-12)));
    }

    return bands;
}

// Mirrors hst::kMicProfiles (Dynamic, Ribbon, Condenser): presence, low-mid,
// high roll-off, and air stages, all peaking filters.
constexpr std::array<BiquadSpec, 4> kMicStages[] = {
    // Dynamic — presence boost, HF roll-off above ~8k
    { { { BiquadSpec::Type::Peaking, 4200.0, 0.9, 3.0 },
        { BiquadSpec::Type::Peaking, 500.0, 0.8, 0.0 },
        { BiquadSpec::Type::Peaking, 8000.0, 0.7, -5.0 },
        { BiquadSpec::Type::Peaking, 10000.0, 0.6, 0.0 } } },
    // Ribbon — soft highs, strong low-mid
    { { { BiquadSpec::Type::Peaking, 3500.0, 0.7, 0.5 },
        { BiquadSpec::Type::Peaking, 420.0, 0.9, 4.0 },
        { BiquadSpec::Type::Peaking, 6500.0, 0.8, -4.0 },
        { BiquadSpec::Type::Peaking, 10000.0, 0.6, 0.0 } } },
    // Condenser — flat with extended highs
    { { { BiquadSpec::Type::Peaking, 5000.0, 0.7, 1.0 },
        { BiquadSpec::Type::Peaking, 500.0, 0.8, 0.0 },
        { BiquadSpec::Type::Peaking, 8000.0, 0.7, 0.0 },
        { BiquadSpec::Type::Peaking, 11000.0, 0.5, 2.0 } } },
};

struct PositionStageSet
{
    std::array<BiquadSpec, 4> stages;
    float gainTrimDb;
};

// Mirrors hst::kMicPositionPresets (Cone, Cone Edge, Edge, Off-axis): low
// shelf, mid peak, high shelf, high-cut low-pass, plus a broadband trim.
constexpr PositionStageSet kPositionStages[] = {
    // Cone: direct, bright inner-cone response.
    { { { { BiquadSpec::Type::LowShelf, 120.0, 0.7, 2.5 },
          { BiquadSpec::Type::Peaking, 2500.0, 1.0, 2.0 },
          { BiquadSpec::Type::HighShelf, 6500.0, 0.8, 3.5 },
          { BiquadSpec::Type::LowPass, 9000.0, 0.707, 0.0 } } },
      0.5f },
    // Cone Edge: balanced transition between dust cap and cone.
    { { { { BiquadSpec::Type::LowShelf, 130.0, 0.7, 1.5 },
          { BiquadSpec::Type::Peaking, 2000.0, 1.0, 1.0 },
          { BiquadSpec::Type::HighShelf, 6000.0, 0.8, 2.0 },
          { BiquadSpec::Type::LowPass, 8500.0, 0.707, 0.0 } } },
      0.0f },
    // Edge: warmer outer-cone response with a lower high-frequency ceiling.
    { { { { BiquadSpec::Type::LowShelf, 150.0, 0.8, 2.0 },
          { BiquadSpec::Type::Peaking, 1500.0, 1.1, 3.5 },
          { BiquadSpec::Type::HighShelf, 5500.0, 0.9, 0.5 },
          { BiquadSpec::Type::LowPass, 7500.0, 0.707, 0.0 } } },
      -0.5f },
    // Off-axis
    { { { { BiquadSpec::Type::LowShelf, 160.0, 0.7, 1.0 },
          { BiquadSpec::Type::Peaking, 2200.0, 1.0, -1.5 },
          { BiquadSpec::Type::HighShelf, 5000.0, 0.8, -3.5 },
          { BiquadSpec::Type::LowPass, 6500.0, 0.707, 0.0 } } },
      -1.0f },
};

constexpr int kPositionStageCount = static_cast<int>(sizeof(kPositionStages) / sizeof(kPositionStages[0]));

int clampedPositionIndex(int positionIndex) noexcept
{
    // Cone Edge is the neutral reference position, matching the FX default.
    if (positionIndex < 0 || positionIndex >= kPositionStageCount)
        return 1;
    return positionIndex;
}
}

CabinetToneBandArray MicColorationProfiles::micColorationBandsDb(CabinetMicClass micClass)
{
    auto index = 0;
    switch (micClass)
    {
        case CabinetMicClass::Unknown:
        case CabinetMicClass::Dynamic: index = 0; break;
        case CabinetMicClass::Ribbon: index = 1; break;
        case CabinetMicClass::Condenser: index = 2; break;
    }

    return bandAveragedResponseDb(kMicStages[index]);
}

CabinetToneBandArray MicColorationProfiles::positionColorationBandsDb(int positionIndex)
{
    return bandAveragedResponseDb(kPositionStages[clampedPositionIndex(positionIndex)].stages);
}

float MicColorationProfiles::positionGainTrimDb(int positionIndex) noexcept
{
    return kPositionStages[clampedPositionIndex(positionIndex)].gainTrimDb;
}

int MicColorationProfiles::positionCount() noexcept
{
    return kPositionStageCount;
}
}
