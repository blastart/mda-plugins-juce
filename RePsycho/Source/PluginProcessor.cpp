#include "PluginProcessor.h"

MDARePsychoAudioProcessor::MDARePsychoAudioProcessor()
: AudioProcessor(BusesProperties()
                 .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
                 .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

MDARePsychoAudioProcessor::~MDARePsychoAudioProcessor()
{
}

const juce::String MDARePsychoAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

int MDARePsychoAudioProcessor::getNumPrograms()
{
    return 1;
}

int MDARePsychoAudioProcessor::getCurrentProgram()
{
    return 0;
}

void MDARePsychoAudioProcessor::setCurrentProgram(int index)
{
}

const juce::String MDARePsychoAudioProcessor::getProgramName(int index)
{
    return {};
}

void MDARePsychoAudioProcessor::changeProgramName(int index, const juce::String &newName)
{
}

void MDARePsychoAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    _buffer.resize(_bufferSize);
    _buffer2.resize(_bufferSize);
    resetState();
}

void MDARePsychoAudioProcessor::releaseResources()
{
}

void MDARePsychoAudioProcessor::reset()
{
    resetState();
}

bool MDARePsychoAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MDARePsychoAudioProcessor::resetState()
{
    memset(_buffer.data(), 0, _bufferSize * sizeof(float));
    memset(_buffer2.data(), 0, _bufferSize * sizeof(float));
    _gai = 0.0f;
    _buf = 0.0f;
    _buf2 = 0.0f;
    _tim = _bufferSize + 1;
}

void MDARePsychoAudioProcessor::update()
{
    float tuneParam   = apvts.getRawParameterValue("Tune")->load();
    float fineParam   = apvts.getRawParameterValue("Fine")->load();
    float decayParam  = apvts.getRawParameterValue("Decay")->load();
    float threshParam = apvts.getRawParameterValue("Thresh")->load();
    float holdParam   = apvts.getRawParameterValue("Hold")->load();
    float mixParam    = apvts.getRawParameterValue("Mix")->load();

    _dtim = 441 + int(0.5f * _bufferSize * holdParam);

    _thr = std::pow(10.0f, 1.5f * threshParam - 1.5f);

    if (decayParam > 0.5f) {
        _env = 1.0f + 0.003f * std::pow(decayParam - 0.5f, 5.0f);
    } else {
        _env = 1.0f + 0.025f * std::pow(decayParam - 0.5f, 5.0f);
    }

    // Tune: semitones from tuneParam, cents from fineParam
    float tuneSemi = (int(tuneParam * 24.0f) - 24.0f + (fineParam - 1.0f)) / 24.0f;
    _tun = std::pow(10.0f, 0.60206f * tuneSemi);

    _wet = 0.5f * std::sqrt(mixParam);
    _dry = std::sqrt(1.0f - mixParam);
}

void MDARePsychoAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
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

    float we = _wet, dr = _dry, tu = _tun, en = _env;
    float ga = _gai, x = 0.0f, x2 = 0.0f, xx = _buf, xx2 = _buf2;
    int ti = _tim, dti = _dtim;

    float qualityParam = apvts.getRawParameterValue("Quality")->load();
    bool highQuality = (qualityParam > 0.5f);

    if (highQuality)
    {
        we = we * 2.0f;
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float a = in1[i];
            float b = in2[i];

            if ((a + b > _thr) && (ti > dti))  // trigger
            {
                ga = 1.0f;
                ti = 0;
            }

            if (ti < _bufferSize)  // play out
            {
                if (ti < 80)  // fade in
                {
                    if (ti == 0) { xx = x; xx2 = x2; }

                    _buffer[ti] = a;
                    _buffer2[ti] = b;
                    x = _buffer[int(ti * tu)];
                    x2 = _buffer2[int(ti * tu)];

                    x = xx * (1.0f - 0.0125f * ti) + x * 0.0125f * ti;
                    x2 = xx2 * (1.0f - 0.0125f * ti) + x2 * 0.0125f * ti;
                }
                else
                {
                    _buffer[ti] = a;
                    _buffer2[ti] = b;

                    float it1 = ti * tu;  // interpolation
                    int of1 = int(it1);
                    int of2 = of1 + 1;
                    it1 = it1 - of1;
                    float it2 = 1.0f - it1;

                    // Bounds check
                    if (of2 < _bufferSize) {
                        x = it2 * _buffer[of1] + it1 * _buffer[of2];
                        x2 = it2 * _buffer2[of1] + it1 * _buffer2[of2];
                    } else {
                        x = _buffer[of1];
                        x2 = _buffer2[of1];
                    }
                }

                ti++;
                ga *= en;
            }
            else  // mute
            {
                ga = 0.0f;
            }

            out1[i] = a * dr + x * ga * we;
            out2[i] = b * dr + x2 * ga * we;
        }
    }
    else
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float a = in1[i];
            float b = in2[i];

            if ((a + b > _thr) && (ti > dti))  // trigger
            {
                ga = 1.0f;
                ti = 0;
            }

            if (ti < _bufferSize)  // play out
            {
                if (ti < 80)  // fade in
                {
                    if (ti == 0) xx = x;

                    _buffer[ti] = a + b;
                    x = _buffer[int(ti * tu)];

                    x = xx * (1.0f - 0.0125f * ti) + x * 0.0125f * ti;
                }
                else
                {
                    _buffer[ti] = a + b;
                    x = _buffer[int(ti * tu)];
                }

                ti++;
                ga *= en;
            }
            else  // mute
            {
                ga = 0.0f;
            }

            out1[i] = a * dr + x * ga * we;
            out2[i] = b * dr + x * ga * we;
        }
    }

    _tim = ti;
    _gai = ga;
    _buf = xx;
    _buf2 = xx2;
}

juce::AudioProcessorEditor *MDARePsychoAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

void MDARePsychoAudioProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    copyXmlToBinary(*apvts.copyState().createXml(), destData);
}

void MDARePsychoAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml.get() != nullptr && xml->hasTagName(apvts.state.getType())) {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout MDARePsychoAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Note: The VST2 parameter order was: Tune, Fine, Decay, Thresh, Hold, Mix, Quality
    // (mapped from fParam3, fParam6, fParam2, fParam1, fParam5, fParam4, fParam7)

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Tune", 1),
        "Tune",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        1.0f,
        juce::AudioParameterFloatAttributes().withLabel("semi")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Fine", 1),
        "Fine",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        1.0f,
        juce::AudioParameterFloatAttributes().withLabel("cent")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Decay", 1),
        "Decay",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        0.5f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Thresh", 1),
        "Thresh",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        0.6f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Hold", 1),
        "Hold",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        0.45f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Mix", 1),
        "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        1.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Quality", 1),
        "Quality",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        0.4f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction(
                [](float value, int) {
                    return (value > 0.5f) ? juce::String("HIGH") : juce::String("LOW");
                })));

    return layout;
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new MDARePsychoAudioProcessor();
}
