#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr double kMaxDelaySeconds = 6.5;   // 1 bar at 40 BPM = 6 s
    constexpr double kCrossfadeSeconds = 0.045; // read-head crossfade on time changes
    constexpr float  kMaxFeedback = 0.85f;      // safety cap
    constexpr int    kControlInterval = 32;     // filter coefficient update rate (samples)

    juce::String hzToString (float v, int)
    {
        return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";
    }
    juce::String pctToString (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; }

    /** Throws -> feedback gain: the (N+1)th repeat lands ~24 dB below the first. */
    float throwsToFeedback (float throws)
    {
        const float g = std::pow (juce::Decibels::decibelsToGain (-24.0f), 1.0f / juce::jmax (1.0f, throws));
        return juce::jmin (g, kMaxFeedback);
    }
}

//==============================================================================
HornDelayAudioProcessor::HornDelayAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "HornDelayState", createParameterLayout())
{
    auto get = [this] (const char* id) { auto* p = apvts.getRawParameterValue (id); jassert (p != nullptr); return p; };
    pSync = get (ParamIDs::sync);           pDivision = get (ParamIDs::division);
    pFallbackBpm = get (ParamIDs::fallbackBpm); pTimeMs = get (ParamIDs::timeMs);
    pThrows = get (ParamIDs::throws);       pDuck = get (ParamIDs::duck);
    pLowCut = get (ParamIDs::lowCut);       pHighCut = get (ParamIDs::highCut);
    pHornFreq = get (ParamIDs::hornFreq);   pHonk = get (ParamIDs::honk);
    pDrive = get (ParamIDs::drive);         pBits = get (ParamIDs::bits);
    pRate = get (ParamIDs::rate);           pPingPong = get (ParamIDs::pingPong);
    pWidth = get (ParamIDs::width);         pMix = get (ParamIDs::mix);
    pOutput = get (ParamIDs::output);
    computeDelaySeconds(); // initialise GUI read-outs before audio starts
}

juce::AudioProcessorValueTreeState::ParameterLayout HornDelayAudioProcessor::createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    auto range = [] (float lo, float hi, float step, float centre)
    {
        NormalisableRange<float> r (lo, hi, step);
        r.setSkewForCentre (centre);
        return r;
    };
    auto hz  = AudioParameterFloatAttributes().withLabel ("Hz").withStringFromValueFunction (hzToString);
    auto pct = AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (pctToString);

    StringArray divNames;
    for (auto& d : kDivisions) divNames.add (d.name);

    params.push_back (std::make_unique<AudioParameterBool>   (ParameterID { ParamIDs::sync, 1 }, "Tempo Sync", true));
    params.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::division, 1 }, "Division", divNames, kDefaultDivision));
    params.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { ParamIDs::fallbackBpm, 1 }, "Fallback BPM",
                          NormalisableRange<float> (40.0f, 240.0f, 0.1f), 90.0f,
                          AudioParameterFloatAttributes().withLabel ("BPM")
                              .withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " BPM"; })));
    params.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { ParamIDs::timeMs, 1 }, "Free Time",
                          range (1.0f, 2000.0f, 0.1f, 300.0f), 333.0f,
                          AudioParameterFloatAttributes().withLabel ("ms")
                              .withStringFromValueFunction ([] (float v, int) { return String (juce::roundToInt (v)) + " ms"; })));
    params.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { ParamIDs::throws, 1 }, "Throws",
                          NormalisableRange<float> (1.0f, 8.0f, 0.01f), 2.0f,
                          AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1); })));
    params.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { ParamIDs::duck, 1 }, "Duck",
                          NormalisableRange<float> (0.0f, 100.0f, 0.1f), 25.0f, pct));
    params.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { ParamIDs::lowCut, 1 }, "Low Cut",
                          range (80.0f, 2000.0f, 1.0f, 450.0f), 450.0f, hz));
    params.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { ParamIDs::highCut, 1 }, "High Cut",
                          range (1500.0f, 14000.0f, 1.0f, 4500.0f), 4500.0f, hz));
    params.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { ParamIDs::hornFreq, 1 }, "Horn Freq",
                          range (400.0f, 3000.0f, 1.0f, 1200.0f), 1200.0f, hz));
    params.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { ParamIDs::honk, 1 }, "Honk",
                          NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, pct));
    params.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { ParamIDs::drive, 1 }, "Drive",
                          NormalisableRange<float> (0.0f, 100.0f, 0.1f), 30.0f, pct));
    params.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { ParamIDs::bits, 1 }, "Bits",
                          NormalisableRange<float> (4.0f, 16.0f, 0.1f), 12.0f,
                          AudioParameterFloatAttributes().withLabel ("bit")
                              .withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " bit"; })));
    params.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { ParamIDs::rate, 1 }, "Sample Rate",
                          range (4000.0f, 48000.0f, 10.0f, 22050.0f), 26040.0f, hz));
    params.push_back (std::make_unique<AudioParameterBool>   (ParameterID { ParamIDs::pingPong, 1 }, "Ping-Pong", false));
    params.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { ParamIDs::width, 1 }, "Width",
                          NormalisableRange<float> (0.0f, 200.0f, 0.1f), 100.0f, pct));
    params.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { ParamIDs::mix, 1 }, "Mix",
                          NormalisableRange<float> (0.0f, 100.0f, 0.1f), 30.0f, pct));
    params.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { ParamIDs::output, 1 }, "Output",
                          NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
                          AudioParameterFloatAttributes().withLabel ("dB")
                              .withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })));

    return { params.begin(), params.end() };
}

