#pragma once

#include <JuceHeader.h>

class MDATalkBoxAudioProcessor : public juce::AudioProcessor
{
public:
    MDATalkBoxAudioProcessor();
    ~MDATalkBoxAudioProcessor() override;

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

    void lpc(float *buf, float *car, int n, int o);
    void lpcDurbin(float *r, int p, float *k, float *g);

    static constexpr int BUF_MAX = 1600;
    static constexpr int ORD_MAX = 50;
    static constexpr float TWO_PI_F = 6.28318530717958647692528676655901f;

    std::vector<float> _car0, _car1;
    std::vector<float> _window;
    std::vector<float> _buf0, _buf1;

    float _emphasis;
    int _K, _N, _O, _pos, _swap;
    float _wet, _dry, _FX;

    float _d0, _d1, _d2, _d3, _d4;
    float _u0, _u1, _u2, _u3, _u4;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MDATalkBoxAudioProcessor)
};
