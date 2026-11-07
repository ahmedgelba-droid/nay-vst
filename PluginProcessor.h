#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <memory>
#include <vector>

// One loaded sample (mono) tagged with the MIDI note it was recorded at.
struct NaySample
{
    int rootNote = 60;
    double sampleRate = 44100.0;
    juce::AudioBuffer<float> data;
};

using NayBank = std::vector<std::shared_ptr<const NaySample>>;

class NayAudioProcessor : public juce::AudioProcessor
{
public:
    NayAudioProcessor();
    ~NayAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Nay"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Message thread only. Returns number of samples loaded.
    int loadSampleFolder (const juce::File& folder);
    juce::String getSampleInfo() const { return sampleInfo; }

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static int parseRootNote (const juce::String& fileName);

    void handleMidi (const juce::MidiMessage&, const std::shared_ptr<const NayBank>&);
    void render (juce::AudioBuffer<float>&, int start, int num);
    double noteTarget (int note) const;

    // sample bank (swapped from the message thread)
    std::shared_ptr<const NayBank> bank;
    juce::SpinLock bankLock;
    juce::String sampleInfo { "No samples loaded" };
    juce::String samplePath;

    // cached parameter pointers
    std::atomic<float>* pGainRaw    = nullptr;
    std::atomic<float>* pAttackRaw  = nullptr;
    std::atomic<float>* pReleaseRaw = nullptr;
    std::atomic<float>* pGlideRaw   = nullptr;
    std::atomic<float>* pVibDepthRaw = nullptr;
    std::atomic<float>* pVibRateRaw = nullptr;
    std::atomic<float>* pTuneRaw    = nullptr;
    std::atomic<float>* qtRaw[12] {};

    // per-block parameter values
    float pGain = 0.8f, pGlide = 60.f, pVibDepth = 0.f, pVibRate = 5.f, pTune = 50.f;
    bool qt[12] {};

    // mono voice state
    std::vector<int> held;
    std::shared_ptr<const NaySample> current;
    double playPos = 0.0, pitch = 60.0, target = 60.0, vibPhase = 0.0;
    float modWheel = 0.f, bendSemis = 0.f;
    juce::ADSR adsr;
    double sr = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NayAudioProcessor)
};
