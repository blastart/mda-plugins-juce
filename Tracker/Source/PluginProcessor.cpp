#include "PluginProcessor.h"

MDATrackerAudioProcessor::MDATrackerAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

MDATrackerAudioProcessor::~MDATrackerAudioProcessor()
{
}

const juce::String MDATrackerAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

int MDATrackerAudioProcessor::getNumPrograms()
{
    return 1;
}

int MDATrackerAudioProcessor::getCurrentProgram()
{
    return 0;
}

void MDATrackerAudioProcessor::setCurrentProgram(int index)
{
}

const juce::String MDATrackerAudioProcessor::getProgramName(int index)
{
    return {};
}

void MDATrackerAudioProcessor::changeProgramName(int index, const juce::String &newName)
{
}

void MDATrackerAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    _dphi = 100.0f / (float)sampleRate; // initial pitch
    _min = (int)(sampleRate / 30.0);     // lower limit
    _res1 = std::cos(0.01f);
    _res2 = std::sin(0.01f);

    resetState();
}

void MDATrackerAudioProcessor::releaseResources()
{
}

void MDATrackerAudioProcessor::reset()
{
    resetState();
}

bool MDATrackerAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MDATrackerAudioProcessor::resetState()
{
    _phi = 0.0f;
    _buf1 = 0.0f;
    _buf2 = 0.0f;
    _buf3 = 0.0f;
    _buf4 = 0.0f;
    _dn = 0.0f;
    _bold = 0.0f;
    _env = 0.0f;
    _saw = 0.0f;
    _dsaw = 0.0f;
    _num = 0;
    _sig = 0;
}

float MDATrackerAudioProcessor::filterFreq(float hz)
{
    float r = 0.999f;
    float j = r * r - 1.0f;
    float k = 2.0f - 2.0f * r * r * std::cos(0.647f * hz / (float)getSampleRate());
    return (std::sqrt(k * k - 4.0f * j * j) - k) / (2.0f * j);
}

void MDATrackerAudioProcessor::update()
{
    float modeParam = apvts.getRawParameterValue("Mode")->load();
    float dynamicsParam = apvts.getRawParameterValue("Dynamics")->load() / 100.0f;
    float mixParam = apvts.getRawParameterValue("Mix")->load() / 100.0f;
    float glideParam = apvts.getRawParameterValue("Glide")->load() / 100.0f;
    float transposeParam = apvts.getRawParameterValue("Transpose")->load();
    float maximumParam = apvts.getRawParameterValue("Maximum")->load() / 100.0f;
    float triggerParam = apvts.getRawParameterValue("Trigger")->load() / 100.0f;
    float outputParam = apvts.getRawParameterValue("Output")->load() / 100.0f;

    _mode = (int)(modeParam);
    _fo = filterFreq(50.0f);
    _fi = (1.0f - _fo) * (1.0f - _fo);
    _ddphi = glideParam * glideParam;
    _thr = (float)std::pow(10.0, 3.0 * triggerParam - 3.8);
    _max = (int)(getSampleRate() / std::pow(10.0f, 1.6f + 2.2f * maximumParam));
    _trans = (float)std::pow(1.0594631, (int)(72.0f * (transposeParam / 72.0f + 0.5f) - 36.0f));
    float wet = (float)std::pow(10.0, 2.0 * outputParam - 1.0);

    if (_mode < 4)
    {
        _dyn = wet * 0.6f * mixParam * dynamicsParam;
        _dry = wet * std::sqrt(1.0f - mixParam);
        _wet = wet * 0.3f * mixParam * (1.0f - dynamicsParam);
    }
    else
    {
        _dry = wet * (1.0f - mixParam);
        _wet = wet * (0.02f * mixParam - 0.004f);
        _dyn = 0.0f;
    }
    _rel = (float)std::pow(10.0, -10.0 / getSampleRate());
}

void MDATrackerAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
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

    float t = _thr, p = _phi, dp = _dphi, ddp = _ddphi;
    float o = _fo, fi = _fi, b1 = _buf1, b2 = _buf2;
    const float twopi = 6.2831853f;
    float we = _wet, dr = _dry, bo = _bold, r1 = _res1, r2 = _res2, b3 = _buf3, b4 = _buf4;
    float sw = _saw, dsw = _dsaw, dy = _dyn, e = _env, re = _rel;
    int m = _max, n = _num, s = _sig, mn = _min, mo = _mode;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        float a = in1[i];
        float b = in2[i];
        float x = a;

        float tmp = (x > 0.0f) ? x : -x; // dynamics envelope
        e = (tmp > e) ? 0.5f * (tmp + e) : e * re;

        b1 = o * b1 + fi * x;
        b2 = o * b2 + b1; // low-pass filter

        if (b2 > t) // if > thresh
        {
            if (s < 1) // and was < thresh
            {
                if (n < mn) // not long ago
                {
                    float tmp2 = b2 / (b2 - bo); // update period
                    tmp = _trans * twopi / (n + _dn - tmp2);
                    dp = dp + ddp * (tmp - dp);
                    _dn = tmp2;
                    dsw = 0.3183098f * dp;
                    if (mo == 4)
                    {
                        r1 = std::cos(4.0f * dp); // resonator
                        r2 = std::sin(4.0f * dp);
                    }
                }
                n = 0; // restart period measurement
            }
            s = 1;
        }
        else
        {
            if (n > m) s = 0; // now < thresh
        }
        n++;
        bo = b2;

        p = std::fmod(p + dp, twopi);
        switch (mo)
        {
            case 0: x = std::sin(p); break;                                    // sine
            case 1: x = (std::sin(p) > 0.0f) ? 0.5f : -0.5f; break;          // square
            case 2: sw = std::fmod(sw + dsw, 2.0f); x = sw - 1.0f; break;    // saw
            case 3: x *= std::sin(p); break;                                   // ring
            case 4: x += (b3 * r1) - (b4 * r2);                               // filt
                    b4 = 0.996f * ((b3 * r2) + (b4 * r1));
                    b3 = 0.996f * x; break;
        }
        x *= (we + dy * e);
        out1[i] = a;
        out2[i] = dr * b + x;
    }

    if (std::abs(b1) < 1.0e-10f) { _buf1 = 0.0f; _buf2 = 0.0f; _buf3 = 0.0f; _buf4 = 0.0f; }
    else { _buf1 = b1; _buf2 = b2; _buf3 = b3; _buf4 = b4; }
    _phi = p;
    _dphi = dp;
    _sig = s;
    _bold = bo;
    _num = (n > 100000) ? 100000 : n;
    _env = e;
    _saw = sw;
    _dsaw = dsw;
    _res1 = r1;
    _res2 = r2;
}

juce::AudioProcessorEditor *MDATrackerAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

void MDATrackerAudioProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    copyXmlToBinary(*apvts.copyState().createXml(), destData);
}

void MDATrackerAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml.get() != nullptr && xml->hasTagName(apvts.state.getType())) {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout MDATrackerAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Mode: 0=Sine, 1=Square, 2=Saw, 3=Ring, 4=EQ
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Mode", 1),
        "Mode",
        juce::NormalisableRange<float>(0.0f, 4.0f, 1.0f),
        0.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction(
                [](float value, int) {
                    switch ((int)value) {
                        case 0: return juce::String("SINE");
                        case 1: return juce::String("SQUARE");
                        case 2: return juce::String("SAW");
                        case 3: return juce::String("RING");
                        case 4: return juce::String("EQ");
                        default: return juce::String("SINE");
                    }
                }
            )));

    // Dynamics: 0..100%
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Dynamics", 1),
        "Dynamics",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f),
        100.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    // Mix: 0..100%
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Mix", 1),
        "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f),
        100.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    // Glide (Tracking): 0..100%
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Glide", 1),
        "Glide",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f),
        97.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    // Transpose: -36..+36 semitones
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Transpose", 1),
        "Transpose",
        juce::NormalisableRange<float>(-36.0f, 36.0f, 1.0f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("semi")));

    // Maximum: 0..100% (controls maximum tracking frequency)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Maximum", 1),
        "Maximum",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f),
        80.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    // Trigger: 0..100% (threshold level)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Trigger", 1),
        "Trigger",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f),
        50.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    // Output: 0..100%
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Output", 1),
        "Output",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f),
        50.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    return layout;
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new MDATrackerAudioProcessor();
}
