#pragma once

#include <JuceHeader.h>

class MDADeEssAudioProcessor : public juce::AudioProcessor
{
public:
    MDADeEssAudioProcessor();
    ~MDADeEssAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;

    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;

    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override;

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String &newName) override;

    void getStateInformation(juce::MemoryBlock &destData) override;
    void setStateInformation(const void *data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "Parameters", createParameterLayout() };

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void update();
    void resetState();

    // Filter buffers for 2nd-order crossover.
    float _fbuf1, _fbuf2;

    // Envelope follower state.
    float _env;

    // Cached parameter-derived values.
    float _thr, _att, _rel, _fil, _gai;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MDADeEssAudioProcessor)
};
