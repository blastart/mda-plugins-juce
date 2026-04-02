#pragma once

#include <JuceHeader.h>

class MDALeslieAudioProcessor : public juce::AudioProcessor
{
public:
    MDALeslieAudioProcessor();
    ~MDALeslieAudioProcessor() override;

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

    static constexpr float _twoPi = 6.2831853f;
    static constexpr int _bufferSize = 256;

    // Filter buffers for the crossover filter.
    float _fbuf1, _fbuf2;

    // High-frequency rotor: speed, phase.
    float _hspd, _hphi;

    // Low-frequency rotor: speed, phase.
    float _lspd, _lphi;

    // HF delay buffer for Doppler effect.
    std::vector<float> _hbuf;

    // Write position in the delay buffer.
    int _hpos;

    // Cached parameter values, computed in update().
    float _filo;        // crossover filter coefficient
    float _hset, _hmom; // high rotor target speed & momentum
    float _lset, _lmom; // low rotor target speed & momentum
    float _gain;        // output gain (linear)
    float _lwid, _llev; // low rotor width & throb level
    float _hwid, _hdep, _hlev; // high rotor width, depth & throb level
    float _spd;         // overall speed multiplier

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MDALeslieAudioProcessor)
};
