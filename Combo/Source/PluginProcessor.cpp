#include "PluginProcessor.h"

MDAComboAudioProcessor::MDAComboAudioProcessor()
: AudioProcessor(BusesProperties()
                 .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
                 .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

MDAComboAudioProcessor::~MDAComboAudioProcessor()
{
}

const juce::String MDAComboAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

int MDAComboAudioProcessor::getNumPrograms()
{
    return 1;
}

int MDAComboAudioProcessor::getCurrentProgram()
{
    return 0;
}

void MDAComboAudioProcessor::setCurrentProgram(int index)
{
}

const juce::String MDAComboAudioProcessor::getProgramName(int index)
{
    return {};
}

void MDAComboAudioProcessor::changeProgramName(int index, const juce::String &newName)
{
}

void MDAComboAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    _buffer.resize(_bufferSize);
    _buffe2.resize(_bufferSize);
    resetState();
}

void MDAComboAudioProcessor::releaseResources()
{
}

void MDAComboAudioProcessor::reset()
{
    resetState();
}

bool MDAComboAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MDAComboAudioProcessor::resetState()
{
    _bufpos = 0;

    memset(_buffer.data(), 0, _bufferSize * sizeof(float));
    memset(_buffe2.data(), 0, _bufferSize * sizeof(float));

    _ff1 = 0.0f; _ff2 = 0.0f; _ff3 = 0.0f; _ff4 = 0.0f; _ff5 = 0.0f;
    _ff6 = 0.0f; _ff7 = 0.0f; _ff8 = 0.0f; _ff9 = 0.0f; _ff10 = 0.0f;
    _hh0 = 0.0f; _hh1 = 0.0f;
}

float MDAComboAudioProcessor::filterFreq(float hz)
{
    float r = 0.999f;
    float j = r * r - 1.0f;
    float k = 2.0f - 2.0f * r * r * std::cos(0.647f * hz / float(getSampleRate()));
    return (std::sqrt(k * k - 4.0f * j * j) - k) / (2.0f * j);
}

void MDAComboAudioProcessor::update()
{
    float fParam1 = apvts.getRawParameterValue("Model")->load();
    float fParam2 = apvts.getRawParameterValue("Drive")->load();
    float fParam3 = apvts.getRawParameterValue("Bias")->load();
    float fParam4 = apvts.getRawParameterValue("Output")->load();
    float fParam5 = apvts.getRawParameterValue("Process")->load();
    float fParam6 = apvts.getRawParameterValue("HPF Freq")->load();
    float fParam7 = apvts.getRawParameterValue("HPF Reso")->load();

    _ster = 0;
    if (fParam5 > 0.5f) _ster = 1;

    _hpf = filterFreq(25.0f);

    switch (int(fParam1 * 6.9f))
    {
        case 0: // DI
            _trim = 0.5f;
            _lpf = 0.0f;
            _mix1 = 0.0f;
            _mix2 = 0.0f;
            _del1 = 0;
            _del2 = 0;
            break;

        case 1: // Speaker sim
            _trim = 0.53f;
            _lpf = filterFreq(2700.0f);
            _mix1 = 0.0f;
            _mix2 = 0.0f;
            _del1 = 0;
            _del2 = 0;
            _hpf = filterFreq(382.0f);
            break;

        case 2: // Radio
            _trim = 1.10f;
            _lpf = filterFreq(1685.0f);
            _mix1 = -1.70f;
            _mix2 = 0.82f;
            _del1 = int(float(getSampleRate()) / 6546.0f);
            _del2 = int(float(getSampleRate()) / 4315.0f);
            break;

        case 3: // Mesa Boogie 1"
            _trim = 0.98f;
            _lpf = filterFreq(1385.0f);
            _mix1 = -0.53f;
            _mix2 = 0.21f;
            _del1 = int(float(getSampleRate()) / 7345.0f);
            _del2 = int(float(getSampleRate()) / 1193.0f);
            break;

        case 4: // Mesa Boogie 8"
            _trim = 0.96f;
            _lpf = filterFreq(1685.0f);
            _mix1 = -0.85f;
            _mix2 = 0.41f;
            _del1 = int(float(getSampleRate()) / 6546.0f);
            _del2 = int(float(getSampleRate()) / 3315.0f);
            break;

        case 5: // Marshall 4x12" celestion
            _trim = 0.59f;
            _lpf = filterFreq(2795.0f);
            _mix1 = -0.29f;
            _mix2 = 0.38f;
            _del1 = int(float(getSampleRate()) / 982.0f);
            _del2 = int(float(getSampleRate()) / 2402.0f);
            _hpf = filterFreq(459.0f);
            break;

        case 6: // Scooped-out metal
            _trim = 0.30f;
            _lpf = filterFreq(1744.0f);
            _mix1 = -0.96f;
            _mix2 = 1.6f;
            _del1 = int(float(getSampleRate()) / 356.0f);
            _del2 = int(float(getSampleRate()) / 1263.0f);
            _hpf = filterFreq(382.0f);
            break;
    }

    _mode = (fParam2 < 0.5f) ? 1 : 0;
    if (_mode) // soft clipping
    {
        _drive = std::pow(10.0f, 2.0f - 6.0f * fParam2);
        _trim *= 0.55f + 150.0f * std::pow(fParam2, 4.0f);
    }
    else // hard clipping
    {
        _drive = 1.0f;
        _clip = 11.7f - 16.0f * fParam2;
        if (fParam2 > 0.7f)
        {
            _drive = std::pow(10.0f, 7.0f * fParam2 - 4.9f);
            _clip = 0.5f;
        }
    }

    _bias = 1.2f * fParam3 - 0.6f;
    if (fParam2 > 0.5f)
        _bias /= (1.0f + 3.0f * (fParam2 - 0.5f));
    else
        _bias /= (1.0f + 3.0f * (0.5f - fParam2));

    _trim *= std::pow(10.0f, 2.0f * fParam4 - 1.0f);
    if (_ster) _trim *= 2.0f;

    _hhf = fParam6;
    _hhq = 1.1f - fParam7;
    if (fParam6 > 0.05f) _drive = _drive * (1.0f + 0.1f * _drive);
}

void MDAComboAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
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

    float m1 = _mix1, m2 = _mix2, clp = _clip;
    float o = _lpf, i_coeff = 1.0f - _lpf, o2 = _hpf, bi = _bias, drv = _drive;
    float f1 = _ff1, f2 = _ff2, f3 = _ff3, f4 = _ff4, f5 = _ff5;
    float f6 = _ff6, f7 = _ff7, f8 = _ff8, f9 = _ff9, f10 = _ff10;
    float hf = _hhf, hq = _hhq, h0 = _hh0, h1 = _hh1;
    int d1 = _del1, d2 = _del2, bp = _bufpos;

    float trm = _trim * i_coeff * i_coeff * i_coeff * i_coeff;

    if (_ster) // stereo
    {
        for (int n = 0; n < buffer.getNumSamples(); ++n)
        {
            float a = drv * (in1[n] + bi);
            float a2 = drv * (in2[n] + bi);
            float b, b2;

            if (_mode)
            {
                b = (a > 0.0f) ? a : -a;
                b2 = (a2 > 0.0f) ? a2 : -a2;
                b = a / (1.0f + b);
                b2 = a2 / (1.0f + b2);
            }
            else
            {
                b = (a > clp) ? clp : a;
                b2 = (a2 > clp) ? clp : a2;
                b = (a < -clp) ? -clp : b;
                b2 = (a2 < -clp) ? -clp : b2;
            }

            _buffer[bp] = b;
            _buffe2[bp] = b2;
            b += (m1 * _buffer[(bp + d1) % 1000]) + (m2 * _buffer[(bp + d2) % 1000]);
            b2 += (m1 * _buffe2[(bp + d1) % 1000]) + (m2 * _buffe2[(bp + d2) % 1000]);

            f1 = o * f1 + trm * b;   f6 = o * f6 + trm * b2;
            f2 = o * f2 + f1;        f7 = o * f7 + f6;
            f3 = o * f3 + f2;        f8 = o * f8 + f7;
            f4 = o * f4 + f3;        f9 = o * f9 + f8; // -24dB/oct filter

            f5 = o2 * (f5 - f4) + f4;   f10 = o2 * (f10 - f9) + f9; // high pass
            b = f4 - f5;                 b2 = f9 - f10;

            bp = (bp == 0) ? 999 : bp - 1;

            out1[n] = b;
            out2[n] = b2;
        }
    }
    else // mono
    {
        if (_mode) // soft clip
        {
            for (int n = 0; n < buffer.getNumSamples(); ++n)
            {
                float a = drv * (in1[n] + in2[n] + bi);

                h0 += hf * (h1 + a); // resonant highpass (Chamberlin SVF)
                h1 -= hf * (h0 + hq * h1);
                a += h1;

                float b = (a > 0.0f) ? a : -a;
                b = a / (1.0f + b);

                _buffer[bp] = b;
                b += (m1 * _buffer[(bp + d1) % 1000]) + (m2 * _buffer[(bp + d2) % 1000]);

                f1 = o * f1 + trm * b;
                f2 = o * f2 + f1;
                f3 = o * f3 + f2;
                f4 = o * f4 + f3; // -24dB/oct filter

                f5 = o2 * (f5 - f4) + f4; // high pass
                b = f4 - f5;

                bp = (bp == 0) ? 999 : bp - 1;

                out1[n] = b;
                out2[n] = b;
            }
        }
        else // hard clip
        {
            for (int n = 0; n < buffer.getNumSamples(); ++n)
            {
                float a = drv * (in1[n] + in2[n] + bi);

                h0 += hf * (h1 + a); // resonant highpass (Chamberlin SVF)
                h1 -= hf * (h0 + hq * h1);
                a += h1;

                float b = (a > clp) ? clp : a; // distort
                b = (a < -clp) ? -clp : b;

                _buffer[bp] = b;
                b += (m1 * _buffer[(bp + d1) % 1000]) + (m2 * _buffer[(bp + d2) % 1000]);

                f1 = o * f1 + trm * b;
                f2 = o * f2 + f1;
                f3 = o * f3 + f2;
                f4 = o * f4 + f3; // -24dB/oct filter

                f5 = o2 * (f5 - f4) + f4; // high pass
                b = f4 - f5;

                bp = (bp == 0) ? 999 : bp - 1;

                out1[n] = b;
                out2[n] = b;
            }
        }
    }

    _bufpos = bp;

    // Trap denormals
    if (std::abs(f1) < 1.0e-10f) { _ff1 = 0.0f; _ff2 = 0.0f; _ff3 = 0.0f; _ff4 = 0.0f; _ff5 = 0.0f; }
    else { _ff1 = f1; _ff2 = f2; _ff3 = f3; _ff4 = f4; _ff5 = f5; }

    if (std::abs(f6) < 1.0e-10f) { _ff6 = 0.0f; _ff7 = 0.0f; _ff8 = 0.0f; _ff9 = 0.0f; _ff10 = 0.0f; }
    else { _ff6 = f6; _ff7 = f7; _ff8 = f8; _ff9 = f9; _ff10 = f10; }

    if (std::abs(h0) < 1.0e-10f) { _hh0 = 0.0f; _hh1 = 0.0f; }
    else { _hh0 = h0; _hh1 = h1; }
}

