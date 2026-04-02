#pragma once

#include <JuceHeader.h>

class MDATransientAudioProcessor : public juce::AudioProcessor
{
public:
    MDATransientAudioProcessor();
    ~MDATransientAudioProcessor() override;

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

    // Output gain.
    float _dry;

    // Attack envelope followers: att1 (fast/slow) and att2 (slow/fast).
    float _att1, _att2;

    // Release for attack envelopes.
    float _rel12;

    // Release envelope followers.
    float _att34;
    float _rel3, _rel4;

    // Envelope states.
    float _env1, _env2, _env3, _env4;

    // Filter coefficients.
    float _fili, _filo, _filx;

    // Filter buffers.
    float _fbuf1, _fbuf2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MDATransientAudioProcessor)
};
