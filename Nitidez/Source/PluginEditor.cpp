#include "PluginEditor.h"

namespace
{
    const juce::Colour kBgTop     { 0xff111a26 };
    const juce::Colour kBgBottom  { 0xff0a1017 };
    const juce::Colour kAccent    { 0xff3de0c5 };
    const juce::Colour kAccent2   { 0xff7aa7ff };
    const juce::Colour kTrack     { 0xff233142 };
    const juce::Colour kText      { 0xffe6edf5 };
    const juce::Colour kTextDim   { 0xff8696a8 };

    juce::String saidaText()    { return juce::String::fromUTF8 ("Sa\xc3\xad" "da"); }
    juce::String reducaoText()  { return juce::String::fromUTF8 ("REDU\xc3\x87" "\xc3\x83" "O"); }
}

//==============================================================================
NitidezLookAndFeel::NitidezLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, kText);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, kAccent.withAlpha (0.35f));
    setColour (juce::Label::textColourId, kText);
}

void NitidezLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPos, float startAngle, float endAngle,
                                           juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    const float thickness = juce::jmax (3.0f, radius * 0.11f);
    const float arcRadius = radius - thickness * 0.5f;
    const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);

    // trilha
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (kTrack);
    g.strokePath (track, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // valor
    if (sliderPos > 0.001f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, angle, true);
        g.setColour (accent.withAlpha (0.25f));
        g.strokePath (value, juce::PathStrokeType (thickness * 1.9f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (accent);
        g.strokePath (value, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // corpo do botao
    const float bodyRadius = radius - thickness * 1.9f;
    juce::ColourGradient body (juce::Colour (0xff324559), centre.x, centre.y - bodyRadius,
                               juce::Colour (0xff141d28), centre.x, centre.y + bodyRadius, false);
    g.setGradientFill (body);
    g.fillEllipse (centre.x - bodyRadius, centre.y - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawEllipse (centre.x - bodyRadius, centre.y - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f, 1.2f);

    // ponteiro
    juce::Path pointer;
    const float pw = juce::jmax (2.5f, bodyRadius * 0.06f);
    pointer.addRoundedRectangle (-pw * 0.5f, -bodyRadius * 0.86f, pw, bodyRadius * 0.38f, pw * 0.5f);
    g.setColour (accent);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
}

//==============================================================================
NitidezEditor::NitidezEditor (NitidezProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lookAndFeel);

    setupKnob (clarityKnob, clarityLabel, "NITIDEZ", " %", 0, kAccent);
    setupKnob (hushKnob,    hushLabel,    "HUSH",    " dB", 1, kAccent2);
    setupKnob (inputKnob,   inputLabel,   "ENTRADA", " dB", 1, kAccent2);
    setupKnob (outputKnob,  outputLabel,  saidaText().toUpperCase(), " dB", 1, kAccent2);

    clarityAtt = std::make_unique<Attachment> (processor.apvts, "clarity", clarityKnob);
    hushAtt    = std::make_unique<Attachment> (processor.apvts, "hush", hushKnob);
    inputAtt   = std::make_unique<Attachment> (processor.apvts, "input", inputKnob);
    outputAtt  = std::make_unique<Attachment> (processor.apvts, "output", outputKnob);

    setSize (520, 340);
    startTimerHz (30);
}

NitidezEditor::~NitidezEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void NitidezEditor::setupKnob (juce::Slider& s, juce::Label& l, const juce::String& labelText,
                               const juce::String& suffix, int decimals, juce::Colour accent)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 22);
    s.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    s.setTextValueSuffix (suffix);
    s.setNumDecimalPlacesToDisplay (decimals);
    s.setColour (juce::Slider::rotarySliderFillColourId, accent);
    s.setPopupDisplayEnabled (false, false, nullptr);
    addAndMakeVisible (s);

    l.setText (labelText, juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centred);
    l.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    l.setColour (juce::Label::textColourId, kTextDim);
    l.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (l);
}

void NitidezEditor::timerCallback()
{
    const float v = processor.meterVoice.load();
    const float r = processor.meterReduction.load();

    // subida rapida, descida suave
    shownVoice += (v - shownVoice) * (v > shownVoice ? 0.6f : 0.15f);
    shownReduction += (r - shownReduction) * 0.25f;

    repaint (voiceMeterArea.getSmallestIntegerContainer().expanded (4));
    repaint (reductionMeterArea.getSmallestIntegerContainer().expanded (4));
}

void NitidezEditor::drawMeter (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& title,
                               float value01, const juce::String& valueText, juce::Colour colour)
{
    g.setColour (kTextDim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText (title, area.withHeight (14.0f).toNearestInt(), juce::Justification::centredLeft);
    g.setColour (kText);
    g.drawText (valueText, area.withHeight (14.0f).toNearestInt(), juce::Justification::centredRight);

    auto bar = area.withTrimmedTop (18.0f).withHeight (10.0f);
    g.setColour (kTrack);
    g.fillRoundedRectangle (bar, 5.0f);

    const float w = bar.getWidth() * juce::jlimit (0.0f, 1.0f, value01);
    if (w > 1.0f)
    {
        g.setColour (colour);
        g.fillRoundedRectangle (bar.withWidth (w), 5.0f);
    }
}

void NitidezEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (kBgTop, 0.0f, 0.0f, kBgBottom, 0.0f, (float) getHeight(), false));
    g.fillAll();

    // titulo
    g.setColour (kText);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("NITIDEZ", 24, 14, 200, 30, juce::Justification::centredLeft);
    g.setColour (kTextDim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("limpeza de voz", 26, 40, 200, 16, juce::Justification::centredLeft);

    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawLine (24.0f, 62.0f, (float) getWidth() - 24.0f, 62.0f);

    // medidores
    drawMeter (g, voiceMeterArea, "VOZ", shownVoice,
               shownVoice > 0.5f ? "detectada" : "--", kAccent);
    drawMeter (g, reductionMeterArea, reducaoText(), shownReduction / 30.0f,
               "-" + juce::String (juce::roundToInt (shownReduction)) + " dB", kAccent2);
}

void NitidezEditor::resized()
{
    // botao principal
    clarityKnob.setBounds (22, 70, 240, 262);
    clarityLabel.setBounds (22, 70, 240, 16);
    clarityKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 24);

    // coluna da direita
    const int rx = 284;
    const int rw = getWidth() - rx - 24;

    voiceMeterArea = { (float) rx, 78.0f, (float) rw, 30.0f };
    reductionMeterArea = { (float) rx, 120.0f, (float) rw, 30.0f };

    const int kw = rw / 3;
    const int ky = 176;
    juce::Slider* knobs[] = { &hushKnob, &inputKnob, &outputKnob };
    juce::Label* labels[] = { &hushLabel, &inputLabel, &outputLabel };

    for (int i = 0; i < 3; ++i)
    {
        knobs[i]->setBounds (rx + i * kw, ky + 16, kw, 124);
        labels[i]->setBounds (rx + i * kw, ky, kw, 16);
    }
}
