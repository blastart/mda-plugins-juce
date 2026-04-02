#pragma once

#include <JuceHeader.h>

class MDAVocInputAudioProcessor : public juce::AudioProcessor
{
public:
    MDAVocInputAudioProcessor();
    ~MDAVocInputAudioProcessor() override;

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

    // Track input pitch mode: 0 = off, 1 = free, 2 = quantize.
    int _track;

    // Output sawtooth increment per sample.
    float _pstep;

    // Tuning multiplier.
    float _pmult;

    // Sawtooth buffer.
    float _sawbuf;

    // Breath noise level.
    float _noise;

    // LF and overall envelope.
    float _lenv, _henv;

    // LF filter buffers.
    float _lbuf0, _lbuf1;

    // Previous LF sample.
    float _lbuf2;

    // Period measurement.
    float _lbuf3;

    // LF filter coefficient.
    float _lfreq;

    // Voiced/unvoiced threshold.
    float _vuv;

    // Preferred period range.
    float _maxp, _minp;

    // Tuning reference (MIDI note 0 in Hz).
    double _root;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MDAVocInputAudioProcessor)
};
