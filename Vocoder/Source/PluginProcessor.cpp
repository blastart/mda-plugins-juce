#include "PluginProcessor.h"

MDAVocoderAudioProcessor::MDAVocoderAudioProcessor()
: AudioProcessor(BusesProperties()
                 .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
                 .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

MDAVocoderAudioProcessor::~MDAVocoderAudioProcessor()
{
}

const juce::String MDAVocoderAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

int MDAVocoderAudioProcessor::getNumPrograms()
{
    return 1;
}

int MDAVocoderAudioProcessor::getCurrentProgram()
{
    return 0;
}

void MDAVocoderAudioProcessor::setCurrentProgram(int index)
{
}

const juce::String MDAVocoderAudioProcessor::getProgramName(int index)
{
    return {};
}

void MDAVocoderAudioProcessor::changeProgramName(int index, const juce::String &newName)
{
}

void MDAVocoderAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    resetState();
    update();
}

void MDAVocoderAudioProcessor::releaseResources()
{
}

void MDAVocoderAudioProcessor::reset()
{
    resetState();
}

bool MDAVocoderAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MDAVocoderAudioProcessor::resetState()
{
    for (int i = 0; i < NBANDS; i++)
        for (int j = 3; j < 12; j++)
            _f[i][j] = 0.0f;

    _kout = 0.0f;
    _kval = 0;
}

void MDAVocoderAudioProcessor::update()
{
    double tpofs = 6.2831853 / getSampleRate();
    double rr, th, re;
    float sh;

    // Mod In: parameter is 0..1, swap if > 0.5
    float modInParam = apvts.getRawParameterValue("Mod In")->load();
    _swap = (modInParam <= 0.5f) ? 1 : 0;

    // Output: parameter is in dB, range -20..+20. Original: param[1] 0..1, gain = pow(10, 2*p1-3*p5-2)
    float outputParam = apvts.getRawParameterValue("Output")->load();
    float filterQParam = apvts.getRawParameterValue("Filter Q")->load();
    // Map output from dB display: original displayed as (40*p1-20), so p1 = (outputParam+20)/40
    float p1 = (outputParam + 20.0f) / 40.0f;
    // Map filterQ from percent: original displayed as (100*p5), so p5 = filterQParam/100
    float p5 = filterQParam / 100.0f;

    _gain = (float)std::pow(10.0f, 2.0f * p1 - 3.0f * p5 - 2.0f);

    _thru = (float)std::pow(10.0f, 0.5f + 2.0f * p1);

    // Hi Band: parameter 0..100%, original p3 = param[3]
    float hiBandParam = apvts.getRawParameterValue("Hi Band")->load();
    float p3 = hiBandParam / 100.0f;
    _high = p3 * p3 * p3 * _thru;

    // Hi Thru: parameter 0..100%, original p2 = param[2]
    float hiThruParam = apvts.getRawParameterValue("Hi Thru")->load();
    float p2 = hiThruParam / 100.0f;
    _thru *= p2 * p2 * p2;

    // Quality (num bands): parameter is a choice, 0 = 8 band, 1 = 16 band
    float qualityParam = apvts.getRawParameterValue("Quality")->load();
    // Original: param[7]<0.5 => 8 bands, else 16 bands
    if (qualityParam < 0.5f)
    {
        _nbnd = 8;
        re = 0.003f;
        _f[1][2] = 3000.0f;
        _f[2][2] = 2200.0f;
        _f[3][2] = 1500.0f;
        _f[4][2] = 1080.0f;
        _f[5][2] = 700.0f;
        _f[6][2] = 390.0f;
        _f[7][2] = 190.0f;
    }
    else
    {
        _nbnd = 16;
        re = 0.0015f;
        _f[ 1][2] = 5000.0f;
        _f[ 2][2] = 4000.0f;
        _f[ 3][2] = 3250.0f;
        _f[ 4][2] = 2750.0f;
        _f[ 5][2] = 2300.0f;
        _f[ 6][2] = 2000.0f;
        _f[ 7][2] = 1750.0f;
        _f[ 8][2] = 1500.0f;
        _f[ 9][2] = 1250.0f;
        _f[10][2] = 1000.0f;
        _f[11][2] =  750.0f;
        _f[12][2] =  540.0f;
        _f[13][2] =  350.0f;
        _f[14][2] =  195.0f;
        _f[15][2] =   95.0f;
    }

    // Envelope: parameter is in ms display. Original param[4] 0..1.
    // Display: if p4 < 0.05 => "FREEZE", else pow(10, 1+3*p4) ms
    // We store as 0..1 to keep the DSP math identical.
    float envelopeParam = apvts.getRawParameterValue("Envelope")->load();
    float p4 = envelopeParam / 100.0f;

    if (p4 < 0.05f) // freeze
    {
        for (int i = 0; i < _nbnd; i++) _f[i][12] = 0.0f;
    }
    else
    {
        _f[0][12] = (float)std::pow(10.0, -1.7 - 2.7 * p4); // envelope speed

        double rrEnv = 0.022 / (double)_nbnd;
        for (int i = 1; i < _nbnd; i++)
        {
            _f[i][12] = (float)(0.025 - rrEnv * (double)i);
            if (_f[0][12] < _f[i][12]) _f[i][12] = _f[0][12];
        }
        _f[0][12] = 0.5f * _f[0][12]; // only top band is at full rate
    }

    rr = 1.0 - std::pow(10.0f, -1.0f - 1.2f * p5);

    // Mid Freq: parameter is in Hz display. Original param[6] 0..1.
    // Display: 800 * pow(2, 3*p6-2) Hz. We store as 0..1.
    float midFreqParam = apvts.getRawParameterValue("Mid Freq")->load();
    float p6 = midFreqParam / 100.0f;
    sh = (float)std::pow(2.0f, 3.0f * p6 - 1.0f); // filter bank range shift

    for (int i = 1; i < _nbnd; i++)
    {
        _f[i][2] *= sh;
        th = std::acos((2.0 * rr * std::cos(tpofs * _f[i][2])) / (1.0 + rr * rr));
        _f[i][0] = (float)(2.0 * rr * std::cos(th)); // a0
        _f[i][1] = (float)(-rr * rr);                 // a1

        _f[i][2] *= 0.96f; // shift 2nd stage slightly to stop high resonance peaks
        th = std::acos((2.0 * rr * std::cos(tpofs * _f[i][2])) / (1.0 + rr * rr));
        _f[i][2] = (float)(2.0 * rr * std::cos(th));
    }
}

void MDAVocoderAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
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

    float a, b, o = 0.0f, aa, bb, oo = _kout, g = _gain, ht = _thru, hh = _high, tmp;
    int k = _kval, sw = _swap, nb = _nbnd;

    for (int n = 0; n < buffer.getNumSamples(); ++n)
    {
        a = in1[n]; // speech (modulator)
        b = in2[n]; // synth (carrier)
        if (sw == 0) { tmp = a; a = b; b = tmp; } // swap channels

        tmp = a - _f[0][7]; // integrate modulator for HF band and filter bank pre-emphasis
        _f[0][7] = a;
        a = tmp;

        if (tmp < 0.0f) tmp = -tmp;
        _f[0][11] -= _f[0][12] * (_f[0][11] - tmp);        // high band envelope
        o = _f[0][11] * (ht * a + hh * (b - _f[0][3]));    // high band + high thru

        _f[0][3] = b; // integrate carrier for HF band

        if (++k & 0x1) // this block runs at half sample rate
        {
            oo = 0.0f;
            aa = a + _f[0][9] - _f[0][8] - _f[0][8]; // apply zeros
            _f[0][9] = _f[0][8]; _f[0][8] = a;
            bb = b + _f[0][5] - _f[0][4] - _f[0][4];
            _f[0][5] = _f[0][4]; _f[0][4] = b;

            for (int i = 1; i < nb; i++) // filter bank: 4th-order band pass
            {
                tmp = _f[i][0] * _f[i][3] + _f[i][1] * _f[i][4] + bb;
                _f[i][4] = _f[i][3];
                _f[i][3] = tmp;
                tmp += _f[i][2] * _f[i][5] + _f[i][1] * _f[i][6];
                _f[i][6] = _f[i][5];
                _f[i][5] = tmp;

                tmp = _f[i][0] * _f[i][7] + _f[i][1] * _f[i][8] + aa;
                _f[i][8] = _f[i][7];
                _f[i][7] = tmp;
                tmp += _f[i][2] * _f[i][9] + _f[i][1] * _f[i][10];
                _f[i][10] = _f[i][9];
                _f[i][9] = tmp;

                if (tmp < 0.0f) tmp = -tmp;
                _f[i][11] -= _f[i][12] * (_f[i][11] - tmp);
                oo += _f[i][5] * _f[i][11];
            }
        }
        o += oo * g;

        out1[n] = o;
        out2[n] = o;
    }

    _kout = oo;
    _kval = k & 0x1;

    // Catch denormals.
    if (std::fabs(_f[0][11]) < 1.0e-10) _f[0][11] = 0.0f;

    for (int i = 1; i < nb; i++)
        if (std::fabs(_f[i][3]) < 1.0e-10 || std::fabs(_f[i][7]) < 1.0e-10)
            for (int j = 3; j < 12; j++) _f[i][j] = 0.0f;

    if (std::fabs(o) > 10.0f) resetState(); // catch instability
}

