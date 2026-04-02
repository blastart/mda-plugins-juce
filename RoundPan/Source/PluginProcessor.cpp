#include "PluginProcessor.h"

MDARoundPanAudioProcessor::MDARoundPanAudioProcessor()
: AudioProcessor(BusesProperties()
                 .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
                 .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

MDARoundPanAudioProcessor::~MDARoundPanAudioProcessor()
{
}

const juce::String MDARoundPanAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

int MDARoundPanAudioProcessor::getNumPrograms()
{
    return 1;
}

int MDARoundPanAudioProcessor::getCurrentProgram()
{
    return 0;
}

void MDARoundPanAudioProcessor::setCurrentProgram(int index)
{
}

const juce::String MDARoundPanAudioProcessor::getProgramName(int index)
{
    return {};
}

void MDARoundPanAudioProcessor::changeProgramName(int index, const juce::String &newName)
{
}

void MDARoundPanAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    resetState();
}

void MDARoundPanAudioProcessor::releaseResources()
{
}

void MDARoundPanAudioProcessor::reset()
{
    resetState();
}

bool MDARoundPanAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MDARoundPanAudioProcessor::resetState()
{
    _phi = 0.0f;
    _dphi = 0.0f;
}

void MDARoundPanAudioProcessor::update()
{
    float panParam  = apvts.getRawParameterValue("Pan")->load();
    float autoParam = apvts.getRawParameterValue("Auto")->load();

    float sr = float(getSampleRate());

    // When Pan changes, set the phase angle directly.
    // panParam 0..1 maps to -pi..+pi (i.e. -180..+180 degrees)
    _phi = 6.2831853f * (panParam - 0.5f);

    // Auto rotation speed
    if (autoParam > 0.55f) {
        _dphi = 20.0f * (autoParam - 0.55f) / sr;
    } else if (autoParam < 0.45f) {
        _dphi = -20.0f * (0.45f - autoParam) / sr;
    } else {
        _dphi = 0.0f;
    }
}

void MDARoundPanAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
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

    const float x = 0.5f;
    const float y = 0.7854f;  // pi/4
    const float fourpi = 12.566371f;  // 4*pi

    float ph = _phi;
    float dph = _dphi;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        float a = x * (in1[i] + in2[i]);

        out1[i] = a * -std::sin(x * ph - y);
        out2[i] = a * std::sin(x * ph + y);

        ph = ph + dph;
    }

    // Wrap phase to [0, 4*pi]
    if (ph < 0.0f)
        ph = ph + fourpi;
    else if (ph > fourpi)
        ph = ph - fourpi;

    _phi = ph;
}

juce::AudioProcessorEditor *MDARoundPanAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

void MDARoundPanAudioProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    copyXmlToBinary(*apvts.copyState().createXml(), destData);
}

void MDARoundPanAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml.get() != nullptr && xml->hasTagName(apvts.state.getType())) {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout MDARoundPanAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Pan", 1),
        "Pan",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        0.5f,
        juce::AudioParameterFloatAttributes()
            .withLabel("deg")
            .withStringFromValueFunction(
                [](float value, int) {
                    return juce::String(int(360.0f * (value - 0.5f)));
                })));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("Auto", 1),
        "Auto",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        0.8f,
        juce::AudioParameterFloatAttributes().withLabel("deg/sec")));

    return layout;
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new MDARoundPanAudioProcessor();
}
