#pragma once

#include "PluginProcessor.h"
#include <array>

// Yellow "finger hole" buttons like the reference screenshot.
class NayLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};

class NayAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit NayAudioProcessorEditor (NayAudioProcessor&);
    ~NayAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };

    NayAudioProcessor& proc;
    NayLookAndFeel laf;

    std::array<juce::ToggleButton, 12> keys;   // indexed by pitch class 0..11 (C..B)
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, 12> keyAtt;
    std::array<Knob, 7> knobs;

    juce::TextButton loadButton { "Load Samples..." };
    juce::Label info;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NayAudioProcessorEditor)
};
