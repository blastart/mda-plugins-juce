#include "PluginProcessor.h"

MDADubDelayAudioProcessor::MDADubDelayAudioProcessor()
: AudioProcessor(BusesProperties()
                 .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
                 .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

MDADubDelayAudioProcessor::~MDADubDelayAudioProcessor()
{
}

const juce::String MDADubDelayAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

int MDADubDelayAudioProcessor::getNumPrograms()
{
    return 1;
}

int MDADubDelayAudioProcessor::getCurrentProgram()
{
    return 0;
}

void MDADubDelayAudioProcessor::setCurrentProgram(int index)
{
}

const juce::String MDADubDelayAudioProcessor::getProgramName(int index)
{
    return {};
}

void MDADubDelayAudioProcessor::changeProgramName(int index, const juce::String &newName)
{
}

void MDADubDelayAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    _size = _maxBufferSize;
    _buffer.resize(_size + 2); // spare just in case
    resetState();
}

void MDADubDelayAudioProcessor::releaseResources()
{
}

void MDADubDelayAudioProcessor::reset()
{
    resetState();
}

bool MDADubDelayAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MDADubDelayAudioProcessor::resetState()
{
    _ipos = 0;
    _fil0 = 0.0f;
    _env = 0.0f;
    _phi = 0.0f;
    _dlbuf = 0.0f;

    memset(_buffer.data(), 0, (_size + 2) * sizeof(float));
}

void MDADubDelayAudioProcessor::update()
{
    float fs = float(getSampleRate());
    if (fs < 8000.0f) fs = 44100.0f;

    float fParam0 = apvts.getRawParameterValue("Delay")->load();
    float fParam1 = apvts.getRawParameterValue("Feedback")->load();
    float fParam2 = apvts.getRawParameterValue("Fb Tone")->load();
    float fParam3 = apvts.getRawParameterValue("LFO Dep.")->load();
    float fParam4 = apvts.getRawParameterValue("LFO Rate")->load();
    float fParam5 = apvts.getRawParameterValue("FX Mix")->load();
    float fParam6 = apvts.getRawParameterValue("Output")->load();

    _del = fParam0 * fParam0 * float(_size);
    _mod = 0.049f * fParam3 * _del;

    _fil = fParam2;
    if (fParam2 > 0.5f) // simultaneously change crossover frequency & high/low mix
    {
        _fil = 0.5f * _fil - 0.25f;
        _lmix = -2.0f * _fil;
        _hmix = 1.0f;
    }
    else
    {
        _hmix = 2.0f * _fil;
        _lmix = 1.0f - _hmix;
    }
    _fil = std::exp(-6.2831853f * std::pow(10.0f, 2.2f + 4.5f * _fil) / fs);

    _fbk = std::abs(2.2f * fParam1 - 1.1f);
    if (fParam1 > 0.5f) _rel = 0.9997f; else _rel = 0.8f; // limit or clip

    _wet = 1.0f - fParam5;
    _wet = fParam6 * (1.0f - _wet * _wet); // -3dB at 50% mix
    _dry = fParam6 * 2.0f * (1.0f - fParam5 * fParam5);

    _dphi = 628.31853f * std::pow(10.0f, 3.0f * fParam4 - 2.0f) / fs; // 100-sample steps
}

void MDADubDelayAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
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

    float w = _wet, y = _dry, fb = _fbk;
    float dl = _dlbuf, db = _dlbuf, ddl = 0.0f;
    float lx = _lmix, hx = _hmix, f = _fil, f0 = _fil0, tmp;
    float e = _env, g, r = _rel;
    float twopi = 6.2831853f;
    int i = _ipos, s = _size, k = 0;

    for (int n = 0; n < buffer.getNumSamples(); ++n)
    {
        float a = in1[n];
        float b = in2[n];

        if (k == 0) // update delay length at slower rate
        {
            db += 0.01f * (_del - db - _mod - _mod * std::sin(_phi)); // smoothed delay+lfo
            ddl = 0.01f * (db - dl); // linear step
            _phi += _dphi;
            if (_phi > twopi) _phi -= twopi;
            k = 100;
        }
        k--;
        dl += ddl; // lin interp between points

        i--;
        if (i < 0) i = s; // delay positions

        int l = (int)dl;
        tmp = dl - (float)l; // remainder
        l += i;
        if (l > s) l -= (s + 1);

        float ol = _buffer[l]; // delay output

        l++;
        if (l > s) l = 0;
        ol += tmp * (_buffer[l] - ol); // lin interp

        tmp = a + fb * ol; // mix input (left only!) & feedback

        f0 = f * (f0 - tmp) + tmp; // low-pass filter
        tmp = lx * f0 + hx * tmp;

        g = (tmp < 0.0f) ? -tmp : tmp; // simple limiter
        e *= r;
        if (g > e) e = g;
        if (e > 1.0f) tmp /= e;

        _buffer[i] = tmp; // delay input

        ol *= w; // wet

        out1[n] = y * a + ol; // dry
        out2[n] = y * b + ol;
    }

    _ipos = i;
    _dlbuf = dl;

    // Trap denormals
    if (std::abs(f0) < 1.0e-10f) { _fil0 = 0.0f; _env = 0.0f; }
    else { _fil0 = f0; _env = e; }
}

juce::AudioProcessorEditor *MDADubDelayAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

void MDADubDelayAudioProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    copyXmlToBinary(*apvts.copyState().createXml(), destData);
}

void MDADubDelayAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml.get() != nullptr && xml->hasTagName(apvts.state.getType())) {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout MDADubDelayAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Delay: 0-1 (squared for delay time), display in ms
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Delay", 1),
        "Delay",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        0.30f,
        juce::AudioParameterFloatAttributes()
            .withLabel("ms")
            .withStringFromValueFunction(
                [](float value, int) {
                    // del = value^2 * size; display = del * 1000 / sampleRate
                    // We approximate with 44100 since we don't have sampleRate here.
                    float del = value * value * 323766.0f;
                    return juce::String(int(del * 1000.0f / 44100.0f));
                }
            )));

    // Feedback: 0-1, display as -110 to +110
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Feedback", 1),
        "Feedback",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        0.70f,
        juce::AudioParameterFloatAttributes()
            .withLabel("Sat<>Lim")
            .withStringFromValueFunction(
                [](float value, int) { return juce::String(int(220.0f * value - 110.0f)); }
            )));

    // Fb Tone: 0-1, display as -100 to +100
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Fb Tone", 1),
        "Fb Tone",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        0.40f,
        juce::AudioParameterFloatAttributes()
            .withLabel("Lo <> Hi")
            .withStringFromValueFunction(
                [](float value, int) { return juce::String(int(200.0f * value - 100.0f)); }
            )));

    // LFO Depth: 0-1, display as 0-100%
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("LFO Dep.", 1),
        "LFO Dep.",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        0.00f,
        juce::AudioParameterFloatAttributes()
            .withLabel("%")
            .withStringFromValueFunction(
                [](float value, int) { return juce::String(int(100.0f * value)); }
            )));

    // LFO Rate: 0-1, display in seconds
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("LFO Rate", 1),
        "LFO Rate",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        0.50f,
        juce::AudioParameterFloatAttributes()
            .withLabel("sec.")
            .withStringFromValueFunction(
                [](float value, int) {
                    return juce::String(std::pow(10.0f, 2.0f - 3.0f * value), 2);
                }
            )));

    // FX Mix: 0-1, display as 0-100%
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("FX Mix", 1),
        "FX Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        0.33f,
        juce::AudioParameterFloatAttributes()
            .withLabel("%")
            .withStringFromValueFunction(
                [](float value, int) { return juce::String(int(100.0f * value)); }
            )));

    // Output: 0-1, display in dB
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Output", 1),
        "Output",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        0.50f,
        juce::AudioParameterFloatAttributes()
            .withLabel("dB")
            .withStringFromValueFunction(
                [](float value, int) {
                    return juce::String(int(20.0f * std::log10(2.0f * value)));
                }
            )));

    return layout;
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new MDADubDelayAudioProcessor();
}
