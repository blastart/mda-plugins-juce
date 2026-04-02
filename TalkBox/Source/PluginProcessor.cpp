#include "PluginProcessor.h"

MDATalkBoxAudioProcessor::MDATalkBoxAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

MDATalkBoxAudioProcessor::~MDATalkBoxAudioProcessor()
{
}

const juce::String MDATalkBoxAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

int MDATalkBoxAudioProcessor::getNumPrograms()
{
    return 1;
}

int MDATalkBoxAudioProcessor::getCurrentProgram()
{
    return 0;
}

void MDATalkBoxAudioProcessor::setCurrentProgram(int index)
{
}

const juce::String MDATalkBoxAudioProcessor::getProgramName(int index)
{
    return {};
}

void MDATalkBoxAudioProcessor::changeProgramName(int index, const juce::String &newName)
{
}

void MDATalkBoxAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    _buf0.resize(BUF_MAX);
    _buf1.resize(BUF_MAX);
    _window.resize(BUF_MAX);
    _car0.resize(BUF_MAX);
    _car1.resize(BUF_MAX);

    _N = 1; // trigger window recalc on first update()

    resetState();
}

void MDATalkBoxAudioProcessor::releaseResources()
{
}

void MDATalkBoxAudioProcessor::reset()
{
    resetState();
}

bool MDATalkBoxAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MDATalkBoxAudioProcessor::resetState()
{
    _pos = 0;
    _K = 0;
    _emphasis = 0.0f;
    _FX = 0.0f;

    _u0 = _u1 = _u2 = _u3 = _u4 = 0.0f;
    _d0 = _d1 = _d2 = _d3 = _d4 = 0.0f;

    if (!_buf0.empty()) memset(_buf0.data(), 0, BUF_MAX * sizeof(float));
    if (!_buf1.empty()) memset(_buf1.data(), 0, BUF_MAX * sizeof(float));
    if (!_car0.empty()) memset(_car0.data(), 0, BUF_MAX * sizeof(float));
    if (!_car1.empty()) memset(_car1.data(), 0, BUF_MAX * sizeof(float));
}

void MDATalkBoxAudioProcessor::update()
{
    float fs = (float)getSampleRate();
    if (fs < 8000.0f) fs = 8000.0f;
    if (fs > 96000.0f) fs = 96000.0f;

    float wetParam = apvts.getRawParameterValue("Wet")->load() / 200.0f;
    float dryParam = apvts.getRawParameterValue("Dry")->load() / 200.0f;
    float swapParam = apvts.getRawParameterValue("Carrier")->load();
    float qualityParam = apvts.getRawParameterValue("Quality")->load() / 100.0f;

    _swap = (swapParam > 0.5f) ? 1 : 0;

    int n = (int)(0.01633f * fs);
    if (n > BUF_MAX) n = BUF_MAX;

    _O = (int)((0.0001f + 0.0004f * qualityParam) * fs);

    if (n != _N) // recalc hanning window
    {
        _N = n;
        float dp = TWO_PI_F / (float)_N;
        float p = 0.0f;
        for (int i = 0; i < _N; i++)
        {
            _window[i] = 0.5f - 0.5f * std::cos(p);
            p += dp;
        }
    }

    _wet = 0.5f * wetParam * wetParam;
    _dry = 2.0f * dryParam * dryParam;
}

void MDATalkBoxAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    update();

    const float *in1 = buffer.getReadPointer(_swap ? 1 : 0);
    const float *in2 = buffer.getReadPointer(_swap ? 0 : 1);
    float *out1 = buffer.getWritePointer(0);
    float *out2 = buffer.getWritePointer(1);

    int p0 = _pos;
    int p1 = (_pos + _N / 2) % _N;
    float e = _emphasis;
    float fx = _FX;
    const float h0 = 0.3f;
    const float h1 = 0.77f;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        float o = in1[i];
        float x = in2[i];
        float dr = o;

        // Allpass filters on carrier input
        float p = _d0 + h0 * x;  _d0 = _d1;  _d1 = x - h0 * p;
        float q = _d2 + h1 * _d4; _d2 = _d3; _d3 = _d4 - h1 * q;
        _d4 = x;
        x = p + q;

        if (_K++)
        {
            _K = 0;

            _car0[p0] = _car1[p1] = x; // carrier input

            x = o - e;  e = o;  // 6dB/oct pre-emphasis

            float w = _window[p0];
            fx = _buf0[p0] * w;
            _buf0[p0] = x * w;  // 50% overlapping hanning windows
            if (++p0 >= _N) { lpc(_buf0.data(), _car0.data(), _N, _O);  p0 = 0; }

            w = 1.0f - w;
            fx += _buf1[p1] * w;
            _buf1[p1] = x * w;
            if (++p1 >= _N) { lpc(_buf1.data(), _car1.data(), _N, _O);  p1 = 0; }
        }

        // Allpass filters on output
        p = _u0 + h0 * fx; _u0 = _u1; _u1 = fx - h0 * p;
        q = _u2 + h1 * _u4; _u2 = _u3; _u3 = _u4 - h1 * q;
        _u4 = fx;
        x = p + q;

        o = _wet * x + _dry * dr;
        out1[i] = o;
        out2[i] = o;
    }

    _emphasis = e;
    _pos = p0;
    _FX = fx;

    // Anti-denormal
    const float den = 1.0e-10f;
    if (std::abs(_d0) < den) _d0 = 0.0f;
    if (std::abs(_d1) < den) _d1 = 0.0f;
    if (std::abs(_d2) < den) _d2 = 0.0f;
    if (std::abs(_d3) < den) _d3 = 0.0f;
    if (std::abs(_u0) < den) _u0 = 0.0f;
    if (std::abs(_u1) < den) _u1 = 0.0f;
    if (std::abs(_u2) < den) _u2 = 0.0f;
    if (std::abs(_u3) < den) _u3 = 0.0f;
}

void MDATalkBoxAudioProcessor::lpc(float *buf, float *car, int n, int o)
{
    float z[ORD_MAX], r[ORD_MAX], k[ORD_MAX], G, x;
    int nn = n;

    for (int j = 0; j <= o; j++, nn--)
    {
        z[j] = r[j] = 0.0f;
        for (int i = 0; i < nn; i++) r[j] += buf[i] * buf[i + j]; // autocorrelation
    }
    r[0] *= 1.001f; // stability fix

    const float minVal = 0.00001f;
    if (r[0] < minVal) { for (int i = 0; i < n; i++) buf[i] = 0.0f; return; }

    lpcDurbin(r, o, k, &G); // calc reflection coeffs

    for (int i = 0; i <= o; i++)
    {
        if (k[i] > 0.995f) k[i] = 0.995f;
        else if (k[i] < -0.995f) k[i] = -0.995f;
    }

    for (int i = 0; i < n; i++)
    {
        x = G * car[i];
        for (int j = o; j > 0; j--) // lattice filter
        {
            x -= k[j] * z[j - 1];
            z[j] = z[j - 1] + k[j] * x;
        }
        buf[i] = z[0] = x; // output buf[] will be windowed elsewhere
    }
}

void MDATalkBoxAudioProcessor::lpcDurbin(float *r, int p, float *k, float *g)
{
    float a[ORD_MAX], at[ORD_MAX], e = r[0];

    for (int i = 0; i <= p; i++) a[i] = at[i] = 0.0f;

    for (int i = 1; i <= p; i++)
    {
        k[i] = -r[i];

        for (int j = 1; j < i; j++)
        {
            at[j] = a[j];
            k[i] -= a[j] * r[i - j];
        }
        if (std::abs(e) < 1.0e-20f) { e = 0.0f; break; }
        k[i] /= e;

        a[i] = k[i];
        for (int j = 1; j < i; j++) a[j] = at[j] + k[i] * at[i - j];

        e *= 1.0f - k[i] * k[i];
    }

    if (e < 1.0e-20f) e = 0.0f;
    *g = std::sqrt(e);
}

juce::AudioProcessorEditor *MDATalkBoxAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

void MDATalkBoxAudioProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    copyXmlToBinary(*apvts.copyState().createXml(), destData);
}

void MDATalkBoxAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml.get() != nullptr && xml->hasTagName(apvts.state.getType())) {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout MDATalkBoxAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Wet", 1),
        "Wet",
        juce::NormalisableRange<float>(0.0f, 200.0f, 0.01f),
        100.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Dry", 1),
        "Dry",
        juce::NormalisableRange<float>(0.0f, 200.0f, 0.01f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Carrier", 1),
        "Carrier",
        juce::NormalisableRange<float>(0.0f, 1.0f, 1.0f),
        0.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction(
                [](float value, int) { return value > 0.5f ? juce::String("LEFT") : juce::String("RIGHT"); }
            )));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Quality", 1),
        "Quality",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f),
        100.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    return layout;
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new MDATalkBoxAudioProcessor();
}
