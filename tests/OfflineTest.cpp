// Offline test for Horn Delay: renders audio through the processor without a DAW.
// Exit code 0 = all checks passed.
#include <juce_audio_formats/juce_audio_formats.h>
#include "../Source/PluginProcessor.h"
#include <cstdio>

namespace
{
    int failures = 0;

    void check (bool ok, const juce::String& what)
    {
        std::printf ("[%s] %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8());
        if (! ok) ++failures;
    }

    void setParam (HornDelayAudioProcessor& p, const char* id, float value)
    {
        auto* param = p.apvts.getParameter (id);
        param->setValueNotifyingHost (param->convertTo0to1 (value));
    }

    struct FixedTempoPlayHead : juce::AudioPlayHead
    {
        double bpm = 120.0;
        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo info;
            info.setBpm (bpm);
            info.setIsPlaying (true);
            return info;
        }
    };

    /** Clean, measurable settings: 100 % wet, no lo-fi, no duck, no honk. */
    void setCleanWet (HornDelayAudioProcessor& p)
    {
        setParam (p, ParamIDs::mix, 100.0f);
        setParam (p, ParamIDs::duck, 0.0f);
        setParam (p, ParamIDs::honk, 0.0f);
        setParam (p, ParamIDs::drive, 0.0f);
        setParam (p, ParamIDs::bits, 16.0f);
        setParam (p, ParamIDs::rate, 48000.0f);
        setParam (p, ParamIDs::lowCut, 80.0f);
        setParam (p, ParamIDs::highCut, 14000.0f);
        setParam (p, ParamIDs::width, 100.0f);
        setParam (p, ParamIDs::pingPong, 0.0f);
    }

