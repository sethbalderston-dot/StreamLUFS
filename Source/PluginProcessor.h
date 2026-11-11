#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "LoudnessMeter.h"

class LufsProcessor : public juce::AudioProcessor {
public:
    LufsProcessor();
    void prepareToPlay(double sampleRate, int) override { meter.prepare(sampleRate); }
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Stream LUFS"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& d) override { d.append(&selectedPlatform, sizeof(int)); }
    void setStateInformation(const void* data, int size) override { if (size == (int) sizeof(int)) std::memcpy(&selectedPlatform, data, sizeof(int)); }

    lm::LoudnessMeter meter;
    int selectedPlatform = 0;
};
