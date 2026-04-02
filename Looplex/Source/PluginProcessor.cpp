#include "PluginProcessor.h"

MDALooplexAudioProcessor::MDALooplexAudioProcessor()
: AudioProcessor(BusesProperties()
                 .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
                 .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    _inMix = 2.0f;
    _inPan = 0.5f;
    _outMix = 0.000030517578f;
    _feedback = 1.0f;
    _modwhl = 1.0f;

    _bufmax = 882000;  // default ~10 seconds stereo at 44100
    _buffer.resize(_bufmax + 10, 0);
    _bufpos = 0;
    _buflen = 0;
    _status = 0;
    _recreq = 0;
    _d0 = 0.0f;

    _oldParam0 = 0.0f;
    _oldParam1 = 0.0f;
}

MDALooplexAudioProcessor::~MDALooplexAudioProcessor()
{
}

const juce::String MDALooplexAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

int MDALooplexAudioProcessor::getNumPrograms()
{
    return 1;
}

int MDALooplexAudioProcessor::getCurrentProgram()
{
    return 0;
}

void MDALooplexAudioProcessor::setCurrentProgram(int index)
{
}

const juce::String MDALooplexAudioProcessor::getProgramName(int index)
{
    return {};
}

void MDALooplexAudioProcessor::changeProgramName(int index, const juce::String &newName)
{
}

void MDALooplexAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    _sampleRate = (float)sampleRate;
    resetState();
}

void MDALooplexAudioProcessor::releaseResources()
{
}

void MDALooplexAudioProcessor::reset()
{
    resetState();
}

bool MDALooplexAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MDALooplexAudioProcessor::resetState()
{
    _bufpos = 0;
    _buflen = 0;
    _status = 0;
    _recreq = 0;
    _d0 = 0.0f;
    _modwhl = 1.0f;
}

void MDALooplexAudioProcessor::reallocateBuffer()
{
    // Calculate new buffer size based on Max Del parameter.
    // The parameter gives seconds from 10 to 200. Buffer is interleaved stereo.
    float maxDelParam = apvts.getRawParameterValue("Max Del")->load();
    int newBufmax = 2 * (int)_sampleRate * (int)(maxDelParam);
    if (newBufmax < 2) newBufmax = 2;

    _bufmax = newBufmax;
    _buffer.assign(_bufmax + 10, 0);
    _bufpos = 0;
    _buflen = 0;
    _status = 0;
}

void MDALooplexAudioProcessor::update()
{
    // Check if Reset was toggled (acts as a trigger to reallocate the buffer)
    float resetParam = apvts.getRawParameterValue("Reset")->load();
    if (std::fabs(resetParam - _oldParam1) > 0.1f)
    {
        _oldParam1 = resetParam;
        float maxDelParam = apvts.getRawParameterValue("Max Del")->load();
        if (std::fabs(_oldParam0 - maxDelParam) > 0.5f)
        {
            _oldParam0 = maxDelParam;
            reallocateBuffer();
        }
    }

    // Record toggle
    float recordParam = apvts.getRawParameterValue("Record")->load();
    if (recordParam > 0.5f)
        _recreq = 1;
    else
        _recreq = 0;

    // Input mix: param^2 * 2
    float inMixParam = apvts.getRawParameterValue("In Mix")->load();
    _inMix = 2.0f * inMixParam * inMixParam;

    // Input pan: 0=left, 0.5=center, 1=right
    _inPan = apvts.getRawParameterValue("In Pan")->load();

    // Feedback
    _feedback = apvts.getRawParameterValue("Feedback")->load();

    // Output mix: convert from normalized to the original scaling factor
    // Original: 0.000030517578f * param^2 (where 0.000030517578 = 1/32768)
    float outMixParam = apvts.getRawParameterValue("Out Mix")->load();
    _outMix = 0.000030517578f * outMixParam * outMixParam;
}

void MDALooplexAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any output channels that don't contain input data.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i) {
        buffer.clear(i, 0, buffer.getNumSamples());
    }

    update();

    // Process MIDI for mod wheel (controls feedback attenuation) and
    // sustain pedal (toggles recording) / soft pedal (resyncs position).
    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();
        if (msg.isControllerOfType(1)) // mod wheel
        {
            _modwhl = 1.0f - msg.getControllerValue() / 127.0f;
        }
        else if (msg.isControllerOfType(64)) // sustain pedal
        {
            if (msg.getControllerValue() > 63)
            {
                _recreq = _recreq ? 0 : 1; // toggle recording
            }
        }
        else if (msg.isControllerOfType(66) || msg.isControllerOfType(67)) // sostenuto/soft
        {
            if (msg.getControllerValue() > 63)
            {
                _bufpos = 0; // resync
            }
        }
    }

    const float *in1 = buffer.getReadPointer(0);
    const float *in2 = buffer.getReadPointer(1);
    float *out1 = buffer.getWritePointer(0);
    float *out2 = buffer.getWritePointer(1);

    float imix = _inMix;
    float ipan = _inPan;
    float omix = _outMix;
    float fb = _feedback * _modwhl;
    float d0 = _d0;

    int bufpos = _bufpos;
    int buflen = _buflen;
    int bufmax = _bufmax;
    int status = _status;
    int recreq = _recreq;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        float l = in1[i];
        float r = in2[i];

        // Input mix with panning
        r *= imix * ipan;
        l *= imix - ipan * imix;

        if (recreq == 1 && status == 0) status = 1;  // first pass
        if (recreq == 0 && status == 1) status = 2;  // later passes

        if (status)
        {
            // Dither
            float d1 = d0;
            d0 = 0.000061f * (float)((rand() & 32767) - 16384);

            // Left delay - read from buffer
            float dl = fb * (float)_buffer[bufpos];
            if (recreq)
            {
                int x = (int)(32768.0f * l + dl + d0 - d1 + 100000.5f) - 100000;
                if (x > 32767) x = 32767; else if (x < -32768) x = -32768;
                _buffer[bufpos] = (short)x;
            }
            bufpos++;

            // Right delay - read from buffer
            float dr = fb * (float)_buffer[bufpos];
            if (recreq)
            {
                int x = (int)(32768.0f * r + dr - d0 + d1 + 100000.5f) - 100000;
                if (x > 32767) x = 32767; else if (x < -32768) x = -32768;
                _buffer[bufpos] = (short)x;
            }
            bufpos++;

            // Looping
            if (bufpos >= bufmax)
            {
                buflen = bufmax;
                bufpos -= buflen;
                status = 2;
            }
            else
            {
                if (status == 1)
                    buflen = bufpos;
                else if (bufpos >= buflen)
                    bufpos -= buflen;
            }

            // Output: original signal plus delayed signal
            l += omix * dl;
            r += omix * dr;
        }

        out1[i] = l;
        out2[i] = r;
    }

    _bufpos = bufpos;
    _buflen = buflen;
    _status = status;
    _d0 = d0;
}

juce::AudioProcessorEditor *MDALooplexAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

void MDALooplexAudioProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    copyXmlToBinary(*apvts.copyState().createXml(), destData);
}

void MDALooplexAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml.get() != nullptr && xml->hasTagName(apvts.state.getType())) {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout MDALooplexAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Max Del: loop length in seconds (10 to 200)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Max Del", 1),
        "Max Del",
        juce::NormalisableRange<float>(10.0f, 200.0f, 1.0f),
        10.0f,
        juce::AudioParameterFloatAttributes().withLabel("s")));

    // Reset: toggle to reallocate buffer (0 or 1)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Reset", 1),
        "Reset",
        juce::NormalisableRange<float>(0.0f, 1.0f, 1.0f),
        0.0f,
        juce::AudioParameterFloatAttributes()));

    // Record: toggle recording on/off
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Record", 1),
        "Record",
        juce::NormalisableRange<float>(0.0f, 1.0f, 1.0f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("(sus ped)")));

    // In Mix: input level (0 to 1)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("In Mix", 1),
        "In Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        1.0f,
        juce::AudioParameterFloatAttributes()));

    // In Pan: input panning (0=L, 0.5=C, 1=R)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("In Pan", 1),
        "In Pan",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.5f,
        juce::AudioParameterFloatAttributes()));

    // Feedback: loop feedback (0 to 1)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Feedback", 1),
        "Feedback",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        1.0f,
        juce::AudioParameterFloatAttributes().withLabel("(mod whl)")));

    // Out Mix: output level (0 to 1)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Out Mix", 1),
        "Out Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        1.0f,
        juce::AudioParameterFloatAttributes()));

    return layout;
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new MDALooplexAudioProcessor();
}
