#pragma once

#include <JuceHeader.h>

class MDAThruZeroAudioProcessor : public juce::AudioProcessor
{
public:
    MDAThruZeroAudioProcessor();
    ~MDAThruZeroAudioProcessor() override;

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

    static constexpr int BUFMAX = 2048;

    // Cached parameter-derived values
    float _rat, _dep, _wet, _dry, _fb, _dem;

    // LFO and feedback state
    float _phi, _fb1, _fb2, _deps;

    // Delay buffers
    std::vector<float> _buffer, _buffer2;
    int _bufpos;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MDAThruZeroAudioProcessor)
};
