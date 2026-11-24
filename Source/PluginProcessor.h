#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <vector>

namespace ParamIDs
{
    inline constexpr const char* sync        = "sync";
    inline constexpr const char* division    = "division";
    inline constexpr const char* fallbackBpm = "fallbackBpm";
    inline constexpr const char* timeMs      = "timeMs";
    inline constexpr const char* throws      = "throws";
    inline constexpr const char* duck        = "duck";
    inline constexpr const char* lowCut      = "lowCut";
    inline constexpr const char* highCut     = "highCut";
    inline constexpr const char* hornFreq    = "hornFreq";
    inline constexpr const char* honk        = "honk";
    inline constexpr const char* drive       = "drive";
    inline constexpr const char* bits        = "bits";
    inline constexpr const char* rate        = "rate";
    inline constexpr const char* pingPong    = "pingPong";
    inline constexpr const char* width       = "width";
    inline constexpr const char* mix         = "mix";
    inline constexpr const char* output      = "output";
}

/** Note divisions offered by the tempo-sync selector (length in quarter notes). */
struct NoteDivision { const char* name; double beats; };

inline constexpr std::array<NoteDivision, 10> kDivisions {{
    { "1/16",          0.25 },
    { "1/8 Triplet",   1.0 / 3.0 },
    { "1/8",           0.5 },
    { "1/8 Dotted",    0.75 },
    { "1/4 Triplet",   2.0 / 3.0 },
    { "1/4",           1.0 },
    { "3/8 (1/4 Dot)", 1.5 },
    { "1/2",           2.0 },
    { "1/2 Dotted",    3.0 },
    { "1 Bar",         4.0 },
}};
inline constexpr int kDefaultDivision = 5; // 1/4

//==============================================================================
/** Cytomic / Andy Simper state-variable filter. Stable under fast modulation. */
struct SimperSVF
{
    enum class Mode { lowpass, highpass, bell };

    void setup (Mode mode, double sampleRate, float freq, float q, float gainDb = 0.0f) noexcept
    {
        freq = juce::jlimit (10.0f, (float) (sampleRate * 0.45), freq);
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float g = (float) std::tan (juce::MathConstants<double>::pi * freq / sampleRate);
        const float k = (mode == Mode::bell) ? 1.0f / (q * A) : 1.0f / q;

        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;

        switch (mode)
        {
            case Mode::lowpass:  m0 = 0.0f; m1 = 0.0f;              m2 = 1.0f;  break;
            case Mode::highpass: m0 = 1.0f; m1 = -k;                m2 = -1.0f; break;
            case Mode::bell:     m0 = 1.0f; m1 = k * (A * A - 1.0f); m2 = 0.0f;  break;
        }
    }

    float process (float v0) noexcept
    {
        const float v3 = v0 - ic2eq;
        const float v1 = a1 * ic1eq + a2 * v3;
        const float v2 = ic2eq + a2 * ic1eq + a3 * v3;
        ic1eq = 2.0f * v1 - ic1eq;
        ic2eq = 2.0f * v2 - ic2eq;
        return m0 * v0 + m1 * v1 + m2 * v2;
    }

    void reset() noexcept { ic1eq = ic2eq = 0.0f; }

    float a1 = 0, a2 = 0, a3 = 0, m0 = 0, m1 = 0, m2 = 0;
    float ic1eq = 0, ic2eq = 0;
};

//==============================================================================
class HornDelayAudioProcessor : public juce::AudioProcessor
{
public:
    HornDelayAudioProcessor();
    ~HornDelayAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Horn Delay"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;

    /** For the GUI: BPM currently used and whether it came from the host. */
    std::atomic<double> displayBpm { 90.0 };
    std::atomic<bool>   bpmFromHost { false };
    std::atomic<double> displayDelayMs { 0.0 };

private:
    struct Preset
    {
        const char* name;
        std::vector<std::pair<const char*, float>> values;
    };
    static const std::vector<Preset>& getPresets();

    double computeDelaySeconds();
    float readTap (const std::vector<float>& line, double delaySamples) const noexcept;
    void updateFilters();

    // Parameter pointers (raw atomics are real-time safe to read)
    std::atomic<float>* pSync = nullptr;     std::atomic<float>* pDivision = nullptr;
    std::atomic<float>* pFallbackBpm = nullptr; std::atomic<float>* pTimeMs = nullptr;
    std::atomic<float>* pThrows = nullptr;   std::atomic<float>* pDuck = nullptr;
    std::atomic<float>* pLowCut = nullptr;   std::atomic<float>* pHighCut = nullptr;
    std::atomic<float>* pHornFreq = nullptr; std::atomic<float>* pHonk = nullptr;
    std::atomic<float>* pDrive = nullptr;    std::atomic<float>* pBits = nullptr;
    std::atomic<float>* pRate = nullptr;     std::atomic<float>* pPingPong = nullptr;
    std::atomic<float>* pWidth = nullptr;    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pOutput = nullptr;

    double sr = 44100.0;

    // Delay lines (one per channel) with crossfading read heads
    std::array<std::vector<float>, 2> lines;
    int lineSize = 0, writePos = 0;
    double currentDelay = 0.0, nextDelay = 0.0, fadePos = 0.0, fadeInc = 0.0;
    bool fading = false;

    // Feedback-loop filters (per channel) and wet-path horn bell
    std::array<SimperSVF, 2> loopHP, loopLP, hornBell;

    // Smoothed controls
    juce::SmoothedValue<float> smFeedback, smMix, smOutput, smWidth, smDrive, smDuck;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> smLowCut, smHighCut, smHornFreq;
    juce::SmoothedValue<float> smHonk;

    // Lo-fi state
    std::array<float, 2> holdSample { 0.0f, 0.0f };
    double holdPhase = 1.0;

    // DC blockers after saturation
    std::array<float, 2> dcX { 0, 0 }, dcY { 0, 0 };

    // Ducking envelope
    float duckEnv = 0.0f, duckAtt = 0.0f, duckRel = 0.0f;

    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HornDelayAudioProcessor)
};
