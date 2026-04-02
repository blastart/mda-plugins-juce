#pragma once

#include <JuceHeader.h>

class MDARePsychoAudioProcessor : public juce::AudioProcessor
{
public:
    MDARePsychoAudioProcessor();
    ~MDARePsychoAudioProcessor() override;

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

    static constexpr int _bufferSize = 22050;

    // Buffers for left and right channel playback
    std::vector<float> _buffer;
    std::vector<float> _buffer2;

    // Cached DSP parameters
    float _thr, _env, _tun, _wet, _dry;

    // Runtime state
    float _gai;        // current gain envelope
    float _buf, _buf2; // crossfade memory for fade-in
    int _tim;          // current playback position (timer)
    int _dtim;         // minimum hold time before re-trigger

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MDARePsychoAudioProcessor)
};
