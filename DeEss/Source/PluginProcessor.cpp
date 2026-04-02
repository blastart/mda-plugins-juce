#include "PluginProcessor.h"

MDADeEssAudioProcessor::MDADeEssAudioProcessor()
: AudioProcessor(BusesProperties()
                 .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
                 .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

MDADeEssAudioProcessor::~MDADeEssAudioProcessor()
{
}

const juce::String MDADeEssAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

int MDADeEssAudioProcessor::getNumPrograms()
{
    return 1;
}

int MDADeEssAudioProcessor::getCurrentProgram()
{
    return 0;
}

void MDADeEssAudioProcessor::setCurrentProgram(int index)
{
}

const juce::String MDADeEssAudioProcessor::getProgramName(int index)
{
    return {};
}

void MDADeEssAudioProcessor::changeProgramName(int index, const juce::String &newName)
{
}

void MDADeEssAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    resetState();
}

void MDADeEssAudioProcessor::releaseResources()
{
}

void MDADeEssAudioProcessor::reset()
{
    resetState();
}

bool MDADeEssAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MDADeEssAudioProcessor::resetState()
{
    _fbuf1 = 0.0f;
    _fbuf2 = 0.0f;
    _env = 0.0f;
}

void MDADeEssAudioProcessor::update()
{
    float fParam1 = apvts.getRawParameterValue("Thresh")->load();
    float fParam2 = apvts.getRawParameterValue("Freq")->load();
    float fParam3 = apvts.getRawParameterValue("HF Drive")->load();

    _thr = std::pow(10.0f, 3.0f * fParam1 - 3.0f);
    _att = 0.01f;
    _rel = 0.992f;
    _fil = 0.05f + 0.94f * fParam2 * fParam2;
    _gai = std::pow(10.0f, 2.0f * fParam3 - 1.0f);
}

void MDADeEssAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
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

    float f1 = _fbuf1, f2 = _fbuf2;
    float fi = _fil, fo = 1.0f - _fil;
    float at = _att, re = _rel, en = _env, th = _thr;
    float gg = _gai;

    for (int n = 0; n < buffer.getNumSamples(); ++n)
    {
        float a = in1[n];
        float b = in2[n];

        float tmp = 0.5f * (a + b);       // 2nd order crossover
        f1 = fo * f1 + fi * tmp;
        tmp -= f1;
        f2 = fo * f2 + fi * tmp;
        tmp = gg * (tmp - f2);             // extra HF gain

        en = (tmp > en) ? en + at * (tmp - en) : en * re;  // envelope
        float g;
        if (en > th)
            g = f1 + f2 + tmp * (th / en); // limit
        else
            g = f1 + f2 + tmp;

        out1[n] = g;
        out2[n] = g;
    }

    // Trap denormals
    if (std::abs(f1) < 1.0e-10f) { _fbuf1 = 0.0f; _fbuf2 = 0.0f; }
    else { _fbuf1 = f1; _fbuf2 = f2; }

    if (std::abs(en) < 1.0e-10f) _env = 0.0f;
    else _env = en;
}

juce::AudioProcessorEditor *MDADeEssAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

void MDADeEssAudioProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    copyXmlToBinary(*apvts.copyState().createXml(), destData);
}

void MDADeEssAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml.get() != nullptr && xml->hasTagName(apvts.state.getType())) {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout MDADeEssAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Thresh: 0-1, display as -60 to 0 dB
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Thresh", 1),
        "Thresh",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        0.15f,
        juce::AudioParameterFloatAttributes()
            .withLabel("dB")
            .withStringFromValueFunction(
                [](float value, int) { return juce::String(int(60.0f * value - 60.0f)); }
            )));

    // Freq: 0-1, display as 1000-12000 Hz
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Freq", 1),
        "Freq",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        0.60f,
        juce::AudioParameterFloatAttributes()
            .withLabel("Hz")
            .withStringFromValueFunction(
                [](float value, int) { return juce::String(int(1000.0f + 11000.0f * value)); }
            )));

    // HF Drive: 0-1, display as -20 to +20 dB
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("HF Drive", 1),
        "HF Drive",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        0.50f,
        juce::AudioParameterFloatAttributes()
            .withLabel("dB")
            .withStringFromValueFunction(
                [](float value, int) { return juce::String(int(40.0f * value - 20.0f)); }
            )));

    return layout;
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new MDADeEssAudioProcessor();
}
