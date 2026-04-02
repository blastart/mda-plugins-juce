#include "PluginProcessor.h"

MDAThruZeroAudioProcessor::MDAThruZeroAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

MDAThruZeroAudioProcessor::~MDAThruZeroAudioProcessor()
{
}

const juce::String MDAThruZeroAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

int MDAThruZeroAudioProcessor::getNumPrograms()
{
    return 1;
}

int MDAThruZeroAudioProcessor::getCurrentProgram()
{
    return 0;
}

void MDAThruZeroAudioProcessor::setCurrentProgram(int index)
{
}

const juce::String MDAThruZeroAudioProcessor::getProgramName(int index)
{
    return {};
}

void MDAThruZeroAudioProcessor::changeProgramName(int index, const juce::String &newName)
{
}

void MDAThruZeroAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    _buffer.resize(BUFMAX);
    _buffer2.resize(BUFMAX);

    resetState();
}

void MDAThruZeroAudioProcessor::releaseResources()
{
}

void MDAThruZeroAudioProcessor::reset()
{
    resetState();
}

bool MDAThruZeroAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MDAThruZeroAudioProcessor::resetState()
{
    _bufpos = 0;
    _phi = 0.0f;
    _fb = 0.0f;
    _fb1 = 0.0f;
    _fb2 = 0.0f;
    _deps = 0.0f;

    if (!_buffer.empty()) memset(_buffer.data(), 0, BUFMAX * sizeof(float));
    if (!_buffer2.empty()) memset(_buffer2.data(), 0, BUFMAX * sizeof(float));
}

void MDAThruZeroAudioProcessor::update()
{
    float rateParam = apvts.getRawParameterValue("Rate")->load();
    float depthParam = apvts.getRawParameterValue("Depth")->load() / 100.0f;
    float mixParam = apvts.getRawParameterValue("Mix")->load() / 100.0f;
    float feedbackParam = apvts.getRawParameterValue("Feedback")->load() / 100.0f;
    float depthModParam = apvts.getRawParameterValue("DepthMod")->load() / 100.0f;

    // Rate: the original used pow(10, 3*param - 2) * 2 / sampleRate
    // rateParam is already the cycle time in seconds (displayed as such).
    // We convert back to the 0-1 range the original expected.
    // Original param[0] range 0..1 mapped to cycle period.
    // We keep it simple: use the same formula with the normalized value.
    float rateNorm = rateParam; // already 0..1
    _rat = (float)(std::pow(10.0f, 3.0f * rateNorm - 2.0f) * 2.0f / getSampleRate());
    _dep = 2000.0f * depthParam * depthParam;
    _dem = _dep - _dep * depthModParam;
    _dep -= _dem;

    _wet = mixParam;
    _dry = 1.0f - _wet;
    if (rateNorm < 0.01f) { _rat = 0.0f; _phi = 0.0f; }
    _fb = 1.9f * feedbackParam - 0.95f;
}

void MDAThruZeroAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    update();

    const float *in1 = buffer.getReadPointer(0);
    const float *in2 = buffer.getReadPointer(1);
    float *out1 = buffer.getWritePointer(0);
    float *out2 = buffer.getWritePointer(1);

    float f = _fb, f1 = _fb1, f2 = _fb2, ph = _phi;
    float ra = _rat, de = _dep, we = _wet, dr = _dry, ds = _deps, dm = _dem;
    int bp = _bufpos;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        float a = in1[i];
        float b = in2[i];

        ph += ra;
        if (ph > 1.0f) ph -= 2.0f;

        bp--;
        bp &= 0x7FF;
        _buffer[bp] = a + f * f1;
        _buffer2[bp] = b + f * f2;

        float dpt = dm + de * (1.0f - ph * ph); // delay mod shape
        int tmp = (int)dpt;
        float tmpf = dpt - (float)tmp;
        tmp = (tmp + bp) & 0x7FF;
        int tmpi = (tmp + 1) & 0x7FF;

        f1 = _buffer[tmp];
        f2 = _buffer2[tmp];
        f1 = tmpf * (_buffer[tmpi] - f1) + f1;  // linear interpolation
        f2 = tmpf * (_buffer2[tmpi] - f2) + f2;

        a = a * dr - f1 * we;
        b = b * dr - f2 * we;

        out1[i] = a;
        out2[i] = b;
    }

    if (std::abs(f1) > 1.0e-10f) { _fb1 = f1; _fb2 = f2; }
    else { _fb1 = _fb2 = 0.0f; }
    _phi = ph;
    _deps = ds;
    _bufpos = bp;
}

juce::AudioProcessorEditor *MDAThruZeroAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

void MDAThruZeroAudioProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    copyXmlToBinary(*apvts.copyState().createXml(), destData);
}

void MDAThruZeroAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml.get() != nullptr && xml->hasTagName(apvts.state.getType())) {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout MDAThruZeroAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Rate: 0..1 normalized value. Original display was cycle period in seconds.
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Rate", 1),
        "Rate",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.30f,
        juce::AudioParameterFloatAttributes()
            .withLabel("sec")
            .withStringFromValueFunction(
                [](float value, int) {
                    if (value < 0.01f) return juce::String("-");
                    return juce::String(std::pow(10.0f, 2.0f - 3.0f * value), 2);
                }
            )));

    // Depth: 0..100%
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Depth", 1),
        "Depth",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f),
        43.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    // Mix: 0..100%
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Mix", 1),
        "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f),
        47.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    // Feedback: 0..100% (mapped internally to -0.95..+0.95)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Feedback", 1),
        "Feedback",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f),
        30.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    // Depth Mod: 0..100%
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("DepthMod", 1),
        "DepthMod",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f),
        100.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    return layout;
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new MDAThruZeroAudioProcessor();
}
