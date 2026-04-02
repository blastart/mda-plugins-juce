#pragma once

#include <JuceHeader.h>

class MDADubDelayAudioProcessor : public juce::AudioProcessor
{
public:
    MDADubDelayAudioProcessor();
    ~MDADubDelayAudioProcessor() override;

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

    // Maximum delay buffer size.
    static constexpr int _maxBufferSize = 323766;

    // Delay buffer.
    std::vector<float> _buffer;
    int _size;
    int _ipos;

    // Wet & dry mix.
    float _wet, _dry;

    // Feedback amount.
    float _fbk;

    // Low & high mix, crossover filter coeff & buffer.
    float _lmix, _hmix, _fil, _fil0;

    // Limiter envelope and release.
    float _env, _rel;

    // Delay length, modulation depth, LFO phase, LFO rate.
    float _del, _mod, _phi, _dphi;

    // Smoothed modulated delay.
    float _dlbuf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MDADubDelayAudioProcessor)
};
