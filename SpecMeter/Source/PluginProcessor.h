#pragma once

#include <JuceHeader.h>

class MDASpecMeterAudioProcessor : public juce::AudioProcessor
{
public:
    MDASpecMeterAudioProcessor();
    ~MDASpecMeterAudioProcessor() override;

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

    // Publicly accessible meter values (for potential future custom editor).
    // These are updated periodically from the audio thread.
    std::atomic<float> Lpeak { 0.0f }, Lhold { 0.0f }, Lmin { 0.0f }, Lrms { 0.0f };
    std::atomic<float> Rpeak { 0.0f }, Rhold { 0.0f }, Rmin { 0.0f }, Rrms { 0.0f };
    std::atomic<float> Corr { 0.0f };
    std::atomic<int> counter { 0 };

    // Spectrum band levels [channel][band], 13 bands per channel.
    float band[2][16];

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void update();
    void resetState();

    // Internal accumulator state
    float _den;
    float _lpeak, _lmin, _lrms;
    float _rpeak, _rmin, _rrms;
    float _corr;
    float _iK;
    int _K, _kmax, _topband;

    // Polyphase filter bank state: [stage][band]
    float _lpp[6][16];
    float _rpp[6][16];

    // Input gain
    float _gain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MDASpecMeterAudioProcessor)
};
