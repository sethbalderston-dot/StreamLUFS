#include "PluginProcessor.h"
#include "PluginEditor.h"

LufsProcessor::LufsProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)) {}

bool LufsProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    return l.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

// Audio passes through untouched; we only measure.
void LufsProcessor::processBlock(juce::AudioBuffer<float>& b, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    if (b.getNumChannels() >= 2) meter.process(b.getReadPointer(0), b.getReadPointer(1), b.getNumSamples());
}

juce::AudioProcessorEditor* LufsProcessor::createEditor() { return new LufsEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LufsProcessor(); }
