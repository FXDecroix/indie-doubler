#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
PluginProcessor::PluginProcessor()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
       apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    // The "Intensity" macro: a single dial (not indie → very indie) that the editor
    // maps onto the individual parameters below. Kept as a real parameter so its
    // position is saved and shown, but it does not drive the DSP directly.
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "intensity", 1 }, "Intensity",
        NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 0.4f));

    layout.add (std::make_unique<AudioParameterInt> (
        ParameterID { "voices", 1 }, "Voices", 1, indie::DoublerEngine::kMaxVoices, 2));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "timingDrift", 1 }, "Timing Drift",
        NormalisableRange<float> { 0.0f, 40.0f, 0.1f }, 12.0f,
        AudioParameterFloatAttributes().withLabel ("ms")));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "variance", 1 }, "Variance",
        NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 0.5f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "detune", 1 }, "Detune",
        NormalisableRange<float> { 0.0f, 25.0f, 0.1f }, 8.0f,
        AudioParameterFloatAttributes().withLabel ("cents")));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "width", 1 }, "Width",
        NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 0.8f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "mix", 1 }, "Mix",
        NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 0.5f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "warmth", 1 }, "Warmth",
        NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 0.35f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "decorrelate", 1 }, "Decorrelate",
        NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 0.5f));

    return layout;
}

void PluginProcessor::applyIntensity (float t)
{
    t = juce::jlimit (0.0f, 1.0f, t);

    auto set = [this] (const juce::String& id, float actualValue)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (actualValue));
    };

    // The "optimal" curve from subtle/tight/clean (not indie) to loose/wide/characterful
    // (very indie). More than just mix — every meaningful dimension opens up together.
    set ("voices",      juce::jmap (t, 1.0f,  3.49f)); // 1 → 3 doubles
    set ("timingDrift", juce::jmap (t, 5.0f,  22.0f)); // tight → loose (ms)
    set ("variance",    juce::jmap (t, 0.15f, 0.9f));  // steady → human
    set ("detune",      juce::jmap (t, 3.0f,  16.0f)); // gentle → wide pitch spread (cents)
    set ("width",       juce::jmap (t, 0.35f, 1.0f));  // narrow → wide
    set ("mix",         juce::jmap (t, 0.22f, 0.55f)); // under → present
    set ("warmth",      juce::jmap (t, 0.25f, 0.5f));  // a touch darker as it gets denser
    set ("decorrelate", juce::jmap (t, 0.35f, 0.85f)); // more glue/decorrelation
}

PluginProcessor::~PluginProcessor()
{
}

//==============================================================================
const juce::String PluginProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PluginProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double PluginProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PluginProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int PluginProcessor::getCurrentProgram()
{
    return 0;
}

void PluginProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String PluginProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void PluginProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels      = (juce::uint32) juce::jmax (1, getTotalNumOutputChannels());

    engine.prepare (spec);

    voicesParam      = apvts.getRawParameterValue ("voices");
    timingDriftParam = apvts.getRawParameterValue ("timingDrift");
    varianceParam    = apvts.getRawParameterValue ("variance");
    detuneParam      = apvts.getRawParameterValue ("detune");
    widthParam       = apvts.getRawParameterValue ("width");
    mixParam         = apvts.getRawParameterValue ("mix");
    warmthParam      = apvts.getRawParameterValue ("warmth");
    decorrelateParam = apvts.getRawParameterValue ("decorrelate");
}

void PluginProcessor::releaseResources()
{
    engine.reset();
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    const auto mainOut = layouts.getMainOutputChannelSet();

    // We support mono or stereo output.
    if (mainOut != juce::AudioChannelSet::mono()
     && mainOut != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    const auto mainIn = layouts.getMainInputChannelSet();

    // The input must either match the output (mono->mono, stereo->stereo),
    // or be a mono source widened to a stereo output - the doubler spreads
    // its generated doubles across the stereo field, so a mono track gets
    // an instant sense of space with no extra routing.
    const bool monoToStereo = mainIn == juce::AudioChannelSet::mono()
                            && mainOut == juce::AudioChannelSet::stereo();

    if (mainIn != mainOut && ! monoToStereo)
        return false;
   #endif

    return true;
  #endif
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Mono source widened to a stereo output: duplicate the input into the
    // second channel so the engine sees a "stereo" source (identical in both
    // channels) and pans the generated doubles across the full stereo field.
    if (totalNumInputChannels == 1 && totalNumOutputChannels == 2)
    {
        buffer.copyFrom (1, 0, buffer, 0, 0, buffer.getNumSamples());
    }
    else
    {
        // In case we have more outputs than inputs, this code clears any output
        // channels that didn't contain input data, (because these aren't
        // guaranteed to be empty - they may contain garbage).
        // This is here to avoid people getting screaming feedback
        // when they first compile a plugin, but obviously you don't need to keep
        // this code if your algorithm always overwrites all the output channels.
        for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
            buffer.clear (i, 0, buffer.getNumSamples());
    }

    indie::DoublerEngine::Parameters params;
    params.numVoices    = (int) std::round (voicesParam->load());
    params.timingDriftMs = timingDriftParam->load();
    params.variance     = varianceParam->load();
    params.detuneCents  = detuneParam->load();
    params.width        = widthParam->load();
    params.mix          = mixParam->load();
    params.warmth       = warmthParam->load();
    params.decorrelate  = decorrelateParam->load();

    engine.setParameters (params);
    engine.process (buffer);
}

//==============================================================================
bool PluginProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

//==============================================================================
void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
