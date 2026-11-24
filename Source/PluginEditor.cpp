#include "PluginEditor.h"

using namespace HornColours;

//==============================================================================
HornLookAndFeel::HornLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, background);
    setColour (juce::Label::textColourId, text);
    setColour (juce::ComboBox::backgroundColourId, background);
    setColour (juce::ComboBox::outlineColourId, outline);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::arrowColourId, brass);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, brassDim);
    setColour (juce::PopupMenu::highlightedTextColourId, text);
    setColour (juce::ToggleButton::textColourId, text);
}

void HornLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float track = juce::jmax (2.5f, radius * 0.12f);
    const bool enabled = s.isEnabled();

    // Arc track + value arc
    juce::Path bg, val;
    bg.addCentredArc (centre.x, centre.y, radius - track, radius - track, 0.0f, startAngle, endAngle, true);
    val.addCentredArc (centre.x, centre.y, radius - track, radius - track, 0.0f, startAngle, angle, true);
    const juce::PathStrokeType stroke (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour (outline);
    g.strokePath (bg, stroke);
    g.setColour (enabled ? brass : brassDim.withAlpha (0.5f));
    g.strokePath (val, stroke);

    // Knob body
    const float body = radius - track * 2.2f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3d46), centre.x, centre.y - body,
                                             juce::Colour (0xff1b1c21), centre.x, centre.y + body, false));
    g.fillEllipse (centre.x - body, centre.y - body, body * 2.0f, body * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (centre.x - body, centre.y - body, body * 2.0f, body * 2.0f, 1.0f);

    // Pointer
    juce::Path p;
    p.addRoundedRectangle (-1.5f, -body + 3.0f, 3.0f, body * 0.5f, 1.5f);
    g.setColour (enabled ? text : textDim);
    g.fillPath (p, juce::AffineTransform::rotation (angle).translated (centre));
}

void HornLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? brass : background);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (highlighted ? brass : outline);
    g.drawRoundedRectangle (r, 5.0f, 1.2f);
    g.setColour (on ? background : text);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, 1);
}

//==============================================================================
LabelledKnob::LabelledKnob (juce::AudioProcessorValueTreeState& state, const juce::String& paramID, const juce::String& caption)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, background);
    slider.setColour (juce::Slider::textBoxTextColourId, text);
    addAndMakeVisible (slider);

    label.setText (caption.toUpperCase(), juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    label.setColour (juce::Label::textColourId, textDim);
    addAndMakeVisible (label);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramID, slider);
}

void LabelledKnob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (16));
    slider.setBounds (r);
}

//==============================================================================
HornDelayAudioProcessorEditor::HornDelayAudioProcessorEditor (HornDelayAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p),
      timeKnob     (p.apvts, ParamIDs::timeMs,      "Free Time"),
      fallbackKnob (p.apvts, ParamIDs::fallbackBpm, "Fallback BPM"),
      throwsKnob   (p.apvts, ParamIDs::throws,      "Throws"),
      duckKnob     (p.apvts, ParamIDs::duck,        "Duck"),
      lowCutKnob   (p.apvts, ParamIDs::lowCut,      "Low Cut"),
      highCutKnob  (p.apvts, ParamIDs::highCut,     "High Cut"),
      hornFreqKnob (p.apvts, ParamIDs::hornFreq,    "Horn Freq"),
      honkKnob     (p.apvts, ParamIDs::honk,        "Honk"),
      driveKnob    (p.apvts, ParamIDs::drive,       "Drive"),
      bitsKnob     (p.apvts, ParamIDs::bits,        "Bits"),
      rateKnob     (p.apvts, ParamIDs::rate,        "Rate"),
      widthKnob    (p.apvts, ParamIDs::width,       "Width"),
      mixKnob      (p.apvts, ParamIDs::mix,         "Mix"),
      outputKnob   (p.apvts, ParamIDs::output,      "Output")
{
    setLookAndFeel (&lnf);

    for (auto* k : { &timeKnob, &fallbackKnob, &throwsKnob, &duckKnob, &lowCutKnob, &highCutKnob, &hornFreqKnob,
                     &honkKnob, &driveKnob, &bitsKnob, &rateKnob, &widthKnob, &mixKnob, &outputKnob })
        addAndMakeVisible (k);

    // Presets
    for (int i = 0; i < processor.getNumPrograms(); ++i)
        presetBox.addItem (processor.getProgramName (i), i + 1);
    presetBox.setSelectedId (processor.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { processor.setCurrentProgram (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);

    // Division selector (item order must match the parameter's choices)
    for (size_t i = 0; i < kDivisions.size(); ++i)
        divisionBox.addItem (kDivisions[i].name, (int) i + 1);
    addAndMakeVisible (divisionBox);
    divisionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, ParamIDs::division, divisionBox);

    divisionCaption.setText ("DIVISION", juce::dontSendNotification);
    divisionCaption.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    divisionCaption.setColour (juce::Label::textColourId, textDim);
    divisionCaption.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (divisionCaption);

    addAndMakeVisible (syncButton);
    addAndMakeVisible (pingPongButton);
    syncAttachment     = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, ParamIDs::sync, syncButton);
    pingPongAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, ParamIDs::pingPong, pingPongButton);

    bpmLabel.setFont (juce::FontOptions (13.0f));
    bpmLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (bpmLabel);

    setSize (860, 470);
    startTimerHz (15);
    timerCallback();
}

