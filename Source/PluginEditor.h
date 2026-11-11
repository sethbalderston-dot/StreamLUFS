#pragma once
#include "PluginProcessor.h"
#include "Platforms.h"
#include <deque>

class LufsEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit LufsEditor(LufsProcessor&);
    ~LufsEditor() override { stopTimer(); }
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
private:
    void timerCallback() override;
    juce::Rectangle<int> rowBounds(int i) const;
    LufsProcessor& proc;
    lm::LoudnessMeter::Snapshot snap;
    std::deque<std::pair<float, float>> history;   // {momentary, short-term}, 10 Hz, 60 s
    int tick = 0;
    juce::TextButton resetButton{ "Reset" };
};
