#include "PluginProcessor.h"

MDAVocInputAudioProcessor::MDAVocInputAudioProcessor()
: AudioProcessor(BusesProperties()
                 .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
                 .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

MDAVocInputAudioProcessor::~MDAVocInputAudioProcessor()
{
}

const juce::String MDAVocInputAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

int MDAVocInputAudioProcessor::getNumPrograms()
{
    return 1;
}

int MDAVocInputAudioProcessor::getCurrentProgram()
{
    return 0;
}

void MDAVocInputAudioProcessor::setCurrentProgram(int index)
{
}

const juce::String MDAVocInputAudioProcessor::getProgramName(int index)
{
    return {};
}

void MDAVocInputAudioProcessor::changeProgramName(int index, const juce::String &newName)
{
}

void MDAVocInputAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    resetState();
    update();
}

void MDAVocInputAudioProcessor::releaseResources()
{
}

void MDAVocInputAudioProcessor::reset()
{
    resetState();
}

bool MDAVocInputAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MDAVocInputAudioProcessor::resetState()
{
    _lbuf0 = 0.0f;
    _lbuf1 = 0.0f;
    _lbuf2 = 0.0f;
    _lbuf3 = 0.0f;
    _pstep = 0.0f;
    _sawbuf = 0.0f;
    _lenv = 0.0f;
    _henv = 0.0f;
}

void MDAVocInputAudioProcessor::update()
{
    float fs = (float)getSampleRate();
    float ifs = 1.0f / fs;

    // Tracking: 0..1 mapped to 0/1/2 (OFF/FREE/QUANT).
    // Original: track = (int)(2.99 * param[0])
    float trackParam = apvts.getRawParameterValue("Tracking")->load();
    _track = (int)(2.99f * trackParam);

    // Pitch: original param[1] 0..1, pmult = pow(1.0594631, floor(48*p-24))
    // We store as semitones -24..+24.
    float pitchParam = apvts.getRawParameterValue("Pitch")->load();
    // Map back to original 0..1: p1 = (pitchParam + 24) / 48
    float p1 = (pitchParam + 24.0f) / 48.0f;
    _pmult = (float)std::pow(1.0594631f, std::floor(48.0f * p1 - 24.0f));
    if (_track == 0) _pstep = 110.0f * _pmult * ifs;

    // Breath: original param[2] 0..1, noise = 6 * p2. Displayed as (100*p) %.
    float breathParam = apvts.getRawParameterValue("Breath")->load();
    float p2 = breathParam / 100.0f;
    _noise = 6.0f * p2;

    _lfreq = 660.0f * ifs;

    // Max Freq: original param[4] 0..1. minp = pow(16, 0.5-p4) * fs/440
    float maxFreqParam = apvts.getRawParameterValue("Max Freq")->load();
    float p4 = maxFreqParam / 100.0f;
    _minp = (float)std::pow(16.0f, 0.5f - p4) * fs / 440.0f;
    _maxp = 0.03f * fs;

    _root = std::log10(8.1757989f * ifs);

    // S Thresh: original param[3] 0..1, vuv = p3*p3. Displayed as (100*p) %.
    float threshParam = apvts.getRawParameterValue("S Thresh")->load();
    float p3 = threshParam / 100.0f;
    _vuv = p3 * p3;
}

void MDAVocInputAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
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

    float a, b;
    float ds = _pstep, s = _sawbuf, n = _noise;
    float l0 = _lbuf0, l1 = _lbuf1, l2 = _lbuf2, l3 = _lbuf3;
    float le = _lenv, he = _henv, et = _lfreq * 0.1f, lf = _lfreq, v = _vuv, mn = _minp, mx = _maxp;
    float rootm = 39.863137f;
    int tr = _track;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        a = in1[i];
        b = in2[i];

        l0 -= lf * (l1 + a);        // fundamental filter (peaking 2nd-order 100Hz lpf)
        l1 -= lf * (l1 - l0);

        b = l0; if (b < 0.0f) b = -b;
        le -= et * (le - b);         // fundamental level

        b = (a + 0.03f) * v;
        if (b < 0.0f) b = -b;
        he -= et * (he - b);         // overall level (+ constant so >f0 when quiet)

        l3 += 1.0f;
        if (tr > 0)                  // pitch tracking
        {
            if (l1 > 0.0f && l2 <= 0.0f) // found +ve zero crossing
            {
                if (l3 > mn && l3 < mx)   // ...in allowed range
                {
                    mn = 0.6f * l3;          // new max pitch to discourage octave jumps
                    l2 = l1 / (l1 - l2);    // fractional period
                    ds = _pmult / (l3 - l2); // new period

                    if (tr == 2)             // quantize pitch
                    {
                        ds = rootm * (float)(std::log10(ds) - _root);
                        ds = (float)std::pow(1.0594631, std::floor(ds + 0.5) + rootm * _root);
                    }
                }
                l3 = l2;                    // restart period measurement
            }
            l2 = l1;                        // remember previous sample
        }

        b = 0.00001f * (float)((std::rand() & 32767) - 16384); // sibilance
        if (le > he) b *= s * n;                      // ...or modulated breath noise
        b += s; s += ds; if (s > 0.5f) s -= 1.0f;    // badly aliased sawtooth

        out1[i] = a;
        out2[i] = b;
    }

    _sawbuf = s;

    if (std::fabs(he) > 1.0e-10) _henv = he; else _henv = 0.0f;
    if (std::fabs(l1) > 1.0e-10) { _lbuf0 = l0; _lbuf1 = l1; _lenv = le; }
    else { _lbuf0 = 0.0f; _lbuf1 = 0.0f; _lenv = 0.0f; }
    _lbuf2 = l2;
    _lbuf3 = l3;
    if (tr) _pstep = ds;
}

juce::AudioProcessorEditor *MDAVocInputAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

void MDAVocInputAudioProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    copyXmlToBinary(*apvts.copyState().createXml(), destData);
}

void MDAVocInputAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml.get() != nullptr && xml->hasTagName(apvts.state.getType())) {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout MDAVocInputAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Tracking: 0..1, mapped to OFF/FREE/QUANT. Original default 0.25.
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Tracking", 1),
        "Tracking",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        0.25f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction(
                [](float value, int) {
                    int t = (int)(2.99f * value);
                    if (t == 0) return juce::String("OFF");
                    if (t == 1) return juce::String("FREE");
                    return juce::String("QUANT");
                }
            )));

    // Pitch: original param[1] 0..1, displayed as semitones floor(48*p-24).
    // Range: -24..+24 semitones. Default 0.50 => 0 semitones.
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Pitch", 1),
        "Pitch",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 1.0f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("semi")));

    // Breath: original param[2] 0..1, displayed as (100*p) %. Default 0.20 => 20%.
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Breath", 1),
        "Breath",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        20.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    // S Thresh: original param[3] 0..1, displayed as (100*p) %. Default 0.50 => 50%.
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("S Thresh", 1),
        "S Thresh",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        50.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    // Max Freq: original param[4] 0..1, displayed as MIDI note name. Default 0.35 => 35%.
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Max Freq", 1),
        "Max Freq",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        35.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    return layout;
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new MDAVocInputAudioProcessor();
}
