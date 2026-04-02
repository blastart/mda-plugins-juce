#pragma once

#include <JuceHeader.h>

class MDAMultiBandAudioProcessor : public juce::AudioProcessor
{
public:
    MDAMultiBandAudioProcessor();
    ~MDAMultiBandAudioProcessor() override;

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

    // Per-band compressor state
    float _gain1, _driv1, _att1, _rel1, _trim1;
    float _gain2, _driv2, _att2, _rel2, _trim2;
    float _gain3, _driv3, _att3, _rel3, _trim3;

    // Crossover filter coefficients and state
    float _fi1, _fo1, _fi2, _fo2;
    float _fb1, _fb2, _fb3;

    // Stereo width level
    float _slev;

    // M/S swap flag
    int _mswap;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MDAMultiBandAudioProcessor)
};
