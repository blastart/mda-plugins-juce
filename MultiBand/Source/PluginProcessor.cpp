#include "PluginProcessor.h"

MDAMultiBandAudioProcessor::MDAMultiBandAudioProcessor()
: AudioProcessor(BusesProperties()
                 .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
                 .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

MDAMultiBandAudioProcessor::~MDAMultiBandAudioProcessor()
{
}

const juce::String MDAMultiBandAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

int MDAMultiBandAudioProcessor::getNumPrograms()
{
    return 1;
}

int MDAMultiBandAudioProcessor::getCurrentProgram()
{
    return 0;
}

void MDAMultiBandAudioProcessor::setCurrentProgram(int index)
{
}

const juce::String MDAMultiBandAudioProcessor::getProgramName(int index)
{
    return {};
}

void MDAMultiBandAudioProcessor::changeProgramName(int index, const juce::String &newName)
{
}

void MDAMultiBandAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    resetState();
}

void MDAMultiBandAudioProcessor::releaseResources()
{
}

void MDAMultiBandAudioProcessor::reset()
{
    resetState();
}

bool MDAMultiBandAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MDAMultiBandAudioProcessor::resetState()
{
    _gain1 = 1.0f;
    _gain2 = 1.0f;
    _gain3 = 1.0f;
    _fb1 = 0.0f;
    _fb2 = 0.0f;
    _fb3 = 0.0f;
}

void MDAMultiBandAudioProcessor::update()
{
    float listen = apvts.getRawParameterValue("Listen")->load();
    float xover1 = apvts.getRawParameterValue("L <> M")->load();
    float xover2 = apvts.getRawParameterValue("M <> H")->load();
    float lComp  = apvts.getRawParameterValue("L Comp")->load();
    float mComp  = apvts.getRawParameterValue("M Comp")->load();
    float hComp  = apvts.getRawParameterValue("H Comp")->load();
    float lOut   = apvts.getRawParameterValue("L Out")->load();
    float mOut   = apvts.getRawParameterValue("M Out")->load();
    float hOut   = apvts.getRawParameterValue("H Out")->load();
    float attack = apvts.getRawParameterValue("Attack")->load();
    float release = apvts.getRawParameterValue("Release")->load();
    float stereo = apvts.getRawParameterValue("Stereo")->load();
    float process = apvts.getRawParameterValue("Process")->load();

    // Band 1 (Low)
    _driv1 = std::pow(10.0f, 2.5f * lComp - 1.0f);
    _trim1 = 0.5f + (4.0f - 2.0f * attack) * (lComp * lComp * lComp);
    _trim1 = _trim1 * std::pow(10.0f, 2.0f * lOut - 1.0f);
    _att1 = std::pow(10.0f, -0.05f - 2.5f * attack);
    _rel1 = std::pow(10.0f, -2.0f - 3.5f * release);

    // Band 2 (Mid)
    _driv2 = std::pow(10.0f, 2.5f * mComp - 1.0f);
    _trim2 = 0.5f + (4.0f - 2.0f * attack) * (mComp * mComp * mComp);
    _trim2 = _trim2 * std::pow(10.0f, 2.0f * mOut - 1.0f);
    _att2 = std::pow(10.0f, -0.05f - 2.0f * attack);
    _rel2 = std::pow(10.0f, -2.0f - 3.0f * release);

    // Band 3 (High)
    _driv3 = std::pow(10.0f, 2.5f * hComp - 1.0f);
    _trim3 = 0.5f + (4.0f - 2.0f * attack) * (hComp * hComp * hComp);
    _trim3 = _trim3 * std::pow(10.0f, 2.0f * hOut - 1.0f);
    _att3 = std::pow(10.0f, -0.05f - 1.5f * attack);
    _rel3 = std::pow(10.0f, -2.0f - 2.5f * release);

    // Listen mode: solo a band by zeroing others
    switch (int(listen * 10.0f))
    {
        case 0: _trim2 = 0.0f; _trim3 = 0.0f; _slev = 0.0f; break;
        case 1:
        case 2: _trim1 = 0.0f; _trim3 = 0.0f; _slev = 0.0f; break;
        case 3:
        case 4: _trim1 = 0.0f; _trim2 = 0.0f; _slev = 0.0f; break;
        default: _slev = stereo; break;
    }

    // Crossover filter coefficients
    _fi1 = std::pow(10.0f, xover1 - 1.70f);
    _fo1 = 1.0f - _fi1;
    _fi2 = std::pow(10.0f, xover2 - 1.05f);
    _fo2 = 1.0f - _fi2;

    // M/S swap
    _mswap = (process > 0.5f) ? 1 : 0;
}

void MDAMultiBandAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
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

    float l = _fb3, b1 = _fb1, b2 = _fb2;
    float f1i = _fi1, f1o = _fo1, f2i = _fi2, f2o = _fo2;
    float g1 = _gain1, d1 = _driv1, t1 = _trim1, a1 = _att1, r1 = 1.0f - _rel1;
    float g2 = _gain2, d2 = _driv2, t2 = _trim2, a2 = _att2, r2 = 1.0f - _rel2;
    float g3 = _gain3, d3 = _driv3, t3 = _trim3, a3 = _att3, r3 = 1.0f - _rel3;
    float sl = _slev;
    int ms = _mswap;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        float a = in1[i];
        float b = in2[i];

        b = ms ? -b : b;

        float s = (a - b) * sl;  // keep stereo component for later
        a += b;

        // Crossover filters
        b2 = (f2i * a) + (f2o * b2);
        b1 = (f1i * b2) + (f1o * b1);
        l  = (f1i * b1) + (f1o * l);
        float m = b2 - l;
        float h = a - b2;

        // Band 1 (Low) compressor
        float tmp1 = (l > 0) ? l : -l;
        g1 = (tmp1 > g1) ? g1 + a1 * (tmp1 - g1) : g1 * r1;
        tmp1 = 1.0f / (1.0f + d1 * g1);

        // Band 2 (Mid) compressor
        float tmp2 = (m > 0) ? m : -m;
        g2 = (tmp2 > g2) ? g2 + a2 * (tmp2 - g2) : g2 * r2;
        tmp2 = 1.0f / (1.0f + d2 * g2);

        // Band 3 (High) compressor
        float tmp3 = (h > 0) ? h : -h;
        g3 = (tmp3 > g3) ? g3 + a3 * (tmp3 - g3) : g3 * r3;
        tmp3 = 1.0f / (1.0f + d3 * g3);

        // Recombine bands (note: original uses tmp3 for band 1, faithful port)
        a = (l * tmp3 * t1) + (m * tmp2 * t2) + (h * tmp3 * t3);
        float c = a + s;
        float d = ms ? (s - a) : (a - s);

        out1[i] = c;
        out2[i] = d;
    }

    // Trap denormals
    _gain1 = (g1 < 1.0e-10f) ? 0.0f : g1;
    _gain2 = (g2 < 1.0e-10f) ? 0.0f : g2;
    _gain3 = (g3 < 1.0e-10f) ? 0.0f : g3;
    if (std::fabs(b1) < 1.0e-10f) {
        _fb1 = 0.0f; _fb2 = 0.0f; _fb3 = 0.0f;
    } else {
        _fb1 = b1; _fb2 = b2; _fb3 = l;
    }
}

juce::AudioProcessorEditor *MDAMultiBandAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

void MDAMultiBandAudioProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    copyXmlToBinary(*apvts.copyState().createXml(), destData);
}

void MDAMultiBandAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml.get() != nullptr && xml->hasTagName(apvts.state.getType())) {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout MDAMultiBandAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Listen", 1),
        "Listen",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        1.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction(
                [](float value, int) {
                    switch (int(value * 10.0f)) {
                        case 0: return juce::String("Low");
                        case 1: case 2: return juce::String("Mid");
                        case 3: case 4: return juce::String("High");
                        default: return juce::String("Output");
                    }
                })));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("L <> M", 1),
        "L <> M",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        0.103f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("M <> H", 1),
        "M <> H",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        0.878f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("L Comp", 1),
        "L Comp",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        0.54f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("M Comp", 1),
        "M Comp",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("H Comp", 1),
        "H Comp",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        0.60f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("L Out", 1),
        "L Out",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        0.45f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("M Out", 1),
        "M Out",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        0.50f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("H Out", 1),
        "H Out",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        0.50f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Attack", 1),
        "Attack",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        0.22f,
        juce::AudioParameterFloatAttributes().withLabel("us")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Release", 1),
        "Release",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        0.602f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Stereo", 1),
        "Stereo",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        0.55f,
        juce::AudioParameterFloatAttributes().withLabel("% Width")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Process", 1),
        "Process",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        0.40f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction(
                [](float value, int) {
                    return (value > 0.5f) ? juce::String("S") : juce::String("M");
                })));

    return layout;
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new MDAMultiBandAudioProcessor();
}
