#include "PluginProcessor.h"

MDATransientAudioProcessor::MDATransientAudioProcessor()
: AudioProcessor(BusesProperties()
                 .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
                 .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

MDATransientAudioProcessor::~MDATransientAudioProcessor()
{
}

const juce::String MDATransientAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

int MDATransientAudioProcessor::getNumPrograms()
{
    return 1;
}

int MDATransientAudioProcessor::getCurrentProgram()
{
    return 0;
}

void MDATransientAudioProcessor::setCurrentProgram(int index)
{
}

const juce::String MDATransientAudioProcessor::getProgramName(int index)
{
    return {};
}

void MDATransientAudioProcessor::changeProgramName(int index, const juce::String &newName)
{
}

void MDATransientAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    resetState();
}

void MDATransientAudioProcessor::releaseResources()
{
}

void MDATransientAudioProcessor::reset()
{
    resetState();
}

bool MDATransientAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MDATransientAudioProcessor::resetState()
{
    _env1 = 0.0f;
    _env2 = 0.0f;
    _env3 = 0.0f;
    _env4 = 0.0f;
    _fbuf1 = 0.0f;
    _fbuf2 = 0.0f;
}

void MDATransientAudioProcessor::update()
{
    // Output gain: parameter is in dB, range -20 to +20.
    float outputParam = apvts.getRawParameterValue("Output")->load();
    _dry = (float)std::pow(10.0, outputParam / 20.0);

    // Filter: parameter goes from -10 to +10.
    // Original: fParam4 goes 0..1, filter logic uses 0.5 as center.
    float filterParam = apvts.getRawParameterValue("Filter")->load();
    float fParam4 = (filterParam + 10.0f) / 20.0f; // map -10..+10 back to 0..1

    if (fParam4 > 0.50f)
    {
        _fili = 0.8f - 1.6f * fParam4;
        _filo = 1.0f + _fili;
        _filx = 1.0f;
    }
    else
    {
        _fili = 0.1f + 1.8f * fParam4;
        _filo = 1.0f - _fili;
        _filx = 0.0f;
    }

    // Attack: parameter goes from -100 to +100.
    // Original: fParam1 goes 0..1, with 0.5 as center.
    float attackParam = apvts.getRawParameterValue("Attack")->load();
    float fParam1 = (attackParam + 100.0f) / 200.0f; // map -100..+100 back to 0..1

    if (fParam1 > 0.5f)
    {
        _att1 = (float)std::pow(10.0, -1.5);
        _att2 = (float)std::pow(10.0, 1.0 - 5.0 * fParam1);
    }
    else
    {
        _att1 = (float)std::pow(10.0, -4.0 + 5.0 * fParam1);
        _att2 = (float)std::pow(10.0, -1.5);
    }

    // Att Hold: parameter goes from 0 to 100.
    // Original: fParam5 goes 0..1.
    float attHoldParam = apvts.getRawParameterValue("Att Hold")->load();
    float fParam5 = attHoldParam / 100.0f;
    _rel12 = 1.0f - (float)std::pow(10.0, -2.0 - 4.0 * fParam5);

    // Release: parameter goes from -100 to +100.
    // Original: fParam2 goes 0..1, with 0.5 as center.
    float releaseParam = apvts.getRawParameterValue("Release")->load();
    float fParam2 = (releaseParam + 100.0f) / 200.0f;

    if (fParam2 > 0.5f)
    {
        _rel3 = 1.0f - (float)std::pow(10.0, -4.5);
        _rel4 = 1.0f - (float)std::pow(10.0, -5.85 + 2.7 * fParam2);
    }
    else
    {
        _rel3 = 1.0f - (float)std::pow(10.0, -3.15 - 2.7 * fParam2);
        _rel4 = 1.0f - (float)std::pow(10.0, -4.5);
    }

    // Rel Hold: parameter goes from 0 to 100.
    // Original: fParam6 goes 0..1.
    float relHoldParam = apvts.getRawParameterValue("Rel Hold")->load();
    float fParam6 = relHoldParam / 100.0f;
    _att34 = (float)std::pow(10.0, -4.0 * fParam6);
}

void MDATransientAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i) {
        buffer.clear(i, 0, buffer.getNumSamples());
    }

    update();

    const float *in1 = buffer.getReadPointer(0);
    const float *in2 = buffer.getReadPointer(1);
    float *out1 = buffer.getWritePointer(0);
    float *out2 = buffer.getWritePointer(1);

    float e1 = _env1, e2 = _env2, e3 = _env3, e4 = _env4;
    const float y = _dry;
    const float a1 = _att1, a2 = _att2, r12 = _rel12;
    const float a34 = _att34, r3 = _rel3, r4 = _rel4;
    const float fi = _fili, fo = _filo, fx = _filx;
    float fb1 = _fbuf1, fb2 = _fbuf2;

    for (int n = 0; n < buffer.getNumSamples(); ++n)
    {
        float a = in1[n];
        float b = in2[n];

        // Apply filter to each channel.
        fb1 = fo * fb1 + fi * a;
        fb2 = fo * fb2 + fi * b;
        float e = fb1 + fx * a;
        float f = fb2 + fx * b;

        // Compute envelope from mono sum of input.
        float i = a + b;
        i = (i > 0.0f) ? i : -i;

        // Attack envelope followers.
        e1 = (i > e1) ? e1 + a1 * (i - e1) : e1 * r12;
        e2 = (i > e2) ? e2 + a2 * (i - e2) : e2 * r12;

        // Release envelope followers.
        e3 = (i > e3) ? e3 + a34 * (i - e3) : e3 * r3;
        e4 = (i > e4) ? e4 + a34 * (i - e4) : e4 * r4;

        // Gain modulation signal.
        float g = (e1 - e2 + e3 - e4);

        out1[n] = y * (a + e * g);
        out2[n] = y * (b + f * g);
    }

    if (e1 < 1.0e-10f) {
        _env1 = 0.0f; _env2 = 0.0f; _env3 = 0.0f; _env4 = 0.0f;
        _fbuf1 = 0.0f; _fbuf2 = 0.0f;
    } else {
        _env1 = e1; _env2 = e2; _env3 = e3; _env4 = e4;
        _fbuf1 = fb1; _fbuf2 = fb2;
    }
}

juce::AudioProcessorEditor *MDATransientAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

void MDATransientAudioProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    copyXmlToBinary(*apvts.copyState().createXml(), destData);
}

void MDATransientAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml.get() != nullptr && xml->hasTagName(apvts.state.getType())) {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout MDATransientAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Attack: original fParam1 0..1 displayed as (200*f-100) = -100..+100 %
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Attack", 1),
        "Attack",
        juce::NormalisableRange<float>(-100.0f, 100.0f, 0.01f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    // Release: original fParam2 0..1 displayed as (200*f-100) = -100..+100 %
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Release", 1),
        "Release",
        juce::NormalisableRange<float>(-100.0f, 100.0f, 0.01f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    // Output: original fParam3 0..1 displayed as (40*f-20) = -20..+20 dB
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Output", 1),
        "Output",
        juce::NormalisableRange<float>(-20.0f, 20.0f, 0.01f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    // Filter: original fParam4 0..1 displayed as (20*f-10) = -10..+10
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Filter", 1),
        "Filter",
        juce::NormalisableRange<float>(-10.0f, 10.0f, 0.01f),
        -0.2f,
        juce::AudioParameterFloatAttributes().withLabel("Lo <> Hi")));

    // Att Hold: original fParam5 0..1 displayed as (100*f) = 0..100 %
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Att Hold", 1),
        "Att Hold",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f),
        35.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    // Rel Hold: original fParam6 0..1 displayed as (100*f) = 0..100 %
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Rel Hold", 1),
        "Rel Hold",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f),
        35.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    return layout;
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new MDATransientAudioProcessor();
}
