#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "NitidezEngine.h"

class NitidezProcessor : public juce::AudioProcessor
{
public:
    NitidezProcessor();
    ~NitidezProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Nitidez"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // Lidos pela interface (30x por segundo)
    std::atomic<float> meterVoice { 0.0f };
    std::atomic<float> meterReduction { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    nitidez::Engine engine;

    std::atomic<float>* clarityParam = nullptr;
    std::atomic<float>* hushParam = nullptr;
    std::atomic<float>* inputParam = nullptr;
    std::atomic<float>* outputParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NitidezProcessor)
};
