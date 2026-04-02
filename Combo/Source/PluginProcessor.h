#pragma once

#include <JuceHeader.h>

class MDAComboAudioProcessor : public juce::AudioProcessor
{
public:
    MDAComboAudioProcessor();
    ~MDAComboAudioProcessor() override;

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

    // Compute filter coefficient from frequency in Hz.
    float filterFreq(float hz);

    static constexpr int _bufferSize = 1024;

    // Two delay buffers (one per channel in stereo mode).
    std::vector<float> _buffer;
    std::vector<float> _buffe2;
    int _bufpos;

    // Model-dependent cached values.
    float _clip, _drive, _trim, _lpf, _hpf, _mix1, _mix2;
    float _bias;
    int _del1, _del2;
    int _mode, _ster;

    // Filter states for left channel (cascade of 4 LPF + 1 HPF).
    float _ff1, _ff2, _ff3, _ff4, _ff5;

    // Filter states for right channel (stereo mode).
    float _ff6, _ff7, _ff8, _ff9, _ff10;

    // HPF resonant filter state (Chamberlin SVF).
    float _hhf, _hhq, _hh0, _hh1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MDAComboAudioProcessor)
};