    /** Process a mono signal (copied to both channels) in host-sized blocks. */
    juce::AudioBuffer<float> render (HornDelayAudioProcessor& p, const std::vector<float>& input, int blockSize = 512)
    {
        juce::AudioBuffer<float> out (2, (int) input.size());
        juce::MidiBuffer midi;
        for (int start = 0; start < (int) input.size(); start += blockSize)
        {
            const int n = juce::jmin (blockSize, (int) input.size() - start);
            juce::AudioBuffer<float> block (2, n);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < n; ++i)
                    block.setSample (ch, i, input[(size_t) (start + i)]);
            p.processBlock (block, midi);
            for (int ch = 0; ch < 2; ++ch)
                out.copyFrom (ch, start, block, ch, 0, n);
        }
        return out;
    }

    int peakIndex (const juce::AudioBuffer<float>& b, int from, int to)
    {
        int best = from; float bestV = 0.0f;
        for (int i = from; i < to; ++i)
            if (std::abs (b.getSample (0, i)) > bestV) { bestV = std::abs (b.getSample (0, i)); best = i; }
        return best;
    }

    bool allFinite (const juce::AudioBuffer<float>& b)
    {
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int i = 0; i < b.getNumSamples(); ++i)
                if (! std::isfinite (b.getSample (ch, i))) return false;
        return true;
    }

    float rangePeak (const juce::AudioBuffer<float>& b, int from, int to)
    {
        float m = 0.0f;
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            m = juce::jmax (m, b.getMagnitude (ch, from, to - from));
        return m;
    }

    /** Synthetic "horn stab": detuned saw chord with a fast brassy envelope. */
    std::vector<float> makeHornStabs (double sr, double bpm, int bars)
    {
        const int total = (int) (sr * 60.0 / bpm * 4 * bars) + (int) sr * 3;
        std::vector<float> x ((size_t) total, 0.0f);
        const double freqs[] = { 233.08, 293.66, 349.23, 466.16 }; // Bb major-ish voicing
        const int beatLen = (int) (sr * 60.0 / bpm);
        for (int bar = 0; bar < bars; ++bar)
        {
            for (int hit : { 0, 3 })   // stab on beat 1 and the "and" of 2
            {
                const int start = bar * 4 * beatLen + hit * beatLen / 2 * (hit == 0 ? 0 : 1);
                const int len = (int) (0.22 * sr);
                for (int i = 0; i < len && start + i < total; ++i)
                {
                    const double t = i / sr;
                    const double env = std::min (1.0, t / 0.008) * std::exp (-t * 9.0);
                    double s = 0.0;
                    for (double f : freqs)
                        for (double det : { 0.997, 1.003 })
                            s += 2.0 * std::fmod (t * f * det, 1.0) - 1.0;
                    x[(size_t) (start + i)] += (float) (0.08 * env * s);
                }
            }
        }
        return x;
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    constexpr double sr = 48000.0;

    // ---- 1. Fallback BPM (no host): 1/4 @ 90 BPM = 666.67 ms ----
    {
        HornDelayAudioProcessor p;
        setCleanWet (p);
        setParam (p, ParamIDs::division, 5.0f);
        setParam (p, ParamIDs::throws, 2.0f);
        p.prepareToPlay (sr, 512);

        std::vector<float> in ((size_t) (sr * 2), 0.0f);
        in[0] = 1.0f;
        auto out = render (p, in);
        const int expected = (int) std::round (sr * 60.0 / 90.0);
        const int first = peakIndex (out, 100, expected + 4000);
        const int second = peakIndex (out, expected + 4000, juce::jmin (out.getNumSamples(), 2 * expected + 4000));
        const float a1 = std::abs (out.getSample (0, first)), a2 = std::abs (out.getSample (0, second));
        check (std::abs (first - expected) < 60, "fallback 90 BPM 1/4 echo at " + juce::String (first) + " (expected ~" + juce::String (expected) + ")");
        check (std::abs (second - 2 * expected) < 120, "second throw at " + juce::String (second));
        check (a2 < a1 && a2 > a1 * 0.05f, "second throw quieter than first (" + juce::String (a1, 3) + " -> " + juce::String (a2, 3) + ")");
        check (allFinite (out), "output finite");
    }

    // ---- 2. Host tempo sync: 1/8 dotted @ 120 BPM = 375 ms ----
    {
        HornDelayAudioProcessor p;
        FixedTempoPlayHead ph; ph.bpm = 120.0;
        p.setPlayHead (&ph);
        setCleanWet (p);
        setParam (p, ParamIDs::division, 3.0f);
        p.prepareToPlay (sr, 256);

        std::vector<float> in ((size_t) sr, 0.0f);
        in[0] = 1.0f;
        auto out = render (p, in, 256);
        const int expected = (int) std::round (0.375 * sr);
        const int first = peakIndex (out, 100, expected + 3000);
        check (std::abs (first - expected) < 60, "host 120 BPM 1/8 dotted echo at " + juce::String (first) + " (expected ~" + juce::String (expected) + ")");
        check (p.bpmFromHost.load() && std::abs (p.displayBpm.load() - 120.0) < 1e-6, "processor reports host BPM");
        p.setPlayHead (nullptr);
    }

    // ---- 3. Free ms mode ----
    {
        HornDelayAudioProcessor p;
        setCleanWet (p);
        setParam (p, ParamIDs::sync, 0.0f);
        setParam (p, ParamIDs::timeMs, 250.0f);
        p.prepareToPlay (sr, 512);
        std::vector<float> in ((size_t) sr, 0.0f);
        in[0] = 1.0f;
        auto out = render (p, in);
        const int first = peakIndex (out, 100, (int) (0.4 * sr));
        check (std::abs (first - (int) (0.25 * sr)) < 60, "free mode 250 ms echo at " + juce::String (first));
    }

    // ---- 4. Stability torture: max throws/drive/honk, loud noise, then silence ----
    {
        HornDelayAudioProcessor p;
        setParam (p, ParamIDs::throws, 8.0f);
        setParam (p, ParamIDs::drive, 100.0f);
        setParam (p, ParamIDs::honk, 100.0f);
        setParam (p, ParamIDs::mix, 100.0f);
        setParam (p, ParamIDs::pingPong, 1.0f);
        setParam (p, ParamIDs::width, 200.0f);
        setParam (p, ParamIDs::division, 0.0f); // 1/16 = many loop passes
        p.prepareToPlay (sr, 512);

        juce::Random rng (42);
        std::vector<float> in ((size_t) (sr * 12), 0.0f);
        for (size_t i = 0; i < (size_t) (sr * 4); ++i)
            in[i] = rng.nextFloat() * 2.0f - 1.0f;
        auto out = render (p, in);
        const float peak = rangePeak (out, 0, out.getNumSamples());
        const float tail = rangePeak (out, (int) (sr * 10), out.getNumSamples());
        check (allFinite (out), "torture output finite");
        check (peak < 8.0f, "torture peak bounded (" + juce::String (peak, 2) + ")");
        check (tail < 1.0e-3f, "tail decays below -60 dB after input stops (" + juce::String (juce::Decibels::gainToDecibels (tail), 1) + " dB)");
    }

    // ---- 5. Click test: switch division while a sine plays ----
    {
        HornDelayAudioProcessor p;
        setCleanWet (p);
        setParam (p, ParamIDs::mix, 50.0f);
        p.prepareToPlay (sr, 512);

        const int len = (int) (sr * 3);
        std::vector<float> in ((size_t) len);
        for (int i = 0; i < len; ++i)
            in[(size_t) i] = 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * i / sr);

        juce::AudioBuffer<float> out (2, len);
        juce::MidiBuffer midi;
        for (int start = 0; start < len; start += 512)
        {
            if (start == (int) (sr * 1.5)) setParam (p, ParamIDs::division, 9.0f);  // 1/4 -> 1 bar
            if (start == (int) (sr * 2.0) / 512 * 512) setParam (p, ParamIDs::division, 0.0f); // -> 1/16
            const int n = juce::jmin (512, len - start);
            juce::AudioBuffer<float> b (2, n);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < n; ++i) b.setSample (ch, i, in[(size_t) (start + i)]);
            p.processBlock (b, midi);
            for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, start, b, ch, 0, n);
        }
        float maxStep = 0.0f;
        for (int i = (int) (sr * 1.0); i < len; ++i)
            maxStep = juce::jmax (maxStep, std::abs (out.getSample (0, i) - out.getSample (0, i - 1)));
        // A 440 Hz sine at amplitude ~1 changes by at most ~0.058 per sample; a click would be far larger.
        check (maxStep < 0.2f, "no clicks on division change (max step " + juce::String (maxStep, 4) + ")");
    }

    // ---- 6. Presets + state save/recall ----
    {
        HornDelayAudioProcessor p;
        check (p.getNumPrograms() >= 3, juce::String (p.getNumPrograms()) + " presets available");
        bool namesOk = false;
        for (int i = 0; i < p.getNumPrograms(); ++i)
            if (p.getProgramName (i) == "Pete Rock Stab") namesOk = true;
        check (namesOk, "'Pete Rock Stab' preset present");

        p.setCurrentProgram (3); // Dusty Throw
        const float bits = p.apvts.getRawParameterValue (ParamIDs::bits)->load();
        check (std::abs (bits - 10.0f) < 0.05f, "preset 'Dusty Throw' sets bits = 10 (got " + juce::String (bits) + ")");

        juce::MemoryBlock state;
        p.getStateInformation (state);

        HornDelayAudioProcessor q;
        q.setStateInformation (state.getData(), (int) state.getSize());
        bool same = true;
        for (auto* id : { ParamIDs::bits, ParamIDs::division, ParamIDs::lowCut, ParamIDs::mix, ParamIDs::throws })
            same &= std::abs (p.apvts.getRawParameterValue (id)->load() - q.apvts.getRawParameterValue (id)->load()) < 1e-4f;
        check (same && q.getCurrentProgram() == 3, "state save/recall round-trip");
    }

    // ---- 7. Demo render: horn stabs through every preset -> WAV files ----
    {
        juce::File dir = juce::File::getCurrentWorkingDirectory().getChildFile ("demo_renders");
        dir.createDirectory();
        const auto dry = makeHornStabs (sr, 92.0, 2);
        juce::WavAudioFormat wav;

        auto writeWav = [&] (const juce::File& f, const juce::AudioBuffer<float>& b)
        {
            f.deleteFile();
            std::unique_ptr<juce::OutputStream> os (f.createOutputStream());
            if (auto* w = wav.createWriterFor (os.get(), sr, 2, 24, {}, 0))
            {
                os.release();
                std::unique_ptr<juce::AudioFormatWriter> writer (w);
                writer->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
            }
        };

        {
            juce::AudioBuffer<float> d (2, (int) dry.size());
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < (int) dry.size(); ++i) d.setSample (ch, i, dry[(size_t) i]);
            writeWav (dir.getChildFile ("00_dry.wav"), d);
        }

        HornDelayAudioProcessor names;
        for (int prog = 0; prog < names.getNumPrograms(); ++prog)
        {
            HornDelayAudioProcessor p;
            FixedTempoPlayHead ph; ph.bpm = 92.0;
            p.setPlayHead (&ph);
            p.setCurrentProgram (prog);
            p.prepareToPlay (sr, 512);
            auto out = render (p, dry);
            p.setPlayHead (nullptr);
            check (allFinite (out) && rangePeak (out, 0, out.getNumSamples()) < 4.0f,
                   "render preset '" + p.getProgramName (prog) + "'");
            writeWav (dir.getChildFile (juce::String (prog + 1).paddedLeft ('0', 2) + "_"
                                        + p.getProgramName (prog).replaceCharacter (' ', '_') + ".wav"), out);
        }
        std::printf ("Demo WAVs written to %s\n", dir.getFullPathName().toRawUTF8());
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
