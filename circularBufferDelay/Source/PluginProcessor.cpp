/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
CircularBufferDelayAudioProcessor::CircularBufferDelayAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
}

CircularBufferDelayAudioProcessor::~CircularBufferDelayAudioProcessor()
{
}

//==============================================================================
const juce::String CircularBufferDelayAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool CircularBufferDelayAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool CircularBufferDelayAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool CircularBufferDelayAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double CircularBufferDelayAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int CircularBufferDelayAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int CircularBufferDelayAudioProcessor::getCurrentProgram()
{
    return 0;
}

void CircularBufferDelayAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String CircularBufferDelayAudioProcessor::getProgramName (int index)
{
    return {};
}

void CircularBufferDelayAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
// STEP 3
// We want to make sure our circular buffer is larger than our main buffer
// main buffer is around 512 or 1024
// we determine that here in prepareToPlay method

// prepareToPlay gets called right before our audio starts to play,
// or anytime our soundcard changes sample rates
// or if stop playing audio and then get ready to play audio again
void CircularBufferDelayAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // here is where we actually set the size of our delay buffer
    // 44,100 * 2 = 88,200 which will be our circular buffer size
    auto delayBufferSize = sampleRate * 2.0;
    
    // call method we created in header file delayBuffer.setSize
    // getTotalNumOutputChannels (probably 2, a stereo signal)
    // set number of samples we want our buffer to have for its size (use delayBufferSize)
    // cast delayBufferSize to int with (int) before delayBufferSize
    delayBuffer.setSize(getTotalNumOutputChannels(), (int)delayBufferSize);
}

void CircularBufferDelayAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool CircularBufferDelayAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

// STEP 4
// processBlock is where our main audio is actually being processed
// where the magic happens
void CircularBufferDelayAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // step 6
    auto bufferSize = buffer.getNumSamples();
    auto delayBufferSize = delayBuffer.getNumSamples();
    
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer (channel);

        // Check to see if main buffer copies to delay buffer without need to wrap...
        if (delayBufferSize > bufferSize + writePosition) {
            // if yes
                // copy main buffer contents to delay buffer
            // (this is just if we're able to copy the buffer size amount of samples without going over the end of our circular buffer)
            delayBuffer.copyFromWithRamp(channel, writePosition, channelData, bufferSize, 0.1f, 0.1f);
        }
        // if no
        else {
            // Determine how much space is left at the end of the delay buffer
            auto numSamplesToEnd = delayBufferSize - writePosition;
            
            // Copy that amount of contents to the end...
            delayBuffer.copyFromWithRamp(channel, writePosition, channelData, numSamplesToEnd, 0.1f, 0.1f);
            
            // Calculate how much contents is remaining to copy
            // calculate how many samples we need at the beginning
            auto numSamplesAtStart = bufferSize - numSamplesToEnd;
            
            // Copy remaining amount to beginning of delay buffer
            delayBuffer.copyFromWithRamp(channel, 0, channelData - numSamplesToEnd, numSamplesAtStart, 0.1f, 0.1f);
        }
    }
    
    
    // step 5
    // take write position and add buffer.getNumSamples amounts to write position
    // everytime we are copying, we want write position to keep track of where we're copying to
    // on the next time we are doing our callback
    
    writePosition += bufferSize;
    
    //STEP 7
    // we always want to make sure our write position stays in the bounds of our delay buffer
    // so when our write position gets to the edge of our delay buffer, it wraps around at the beginning
    // because the write positon tells us WHERE we need to do the copying
    // ensure that write position stays between 0 and delayBufferSize
    writePosition %= delayBufferSize;
}

//==============================================================================
bool CircularBufferDelayAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* CircularBufferDelayAudioProcessor::createEditor()
{
    return new CircularBufferDelayAudioProcessorEditor (*this);
}

//==============================================================================
void CircularBufferDelayAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void CircularBufferDelayAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CircularBufferDelayAudioProcessor();
}
