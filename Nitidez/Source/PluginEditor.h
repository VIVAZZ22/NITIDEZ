#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class NitidezLookAndFeel : public juce::LookAndFeel_V4
{
public:
    NitidezLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;
};

class NitidezEditor : public juce::AudioProcessorEditor,
                      private juce::Timer
{
public:
    explicit NitidezEditor (NitidezProcessor&);
    ~NitidezEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setupKnob (juce::Slider&, juce::Label&, const juce::String& labelText,
                    const juce::String& suffix, int decimals, juce::Colour accent);
    void drawMeter (juce::Graphics&, juce::Rectangle<float> area, const juce::String& title,
                    float value01, const juce::String& valueText, juce::Colour colour);

    NitidezProcessor& processor;
    NitidezLookAndFeel lookAndFeel;

    juce::Slider clarityKnob, hushKnob, inputKnob, outputKnob;
    juce::Label clarityLabel, hushLabel, inputLabel, outputLabel;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> clarityAtt, hushAtt, inputAtt, outputAtt;

    juce::Rectangle<float> voiceMeterArea, reductionMeterArea;
    float shownVoice = 0.0f, shownReduction = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NitidezEditor)
};
