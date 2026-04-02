#pragma once

#include <JuceHeader.h>

class MDATrackerAudioProcessor : public juce::AudioProcessor
{
public:
    MDATrackerAudioProcessor();
    ~MDATrackerAudioProcessor() override;

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

    float filterFreq(float hz);

    // Cached parameter-derived values
    float _fi, _fo, _thr, _phi, _dphi, _ddphi, _trans;
    float _buf1, _buf2, _dn, _bold, _wet, _dry;
    float _dyn, _env, _rel, _saw, _dsaw;
    float _res1, _res2, _buf3, _buf4;
    int _max, _min, _num, _sig, _mode;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MDATrackerAudioProcessor)
};
