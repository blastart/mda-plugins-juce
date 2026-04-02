#pragma once

#include <JuceHeader.h>

class MDALooplexAudioProcessor : public juce::AudioProcessor
{
public:
    MDALooplexAudioProcessor();
    ~MDALooplexAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;

    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;

    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override;

    bool acceptsMidi() const override { return true; }
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
    void reallocateBuffer();

    float _sampleRate = 44100.0f;

    // DSP state
    float _inMix, _inPan, _outMix, _feedback, _modwhl;

    // Loop buffer (interleaved stereo, stored as short like original)
    std::vector<short> _buffer;
    int _bufpos, _buflen, _bufmax;
    int _status;  // 0=not recorded, 1=first pass, 2=looping
    int _recreq;  // record request: 0=monitor, 1=record

    // Dither state
    float _d0;

    // Previous parameter values for detecting changes
    float _oldParam0, _oldParam1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MDALooplexAudioProcessor)
};