//==============================================================================
bool HornDelayAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in  = layouts.getMainInputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return in == out || (in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo());
}

void HornDelayAudioProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    lineSize = (int) std::ceil (kMaxDelaySeconds * sr) + 8;
    for (auto& l : lines)
        l.assign ((size_t) lineSize, 0.0f);
    writePos = 0;

    currentDelay = nextDelay = juce::jlimit (3.0, (double) lineSize - 4.0, computeDelaySeconds() * sr);
    fading = false;
    fadePos = 0.0;
    fadeInc = 1.0 / (kCrossfadeSeconds * sr);

    auto initLinear = [this] (juce::SmoothedValue<float>& s, float v, double secs) { s.reset (sr, secs); s.setCurrentAndTargetValue (v); };
    initLinear (smFeedback, throwsToFeedback (pThrows->load()), 0.05);
    initLinear (smMix,      pMix->load() * 0.01f, 0.05);
    initLinear (smOutput,   juce::Decibels::decibelsToGain (pOutput->load()), 0.05);
    initLinear (smWidth,    pWidth->load() * 0.01f, 0.05);
    initLinear (smDrive,    pDrive->load() * 0.01f, 0.05);
    initLinear (smDuck,     pDuck->load() * 0.01f, 0.05);
    initLinear (smHonk,     pHonk->load() * 0.01f, 0.05);

    for (auto* s : { &smLowCut, &smHighCut, &smHornFreq }) s->reset (sr, 0.05);
    smLowCut.setCurrentAndTargetValue (pLowCut->load());
    smHighCut.setCurrentAndTargetValue (pHighCut->load());
    smHornFreq.setCurrentAndTargetValue (pHornFreq->load());

    for (int ch = 0; ch < 2; ++ch) { loopHP[(size_t) ch].reset(); loopLP[(size_t) ch].reset(); hornBell[(size_t) ch].reset(); }
    updateFilters();

    holdSample = { 0.0f, 0.0f };
    holdPhase = 1.0;
    dcX = { 0, 0 }; dcY = { 0, 0 };
    duckEnv = 0.0f;
    duckAtt = 1.0f - std::exp (-1.0f / (float) (0.005 * sr));
    duckRel = 1.0f - std::exp (-1.0f / (float) (0.250 * sr));
}

