#include "PluginProcessor.h"

static constexpr float SILENCE = 0.00000001f;

MDASpecMeterAudioProcessor::MDASpecMeterAudioProcessor()
: AudioProcessor(BusesProperties()
                 .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
                 .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    _K = 0;
    _kmax = 2048;
    _topband = 11;
    _iK = 1.0f / (float)_kmax;
    _den = 1.0e-8f;
    _gain = 1.0f;

    memset(band, 0, sizeof(band));
    memset(_lpp, 0, sizeof(_lpp));
    memset(_rpp, 0, sizeof(_rpp));
}

MDASpecMeterAudioProcessor::~MDASpecMeterAudioProcessor()
{
}

const juce::String MDASpecMeterAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

int MDASpecMeterAudioProcessor::getNumPrograms()
{
    return 1;
}

int MDASpecMeterAudioProcessor::getCurrentProgram()
{
    return 0;
}

void MDASpecMeterAudioProcessor::setCurrentProgram(int index)
{
}

const juce::String MDASpecMeterAudioProcessor::getProgramName(int index)
{
    return {};
}

void MDASpecMeterAudioProcessor::changeProgramName(int index, const juce::String &newName)
{
}

void MDASpecMeterAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    if (sampleRate > 64000.0) {
        _topband = 12;
        _kmax = 4096;
    } else {
        _topband = 11;
        _kmax = 2048;
    }
    _iK = 1.0f / (float)_kmax;

    resetState();
}

void MDASpecMeterAudioProcessor::releaseResources()
{
}

void MDASpecMeterAudioProcessor::reset()
{
    resetState();
}

bool MDASpecMeterAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MDASpecMeterAudioProcessor::resetState()
{
    Lpeak.store(0.0f);
    Rpeak.store(0.0f);
    Lrms.store(0.0f);
    Rrms.store(0.0f);
    Corr.store(0.0f);
    Lhold.store(0.0f);
    Rhold.store(0.0f);
    Lmin.store(0.0000001f);
    Rmin.store(0.0000001f);

    _lpeak = _rpeak = _lrms = _rrms = _corr = 0.0f;
    _lmin = _rmin = 0.0000001f;
    _K = 0;
    counter.store(0);

    memset(band, 0, sizeof(band));
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 6; j++) {
            _lpp[j][i] = 0.0f;
            _rpp[j][i] = 0.0f;
        }
    }
}

void MDASpecMeterAudioProcessor::update()
{
    float gainParam = apvts.getRawParameterValue("Gain")->load();
    _gain = std::pow(10.0f, 2.0f * gainParam - 1.0f);
}

void MDASpecMeterAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any output channels that don't contain input data.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i) {
        buffer.clear(i, 0, buffer.getNumSamples());
    }

    update();

    const float *in1 = buffer.getReadPointer(0);
    const float *in2 = buffer.getReadPointer(1);
    float *out1 = buffer.getWritePointer(0);
    float *out2 = buffer.getWritePointer(1);

    _den = -_den;
    float den = _den;
    float iN = _iK;
    int k = _K;
    int j0 = _topband;

    float lpeak_local = _lpeak;
    float rpeak_local = _rpeak;
    float lrms_local = _lrms;
    float rrms_local = _rrms;
    float corr_local = _corr;
    float lmin_local = _lmin;
    float rmin_local = _rmin;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        float l = in1[i];
        float r = in2[i];

        // Pass through (this is an analyzer, not an effect)
        out1[i] = l;
        out2[i] = r;

        l += den;  // anti-denormal
        r += den;

        // RMS integrate
        lrms_local += l * l;
        rrms_local += r * r;

        // Peak detect
        float p = std::fabs(l);
        if (p > lpeak_local) lpeak_local = p;
        float q = std::fabs(r);
        if (q > rpeak_local) rpeak_local = q;

        // Measure correlation
        if ((l * r) > 0.0f) corr_local += iN;

        // Polyphase filter bank
        int j = j0;
        int mask = k << 1;

        do
        {
            p = _lpp[0][j] + 0.208f * l;
            _lpp[0][j] = _lpp[1][j];
            _lpp[1][j] = l - 0.208f * p;

            q = _lpp[2][j] + _lpp[4][j] * 0.682f;
            _lpp[2][j] = _lpp[3][j];
            _lpp[3][j] = _lpp[4][j] - 0.682f * q;
            _lpp[4][j] = l;
            _lpp[5][j] += std::fabs(p - q);  // top octave
            l = p + q;                         // lower octaves

            p = _rpp[0][j] + 0.208f * r;
            _rpp[0][j] = _rpp[1][j];
            _rpp[1][j] = r - 0.208f * p;

            q = _rpp[2][j] + _rpp[4][j] * 0.682f;
            _rpp[2][j] = _rpp[3][j];
            _rpp[3][j] = _rpp[4][j] - 0.682f * q;
            _rpp[4][j] = r;
            _rpp[5][j] += std::fabs(p - q);  // top octave
            r = p + q;                         // lower octaves

            j--;
            mask >>= 1;
        } while (mask & 1);

        if (++k == _kmax)
        {
            k = 0;
            counter.store(counter.load() + 1);

            // Left channel peak/RMS
            if (lpeak_local == 0.0f)
            {
                Lpeak.store(0.0f);
                Lrms.store(0.0f);
            }
            else
            {
                if (lpeak_local > 2.0f) lpeak_local = 2.0f;
                float lp = Lpeak.load();
                float lh = Lhold.load();
                if (lpeak_local >= lp)
                {
                    lp = lpeak_local;
                    lh = 2.0f * lp;
                }
                else
                {
                    lh *= 0.95f;
                    if (lh < lp) lp = lh;
                }
                Lpeak.store(lp);
                Lhold.store(lh);
                Lmin.store(lmin_local);
                lmin_local *= 1.01f;
                float lr = Lrms.load();
                lr += 0.2f * (iN * lrms_local - lr);
                Lrms.store(lr);
            }

            // Right channel peak/RMS
            if (rpeak_local == 0.0f)
            {
                Rpeak.store(0.0f);
                Rrms.store(0.0f);
            }
            else
            {
                if (rpeak_local > 2.0f) rpeak_local = 2.0f;
                float rp = Rpeak.load();
                float rh = Rhold.load();
                if (rpeak_local >= rp)
                {
                    rp = rpeak_local;
                    rh = 2.0f * rp;
                }
                else
                {
                    rh *= 0.95f;
                    if (rh < rp) rp = rh;
                }
                Rpeak.store(rp);
                Rhold.store(rh);
                Rmin.store(rmin_local);
                rmin_local *= 1.01f;
                float rr = Rrms.load();
                rr += 0.2f * (iN * rrms_local - rr);
                Rrms.store(rr);
            }

            rpeak_local = lpeak_local = lrms_local = rrms_local = 0.0f;

            // Correlation
            float c = Corr.load();
            c += 0.1f * (corr_local - c);
            Corr.store(c);
            corr_local = SILENCE;

            // Spectrum output
            float dec = 0.08f;
            for (int b = 0; b < 13; b++)
            {
                band[0][b] += dec * (iN * _lpp[5][b] - band[0][b]);
                if (band[0][b] > 2.0f) band[0][b] = 2.0f;
                else if (band[0][b] < 0.014f) band[0][b] = 0.014f;

                band[1][b] += dec * (iN * _rpp[5][b] - band[1][b]);
                if (band[1][b] > 2.0f) band[1][b] = 2.0f;
                else if (band[1][b] < 0.014f) band[1][b] = 0.014f;

                _rpp[5][b] = _lpp[5][b] = SILENCE;
                dec = dec * 1.1f;
            }
        }
    }

    _K = k;
    _lpeak = lpeak_local;
    _rpeak = rpeak_local;
    _lrms = lrms_local;
    _rrms = rrms_local;
    _corr = corr_local;
    _lmin = lmin_local;
    _rmin = rmin_local;
}

juce::AudioProcessorEditor *MDASpecMeterAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

void MDASpecMeterAudioProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    copyXmlToBinary(*apvts.copyState().createXml(), destData);
}

void MDASpecMeterAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml.get() != nullptr && xml->hasTagName(apvts.state.getType())) {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout MDASpecMeterAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Gain: input gain for metering, maps to -20dB..+20dB via pow(10, 2*x - 1)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Gain", 1),
        "Gain",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.5f,
        juce::AudioParameterFloatAttributes()
            .withLabel("dB")
            .withStringFromValueFunction(
                [](float value, int) { return juce::String(40.0f * value - 20.0f, 1); }
            )));

    // RMS Speed: controls RMS smoothing (exposed for potential use)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("RMS Speed", 1),
        "RMS Speed",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.5f,
        juce::AudioParameterFloatAttributes()));

    // Spectrum Speed: controls spectrum decay (exposed for potential use)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Spec Speed", 1),
        "Spec Speed",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.75f,
        juce::AudioParameterFloatAttributes()));

    return layout;
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new MDASpecMeterAudioProcessor();
}