juce::AudioProcessorEditor *MDAComboAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

void MDAComboAudioProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    copyXmlToBinary(*apvts.copyState().createXml(), destData);
}

void MDAComboAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml.get() != nullptr && xml->hasTagName(apvts.state.getType())) {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout MDAComboAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Model: 0-1 maps to 7 models via int(value * 6.9)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Model", 1),
        "Model",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        1.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction(
                [](float value, int) {
                    switch (int(value * 6.9f)) {
                        case 0: return juce::String("D.I.");
                        case 1: return juce::String("Spkr Sim");
                        case 2: return juce::String("Radio");
                        case 3: return juce::String("MB 1\"");
                        case 4: return juce::String("MB 8\"");
                        case 5: return juce::String("4x12 ^");
                        case 6: return juce::String("4x12 >");
                        default: return juce::String("D.I.");
                    }
                }
            )));

    // Drive: 0-1, display as -100 to +100, label "S <> H"
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Drive", 1),
        "Drive",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        0.50f,
        juce::AudioParameterFloatAttributes()
            .withLabel("S <> H")
            .withStringFromValueFunction(
                [](float value, int) { return juce::String(int(200.0f * value - 100.0f)); }
            )));

    // Bias: 0-1, display as -100 to +100
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Bias", 1),
        "Bias",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        0.50f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction(
                [](float value, int) { return juce::String(int(200.0f * value - 100.0f)); }
            )));

    // Output: 0-1, display as -20 to +20 dB
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Output", 1),
        "Output",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        0.50f,
        juce::AudioParameterFloatAttributes()
            .withLabel("dB")
            .withStringFromValueFunction(
                [](float value, int) { return juce::String(int(40.0f * value - 20.0f)); }
            )));

    // Process: 0-1, stereo if > 0.5
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Process", 1),
        "Process",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        0.40f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction(
                [](float value, int) { return (value > 0.5f) ? juce::String("STEREO") : juce::String("MONO"); }
            )));

    // HPF Freq: 0-1, display as 0-100%
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("HPF Freq", 1),
        "HPF Freq",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        0.00f,
        juce::AudioParameterFloatAttributes()
            .withLabel("%")
            .withStringFromValueFunction(
                [](float value, int) { return juce::String(int(100.0f * value)); }
            )));

    // HPF Reso: 0-1, display as 0-100%
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("HPF Reso", 1),
        "HPF Reso",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        0.50f,
        juce::AudioParameterFloatAttributes()
            .withLabel("%")
            .withStringFromValueFunction(
                [](float value, int) { return juce::String(int(100.0f * value)); }
            )));

    return layout;
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new MDAComboAudioProcessor();
}