double HornDelayAudioProcessor::computeDelaySeconds()
{
    double bpm = pFallbackBpm->load();
    bool fromHost = false;

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto hostBpm = pos->getBpm())
                if (*hostBpm > 1.0 && *hostBpm < 1000.0) { bpm = *hostBpm; fromHost = true; }

    displayBpm.store (bpm);
    bpmFromHost.store (fromHost);

    double seconds;
    if (pSync->load() > 0.5f)
    {
        const int idx = juce::jlimit (0, (int) kDivisions.size() - 1, (int) pDivision->load());
        seconds = kDivisions[(size_t) idx].beats * 60.0 / bpm;
    }
    else
    {
        seconds = pTimeMs->load() * 0.001;
    }

    seconds = juce::jlimit (0.001, kMaxDelaySeconds - 0.01, seconds);
    displayDelayMs.store (seconds * 1000.0);
    return seconds;
}

void HornDelayAudioProcessor::updateFilters()
{
    const float lo   = smLowCut.getCurrentValue();
    const float hi   = juce::jmax (smHighCut.getCurrentValue(), lo * 1.5f);
    const float honk = smHonk.getCurrentValue();
    for (size_t ch = 0; ch < 2; ++ch)
    {
        loopHP[ch].setup (SimperSVF::Mode::highpass, sr, lo, 0.707f);
        loopLP[ch].setup (SimperSVF::Mode::lowpass,  sr, hi, 0.707f);
        hornBell[ch].setup (SimperSVF::Mode::bell, sr, smHornFreq.getCurrentValue(), 0.8f + 1.7f * honk, 12.0f * honk);
    }
}

float HornDelayAudioProcessor::readTap (const std::vector<float>& line, double delaySamples) const noexcept
{
    double rp = (double) writePos - delaySamples;
    while (rp < 0.0) rp += lineSize;

    const int   i1 = (int) rp;
    const float t  = (float) (rp - i1);
    const int   i0 = (i1 - 1 + lineSize) % lineSize;
    const int   i2 = (i1 + 1) % lineSize;
    const int   i3 = (i1 + 2) % lineSize;

    const float y0 = line[(size_t) i0], y1 = line[(size_t) i1], y2 = line[(size_t) i2], y3 = line[(size_t) i3];
    // 4-point Hermite interpolation
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * t + c2) * t + c1) * t + y1;
}

