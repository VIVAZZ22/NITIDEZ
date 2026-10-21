#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    const juce::ParameterID kClarity { "clarity", 1 };
    const juce::ParameterID kHush    { "hush", 1 };
    const juce::ParameterID kInput   { "input", 1 };
    const juce::ParameterID kOutput  { "output", 1 };
}

juce::AudioProcessorValueTreeState::ParameterLayout NitidezProcessor::createLayout()
{
    using juce::AudioParameterFloat;
    using juce::NormalisableRange;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterFloat> (
        kClarity, "Nitidez", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 70.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<AudioParameterFloat> (
        kHush, "Hush", NormalisableRange<float> (0.0f, 30.0f, 0.1f), 9.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<AudioParameterFloat> (
        kInput, "Entrada", NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<AudioParameterFloat> (
        kOutput, "Saida", NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    return layout;
}

NitidezProcessor::NitidezProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Nitidez", createLayout())
{
    clarityParam = apvts.getRawParameterValue ("clarity");
    hushParam    = apvts.getRawParameterValue ("hush");
    inputParam   = apvts.getRawParameterValue ("input");
    outputParam  = apvts.getRawParameterValue ("output");
}

bool NitidezProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();

    if (in != out)
        return false;

    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void NitidezProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    engine.prepare (sampleRate, juce::jmax (1, getTotalNumInputChannels()));
    engine.setParameters (clarityParam->load() / 100.0f, hushParam->load(), inputParam->load(), outputParam->load());
    engine.reset();

    // O DAW usa isso para compensar o atraso (o FL Studio faz isso automaticamente).
    setLatencySamples (engine.getLatencySamples());
}

void NitidezProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = juce::jmin (buffer.getNumChannels(), getTotalNumInputChannels());

    // Se a saida tiver mais canais do que a entrada, limpa o excedente.
    for (int c = getTotalNumInputChannels(); c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    engine.setParameters (clarityParam->load() / 100.0f, hushParam->load(), inputParam->load(), outputParam->load());
    engine.process (buffer.getArrayOfWritePointers(), numCh, buffer.getNumSamples());

    meterVoice.store (engine.getVoiceProbability());
    meterReduction.store (engine.getReductionDb());
}

juce::AudioProcessorEditor* NitidezProcessor::createEditor()
{
    return new NitidezEditor (*this);
}

void NitidezProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void NitidezProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

// Ponto de entrada do plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NitidezProcessor();
}