HornDelayAudioProcessorEditor::~HornDelayAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void HornDelayAudioProcessorEditor::timerCallback()
{
    const bool sync = syncButton.getToggleState();
    const double bpm = processor.displayBpm.load();
    const bool host = processor.bpmFromHost.load();

    juce::String s;
    if (sync)
        s << (host ? juce::String ("Host ") + juce::String (bpm, 1)
                   : juce::String ("Fallback ") + juce::String (processor.apvts.getRawParameterValue (ParamIDs::fallbackBpm)->load(), 1))
          << " BPM  |  ";
    else
        s << "Free mode  |  ";
    // Compute from current parameters so the read-out also updates when audio isn't running.
    double ms = processor.apvts.getRawParameterValue (ParamIDs::timeMs)->load();
    if (sync)
    {
        const int idx = juce::jlimit (0, (int) kDivisions.size() - 1,
                                      (int) processor.apvts.getRawParameterValue (ParamIDs::division)->load());
        const double fallback = processor.apvts.getRawParameterValue (ParamIDs::fallbackBpm)->load();
        ms = kDivisions[(size_t) idx].beats * 60000.0 / (host ? bpm : fallback);
    }
    s << juce::String (ms, 1) << " ms";
    bpmLabel.setText (s, juce::dontSendNotification);
    bpmLabel.setColour (juce::Label::textColourId, host || ! sync ? brass : textDim);

    divisionBox.setEnabled (sync);
    fallbackKnob.setEnabled (sync);
    timeKnob.setEnabled (! sync);

    if (presetBox.getSelectedId() != processor.getCurrentProgram() + 1)
        presetBox.setSelectedId (processor.getCurrentProgram() + 1, juce::dontSendNotification);
}

void HornDelayAudioProcessorEditor::drawSection (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title)
{
    g.setColour (panel);
    g.fillRoundedRectangle (area.toFloat(), 8.0f);
    g.setColour (outline);
    g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 8.0f, 1.0f);
    g.setColour (brass);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (title, area.reduced (12, 6).removeFromTop (18), juce::Justification::centredLeft);
}

void HornDelayAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (background);

    // Header
    g.setColour (brass);
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawText ("HORN DELAY", 20, 12, 220, 34, juce::Justification::centredLeft);
    g.setColour (textDim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("90s hip-hop horn throws", 22, 42, 220, 16, juce::Justification::centredLeft);

    drawSection (g, timeArea, "TIME");
    drawSection (g, hornArea, "HORN TONE");
    drawSection (g, lofiArea, "LO-FI");
    drawSection (g, outArea,  "OUTPUT");
}

void HornDelayAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced (14);

    // Header row
    auto header = r.removeFromTop (52);
    header.removeFromLeft (240);
    presetBox.setBounds (header.removeFromLeft (200).withSizeKeepingCentre (200, 28));
    header.removeFromLeft (12);
    bpmLabel.setBounds (header);
    r.removeFromTop (8);

    constexpr int knobW = 92, knobH = 112, gap = 10;
    auto layoutKnobs = [] (juce::Rectangle<int> area, std::initializer_list<LabelledKnob*> knobs)
    {
        auto row = area.reduced (8).withTrimmedTop (22);
        const int w = row.getWidth() / (int) knobs.size();
        for (auto* k : knobs)
            k->setBounds (row.removeFromLeft (w).withSizeKeepingCentre (juce::jmin (w, knobW), juce::jmin (row.getHeight(), knobH)));
    };

    // Top row: TIME (wide) + HORN TONE
    auto top = r.removeFromTop ((r.getHeight() - gap) / 2);
    r.removeFromTop (gap);
    timeArea = top.removeFromLeft (top.getWidth() / 2 + 40);
    top.removeFromLeft (gap);
    hornArea = top;

    // TIME: sync + division controls on the left, knobs on the right
    {
        auto a = timeArea.reduced (12).withTrimmedTop (22);
        auto ctrl = a.removeFromLeft (120);
        syncButton.setBounds (ctrl.removeFromTop (30));
        ctrl.removeFromTop (10);
        divisionCaption.setBounds (ctrl.removeFromTop (16));
        divisionBox.setBounds (ctrl.removeFromTop (28));
        auto knobs = timeArea.withTrimmedLeft (132);
        layoutKnobs (knobs, { &fallbackKnob, &timeKnob, &throwsKnob, &duckKnob });
    }
    layoutKnobs (hornArea, { &lowCutKnob, &highCutKnob, &hornFreqKnob, &honkKnob });

    // Bottom row: LO-FI + OUTPUT
    lofiArea = r.removeFromLeft (r.getWidth() / 2 - 20);
    r.removeFromLeft (gap);
    outArea = r;
    layoutKnobs (lofiArea, { &driveKnob, &bitsKnob, &rateKnob });

    auto outCtrl = outArea.reduced (12).withTrimmedTop (22).removeFromLeft (110);
    pingPongButton.setBounds (outCtrl.withSizeKeepingCentre (110, 30));
    layoutKnobs (outArea.withTrimmedLeft (122), { &widthKnob, &mixKnob, &outputKnob });
}