//==============================================================================
void HornDelayAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int n      = buffer.getNumSamples();

    for (int ch = numIn; ch < numOut; ++ch)          // mono in -> stereo out
        buffer.copyFrom (ch, 0, buffer, 0, 0, n);

    if (lineSize == 0 || n == 0)
        return;

    // ---- block-rate targets ----
    const double targetDelay = juce::jlimit (3.0, (double) lineSize - 4.0, computeDelaySeconds() * sr);
    smFeedback.setTargetValue (throwsToFeedback (pThrows->load()));
    smMix.setTargetValue (pMix->load() * 0.01f);
    smOutput.setTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
    smWidth.setTargetValue (pWidth->load() * 0.01f);
    smDrive.setTargetValue (pDrive->load() * 0.01f);
    smDuck.setTargetValue (pDuck->load() * 0.01f);
    smHonk.setTargetValue (pHonk->load() * 0.01f);
    smLowCut.setTargetValue (pLowCut->load());
    smHighCut.setTargetValue (pHighCut->load());
    smHornFreq.setTargetValue (pHornFreq->load());

    const bool  pingPong = pPingPong->load() > 0.5f;
    const float bits     = pBits->load();
    const float levels   = std::pow (2.0f, bits - 1.0f);
    const double holdInc = juce::jmin (1.0, (double) pRate->load() / sr);
    const bool  doHold   = holdInc < 0.999;

    float* outL = buffer.getWritePointer (0);
    float* outR = numOut > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < n; ++i)
    {
        // ---- control-rate updates ----
        if ((i % kControlInterval) == 0)
        {
            smLowCut.skip (kControlInterval); smHighCut.skip (kControlInterval);
            smHornFreq.skip (kControlInterval); smHonk.skip (kControlInterval);
            updateFilters();
        }

        const float g      = smFeedback.getNextValue();
        const float mix    = smMix.getNextValue();
        const float outG   = smOutput.getNextValue();
        const float width  = smWidth.getNextValue();
        const float drive  = smDrive.getNextValue();
        const float duck   = smDuck.getNextValue();

        const float inL = outL[i];
        const float inR = outR != nullptr ? outR[i] : inL;

        // ---- ducking envelope (follows the dry signal) ----
        const float lvl = juce::jmax (std::abs (inL), std::abs (inR));
        duckEnv += (lvl > duckEnv ? duckAtt : duckRel) * (lvl - duckEnv);

        // ---- crossfading read heads: click-free time changes ----
        if (! fading && std::abs (targetDelay - currentDelay) > 0.5)
        {
            nextDelay = targetDelay;
            fadePos = 0.0;
            fading = true;
        }

        float tap[2];
        for (size_t ch = 0; ch < 2; ++ch)
        {
            float v = readTap (lines[ch], currentDelay);
            if (fading)
            {
                const float f = (float) fadePos;
                v = v * (1.0f - f) + readTap (lines[ch], nextDelay) * f;
            }
            tap[ch] = v;
        }
        if (fading && (fadePos += fadeInc) >= 1.0)
        {
            currentDelay = nextDelay;
            fading = false;
        }

        // ---- feedback loop: band-limit each repeat, soft-limit, write ----
        float rep[2];
        for (size_t ch = 0; ch < 2; ++ch)
            rep[ch] = loopLP[ch].process (loopHP[ch].process (tap[ch]));

        const float fbL = g * std::tanh (rep[0]);
        const float fbR = g * std::tanh (rep[1]);
        if (pingPong)
        {
            lines[0][(size_t) writePos] = 0.5f * (inL + inR) + fbR;
            lines[1][(size_t) writePos] = fbL;
        }
        else
        {
            lines[0][(size_t) writePos] = inL + fbL;
            lines[1][(size_t) writePos] = inR + fbR;
        }
        if (++writePos >= lineSize) writePos = 0;

        // ---- wet colour: drive -> lo-fi -> horn resonance ----
        const float pre  = 1.0f + 9.0f * drive;
        const float bias = 0.2f * drive;
        const float comp = 1.0f / std::pow (pre, 0.8f);
        const float driveBlend = juce::jmin (1.0f, drive * 3.0f);

        if (doHold)
        {
            holdPhase += holdInc;
            if (holdPhase >= 1.0) holdPhase -= 1.0;
        }
        const bool takeSample = ! doHold || holdPhase < holdInc;

        float wet[2];
        for (size_t ch = 0; ch < 2; ++ch)
        {
            float x = rep[ch];
            const float sat = (std::tanh (pre * x + bias) - std::tanh (bias)) * comp;
            x += driveBlend * (sat - x);

            // DC blocker (asymmetric saturation adds offset)
            const float y = x - dcX[ch] + 0.995f * dcY[ch];
            dcX[ch] = x; dcY[ch] = y;
            x = y;

            if (takeSample)
                holdSample[ch] = std::round (x * levels) / levels;
            x = holdSample[ch];

            wet[ch] = hornBell[ch].process (x);
        }

        // ---- stereo width (M/S) ----
        const float mid  = 0.5f * (wet[0] + wet[1]);
        const float side = 0.5f * (wet[0] - wet[1]) * width;
        float wL = mid + side, wR = mid - side;

        // ---- duck + mix + output ----
        const float duckGain = 1.0f - duck * juce::jlimit (0.0f, 1.0f, duckEnv * 4.0f) * 0.9f;
        const float dryG = juce::jmin (1.0f, 2.0f * (1.0f - mix));
        const float wetG = juce::jmin (1.0f, 2.0f * mix) * duckGain;

        wL = (inL * dryG + wL * wetG) * outG;
        wR = (inR * dryG + wR * wetG) * outG;

        if (outR != nullptr) { outL[i] = wL; outR[i] = wR; }
        else                 { outL[i] = 0.5f * (wL + wR); }
    }
}

