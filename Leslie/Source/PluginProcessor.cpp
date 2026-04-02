#include "PluginProcessor.h"

MDALeslieAudioProcessor::MDALeslieAudioProcessor()
: AudioProcessor(BusesProperties()
                 .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
                 .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

MDALeslieAudioProcessor::~MDALeslieAudioProcessor()
{
}

const juce::String MDALeslieAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

int MDALeslieAudioProcessor::getNumPrograms()
{
    return 3;
}

int MDALeslieAudioProcessor::getCurrentProgram()
{
    return 0;
}

void MDALeslieAudioProcessor::setCurrentProgram(int index)
{
}

const juce::String MDALeslieAudioProcessor::getProgramName(int index)
{
    switch (index) {
        case 0: return "Leslie Simulator";
        case 1: return "Slow";
        case 2: return "Fast";
        default: return {};
    }
}

void MDALeslieAudioProcessor::changeProgramName(int index, const juce::String &newName)
{
}

void MDALeslieAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    _hbuf.resize(_bufferSize);
    resetState();
}

void MDALeslieAudioProcessor::releaseResources()
{
}

void MDALeslieAudioProcessor::reset()
{
    resetState();
}

bool MDALeslieAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MDALeslieAudioProcessor::resetState()
{
    _lspd = 0.0f;
    _hspd = 0.0f;
    _lphi = 0.0f;
    _hphi = 1.6f;
    _hpos = 0;
    _fbuf1 = 0.0f;
    _fbuf2 = 0.0f;

    memset(_hbuf.data(), 0, _bufferSize * sizeof(float));
}

void MDALeslieAudioProcessor::update()
{
    float ifs = 1.0f / float(getSampleRate());

    // Speed multiplier from the Speed parameter (0-200%, default 100%).
    _spd = _twoPi * ifs * apvts.getRawParameterValue("Speed")->load() * 0.01f;

    // Crossover filter coefficient. The Crossover parameter is in Hz (150-1500).
    // We normalize it to 0-1 range for the filter formula.
    float crossover = apvts.getRawParameterValue("Crossover")->load();
    float crossNorm = (crossover - 150.0f) / (1500.0f - 150.0f);
    _filo = 1.0f - std::pow(10.0f, crossNorm * (2.27f - 0.54f * crossNorm) - 1.92f);

    // Speed mode: 0=stop, 1=slow, 2=fast. Determines rotor target speeds
    // and momentum (how quickly rotors accelerate/decelerate).
    int speedMode = int(apvts.getRawParameterValue("Speed Mode")->load());
    switch (speedMode) {
        case 0: // stop
            _lset = 0.0f; _hset = 0.0f;
            _lmom = 0.12f; _hmom = 0.1f;
            break;
        case 1: // slow
            _lset = 0.49f; _hset = 0.66f;
            _lmom = 0.27f; _hmom = 0.18f;
            break;
        case 2: // fast
        default:
            _lset = 5.31f; _hset = 6.40f;
            _lmom = 0.14f; _hmom = 0.09f;
            break;
    }

    // Convert momentum from time constants to per-sample coefficients.
    _hmom = std::pow(10.0f, -ifs / _hmom);
    _lmom = std::pow(10.0f, -ifs / _lmom);
    _hset *= _spd;
    _lset *= _spd;

    // Output gain.
    float outputDb = apvts.getRawParameterValue("Output")->load();
    _gain = 0.4f * std::pow(10.0f, outputDb / 20.0f);

    // Low rotor parameters.
    _lwid = apvts.getRawParameterValue("Lo Width")->load() * 0.01f;
    _lwid *= _lwid;
    _llev = apvts.getRawParameterValue("Lo Throb")->load() * 0.01f;
    _llev = _gain * 0.9f * _llev * _llev;

    // High rotor parameters.
    _hwid = apvts.getRawParameterValue("Hi Width")->load() * 0.01f;
    _hwid *= _hwid;
    _hdep = apvts.getRawParameterValue("Hi Depth")->load() * 0.01f;
    _hdep = _hdep * _hdep * float(getSampleRate()) / 760.0f;
    _hlev = apvts.getRawParameterValue("Hi Throb")->load() * 0.01f;
    _hlev = _gain * 0.9f * _hlev * _hlev;
}

void MDALeslieAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
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

    const float filo = _filo;
    const float gain = _gain;
    const float hl = _hlev;
    const float ll = _llev;
    const float hw = _hwid;
    const float lw = _lwid;
    const float hd = _hdep;
    const float hm = _hmom;
    const float lm = _lmom;
    const float ht = _hset * (1.0f - hm);
    const float lt = _lset * (1.0f - lm);

    float fbuf1 = _fbuf1;
    float fbuf2 = _fbuf2;
    float hspd = _hspd;
    float lspd = _lspd;
    float hphi = _hphi;
    float lphi = _lphi;
    int hpos = _hpos;

    // Piecewise linear LFO approximation constants.
    const float k0 = 0.03125f;
    const float k1 = 32.0f;
    long k = 0;

    // LFO values and their per-sample deltas.
    float chp = std::cos(hphi); chp *= chp * chp;  // sin^3 shape for high rotor
    float clp = std::cos(lphi);
    float shp = std::sin(hphi);
    float slp = std::sin(lphi);
    float dchp = 0.0f, dclp = 0.0f, dshp = 0.0f, dslp = 0.0f;

    for (int i = 0; i < buffer.getNumSamples(); ++i) {
        // Sum stereo input to mono.
        float a = in1[i] + in2[i];

        // Update LFO every k1 samples (piecewise linear approximation).
        if (k > 0) {
            k--;
        } else {
            // Tend rotor speeds toward their targets.
            lspd = lm * lspd + lt;
            hspd = hm * hspd + ht;
            lphi += k1 * lspd;
            hphi += k1 * hspd;

            // Compute deltas for the next k1 samples.
            dchp = std::cos(hphi + k1 * hspd);
            dchp = k0 * (dchp * dchp * dchp - chp);
            dclp = k0 * (std::cos(lphi + k1 * lspd) - clp);
            dshp = k0 * (std::sin(hphi + k1 * hspd) - shp);
            dslp = k0 * (std::sin(lphi + k1 * lspd) - slp);

            k = long(k1);
        }

        // Crossover filter: split into low and high frequency bands.
        fbuf1 = filo * (fbuf1 - a) + a;
        fbuf2 = filo * (fbuf2 - fbuf1) + fbuf1;
        float h = (gain - hl * chp) * (a - fbuf2);  // high band with amplitude modulation
        float l = (gain - ll * clp) * fbuf2;         // low band with amplitude modulation

        // Delay line for high-frequency Doppler effect.
        if (hpos > 0) hpos--; else hpos = 200;
        float hint = float(hpos) + hd * (1.0f + chp);
        int hdd = int(hint);
        hint = hint - float(hdd);
        int hdd2 = hdd + 1;
        if (hdd > 199) { if (hdd > 200) hdd -= 201; hdd2 -= 201; }

        _hbuf[hpos] = h;
        float da = _hbuf[hdd];
        h += da + hint * (_hbuf[hdd2] - da);  // interpolated delay output

        // Stereo output: pan rotors left/right using sine/cosine LFOs.
        float c = l + h;
        float d = l + h;
        h *= hw * shp;
        l *= lw * slp;
        d += l - h;
        c += h - l;

        out1[i] = c;
        out2[i] = d;

        // Advance LFO values.
        chp += dchp;
        clp += dclp;
        shp += dshp;
        slp += dslp;
    }

    // Wrap LFO phases to [0, 2*pi].
    _lphi = std::fmod(lphi + (k1 - float(k)) * lspd, _twoPi);
    _hphi = std::fmod(hphi + (k1 - float(k)) * hspd, _twoPi);

    _fbuf1 = fbuf1;
    _fbuf2 = fbuf2;
    _hspd = hspd;
    _lspd = lspd;
    _hpos = hpos;

    // Catch denormals.
    if (std::abs(_fbuf1) < 1.0e-10f) _fbuf1 = 0.0f;
    if (std::abs(_fbuf2) < 1.0e-10f) _fbuf2 = 0.0f;
}

juce::AudioProcessorEditor *MDALeslieAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

void MDALeslieAudioProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    copyXmlToBinary(*apvts.copyState().createXml(), destData);
}

void MDALeslieAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml.get() != nullptr && xml->hasTagName(apvts.state.getType())) {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout MDALeslieAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Speed Mode: stop (0), slow (1), fast (2).
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("Speed Mode", 1),
        "Speed Mode",
        juce::StringArray { "Stop", "Slow", "Fast" },
        2));  // default: Fast

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Lo Width", 1),
        "Lo Width",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f),
        50.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Lo Throb", 1),
        "Lo Throb",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f),
        60.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Hi Width", 1),
        "Hi Width",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f),
        70.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Hi Depth", 1),
        "Hi Depth",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f),
        60.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Hi Throb", 1),
        "Hi Throb",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f),
        70.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Crossover", 1),
        "Crossover",
        juce::NormalisableRange<float>(150.0f, 1500.0f, 0.1f, 0.4f),
        450.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Output", 1),
        "Output",
        juce::NormalisableRange<float>(-20.0f, 20.0f, 0.01f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Speed", 1),
        "Speed",
        juce::NormalisableRange<float>(0.0f, 200.0f, 0.01f),
        100.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    return layout;
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new MDALeslieAudioProcessor();
}
