#pragma once

#include <JuceHeader.h>

class MDAVocoderAudioProcessor : public juce::AudioProcessor
{
public:
    MDAVocoderAudioProcessor();
    ~MDAVocoderAudioProcessor() override;

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

    static constexpr int NBANDS = 16;

    // Input channel swap.
    int _swap;

    // Output level.
    float _gain;

    // HF thru and HF band levels.
    float _thru, _high;

    // Downsampled output and counter.
    float _kout;
    int _kval;

    // Number of active bands.
    int _nbnd;

    // Filter coefficients and buffers.
    // [0-8][0 1 2 | 0 1 2 3 | 0 1 2 3 | val rate]
    //  #   reson  | carrier  | modulator| envelope
    float _f[NBANDS][13];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MDAVocoderAudioProcessor)
};