//==============================================================================
const std::vector<HornDelayAudioProcessor::Preset>& HornDelayAudioProcessor::getPresets()
{
    using namespace ParamIDs;
    // Division indices: 0=1/16 1=1/8T 2=1/8 3=1/8D 4=1/4T 5=1/4 6=3/8 7=1/2 8=1/2D 9=1 Bar
    static const std::vector<Preset> presets {
        { "Classic Throw",      { } },
        { "Pete Rock Stab",     { { division, 3 }, { throws, 2.0f }, { lowCut, 520 }, { highCut, 4200 }, { hornFreq, 1400 },
                                  { honk, 60 }, { drive, 35 }, { bits, 12 }, { rate, 26040 }, { width, 120 },
                                  { mix, 35 }, { duck, 40 } } },
        { "Bad Boy Brass",      { { division, 5 }, { throws, 3.0f }, { lowCut, 350 }, { highCut, 6500 }, { hornFreq, 1100 },
                                  { honk, 45 }, { drive, 50 }, { bits, 16 }, { rate, 48000 }, { pingPong, 1 },
                                  { width, 140 }, { mix, 30 }, { duck, 30 } } },
        { "Dusty Throw",        { { division, 6 }, { throws, 3.0f }, { lowCut, 600 }, { highCut, 3200 }, { hornFreq, 900 },
                                  { honk, 70 }, { drive, 60 }, { bits, 10 }, { rate, 22050 }, { width, 90 },
                                  { mix, 40 }, { duck, 50 } } },
        { "Large Pro Slap",     { { division, 2 }, { throws, 1.0f }, { lowCut, 450 }, { highCut, 5000 }, { hornFreq, 1600 },
                                  { honk, 50 }, { drive, 25 }, { bits, 12 }, { rate, 26040 }, { mix, 35 }, { duck, 20 } } },
        { "Ping-Pong Horns",    { { division, 3 }, { throws, 4.0f }, { lowCut, 400 }, { highCut, 5500 }, { hornFreq, 1250 },
                                  { honk, 40 }, { drive, 30 }, { bits, 13 }, { rate, 32000 }, { pingPong, 1 },
                                  { width, 160 }, { mix, 30 }, { duck, 35 } } },
        { "Half-Time Fade",     { { division, 7 }, { throws, 2.5f }, { lowCut, 500 }, { highCut, 3800 }, { hornFreq, 1000 },
                                  { honk, 55 }, { drive, 40 }, { bits, 11 }, { rate, 24000 }, { mix, 32 }, { duck, 45 } } },
    };
    return presets;
}

int HornDelayAudioProcessor::getNumPrograms() { return (int) getPresets().size(); }

const juce::String HornDelayAudioProcessor::getProgramName (int index)
{
    const auto& p = getPresets();
    return juce::isPositiveAndBelow (index, (int) p.size()) ? juce::String (p[(size_t) index].name) : juce::String();
}

void HornDelayAudioProcessor::setCurrentProgram (int index)
{
    const auto& presets = getPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;

    currentProgram = index;

    // Start from defaults, then apply the preset's overrides.
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());

    for (const auto& [id, value] : presets[(size_t) index].values)
        if (auto* rp = apvts.getParameter (id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (value));
}

//==============================================================================
void HornDelayAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void HornDelayAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = tree.getProperty ("program", 0);
            apvts.replaceState (tree);
        }
}

juce::AudioProcessorEditor* HornDelayAudioProcessor::createEditor()
{
    return new HornDelayAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HornDelayAudioProcessor();
}
