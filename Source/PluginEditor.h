#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace HornColours
{
    const juce::Colour background { 0xff15161a };
    const juce::Colour panel      { 0xff1f2127 };
    const juce::Colour outline    { 0xff2e3139 };
    const juce::Colour brass      { 0xffe0a83a };
    const juce::Colour brassDim   { 0xff7a5d22 };
    const juce::Colour text       { 0xffe8e3d6 };
    const juce::Colour textDim    { 0xff8b8a86 };
}

class HornLookAndFeel : public juce::LookAndFeel_V4
{
public:
    HornLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

/** A rotary knob with a caption, bound to an APVTS parameter. */
struct LabelledKnob : public juce::Component
{
    LabelledKnob (juce::AudioProcessorValueTreeState& state, const juce::String& paramID, const juce::String& caption);
    void resized() override;

    juce::Slider slider;
    juce::Label  label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class HornDelayAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit HornDelayAudioProcessorEditor (HornDelayAudioProcessor&);
    ~HornDelayAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void drawSection (juce::Graphics&, juce::Rectangle<int> area, const juce::String& title);

    HornDelayAudioProcessor& processor;
    HornLookAndFeel lnf;

    juce::ComboBox presetBox, divisionBox;
    juce::ToggleButton syncButton { "Tempo Sync" }, pingPongButton { "Ping-Pong" };
    juce::Label bpmLabel, divisionCaption;

    LabelledKnob timeKnob, fallbackKnob, throwsKnob, duckKnob;
    LabelledKnob lowCutKnob, highCutKnob, hornFreqKnob, honkKnob;
    LabelledKnob driveKnob, bitsKnob, rateKnob;
    LabelledKnob widthKnob, mixKnob, outputKnob;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> divisionAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttachment, pingPongAttachment;

    juce::Rectangle<int> timeArea, hornArea, lofiArea, outArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HornDelayAudioProcessorEditor)
};