juce::AudioProcessorEditor *MDAVocoderAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

void MDAVocoderAudioProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    copyXmlToBinary(*apvts.copyState().createXml(), destData);
}

void MDAVocoderAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml.get() != nullptr && xml->hasTagName(apvts.state.getType())) {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout MDAVocoderAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Mod In: 0..1, displayed as LEFT or RIGHT. Original param[0] default 0.33.
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Mod In", 1),
        "Mod In",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        0.33f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction(
                [](float value, int) { return value <= 0.5f ? "RIGHT" : "LEFT"; }
            )));

    // Output: original param[1] 0..1, displayed as (40*p-20) dB. Default 0.5 => 0 dB.
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Output", 1),
        "Output",
        juce::NormalisableRange<float>(-20.0f, 20.0f, 0.1f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    // Hi Thru: original param[2] 0..1, displayed as (100*p) %. Default 0.40 => 40%.
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Hi Thru", 1),
        "Hi Thru",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        40.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    // Hi Band: original param[3] 0..1, displayed as (100*p) %. Default 0.40 => 40%.
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Hi Band", 1),
        "Hi Band",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        40.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    // Envelope: original param[4] 0..1, displayed as pow(10,1+3*p) ms or FREEZE. Default 0.16 => 16%.
    // We store as 0..100 (percent), internally divide by 100 to get 0..1.
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Envelope", 1),
        "Envelope",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        16.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel("ms")
            .withStringFromValueFunction(
                [](float value, int) {
                    float p = value / 100.0f;
                    if (p < 0.05f) return juce::String("FREEZE");
                    return juce::String(std::pow(10.0f, 1.0f + 3.0f * p), 1);
                }
            )));

    // Filter Q: original param[5] 0..1, displayed as (100*p) %. Default 0.55 => 55%.
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Filter Q", 1),
        "Filter Q",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        55.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    // Mid Freq: original param[6] 0..1, displayed as 800*pow(2,3*p-2) Hz. Default 0.6667 => 66.67%.
    // We store as 0..100 (percent), internally divide by 100 to get 0..1.
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Mid Freq", 1),
        "Mid Freq",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        66.67f,
        juce::AudioParameterFloatAttributes()
            .withLabel("Hz")
            .withStringFromValueFunction(
                [](float value, int) {
                    float p = value / 100.0f;
                    return juce::String(800.0f * std::pow(2.0f, 3.0f * p - 2.0f), 0);
                }
            )));

    // Quality: original param[7], < 0.5 = 8 band, >= 0.5 = 16 band. Default 0.33 => 8 band.
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Quality", 1),
        "Quality",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        0.33f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction(
                [](float value, int) { return value < 0.5f ? "8 BAND" : "16 BAND"; }
            )));

    return layout;
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new MDAVocoderAudioProcessor();
}
