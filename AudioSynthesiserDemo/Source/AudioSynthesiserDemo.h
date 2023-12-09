/*
  ==============================================================================

   This file is part of the JUCE examples.
   Copyright (c) 2022 - Raw Material Software Limited

   The code included in this file is provided under the terms of the ISC license
   http://www.isc.org/downloads/software-support-policy/isc-license. Permission
   To use, copy, modify, and/or distribute this software for any purpose with or
   without fee is hereby granted provided that the above copyright notice and
   this permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES,
   WHETHER EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR
   PURPOSE, ARE DISCLAIMED.

  ==============================================================================
*/

/*******************************************************************************
 The block below describes the properties of this PIP. A PIP is a short snippet
 of code that can be read by the Projucer and used to generate a JUCE project.

 BEGIN_JUCE_PIP_METADATA

 name:             AudioSynthesiserDemo
 version:          1.0.0
 vendor:           JUCE
 website:          http://juce.com
 description:      Simple synthesiser application.

 dependencies:     juce_audio_basics, juce_audio_devices, juce_audio_formats,
                   juce_audio_processors, juce_audio_utils, juce_core,
                   juce_data_structures, juce_events, juce_graphics,
                   juce_gui_basics, juce_gui_extra
 exporters:        xcode_mac, vs2022, linux_make, androidstudio, xcode_iphone

 moduleFlags:      JUCE_STRICT_REFCOUNTEDPOINTER=1

 type:             Component
 mainClass:        AudioSynthesiserDemo

 useLocalCopy:     1

 END_JUCE_PIP_METADATA

*******************************************************************************/

#pragma once

#include "DemoUtilities.h"
#include "AudioLiveScrollingDisplay.h"


typedef juce::AudioProcessorValueTreeState::SliderAttachment SliderAttachment;
typedef juce::AudioProcessorValueTreeState::ButtonAttachment ButtonAttachment;


//==============================================================================
/** Our demo synth sound is just a basic sine wave.. */
struct SineWaveSound final : public SynthesiserSound
{
    bool appliesToNote (int /*midiNoteNumber*/) override    { return true; }
    bool appliesToChannel (int /*midiChannel*/) override    { return true; }
};

//==============================================================================
/** Our demo synth voice just plays a sine wave.. */
struct SineWaveVoice final : public SynthesiserVoice
{
    SineWaveVoice(LEAF * leaf) : leaf(leaf)
    {
        for (int i = 0; i < numModes; i++)
        {
            tCycle_init(&mySine[i], leaf);
            outputWeights[i] = sin((i+1.0f) * pickupPos);
        }
    }
    
    void setInitialAmplitudes()
    {
        for (int i = 0; i < numModes; i++)
        {
            int n = i + 1;
            double denom = ((n * n) * pluckPos) * (PI - pluckPos);
            amplitudes[i] = 2.0 * sin(pluckPos * n) / denom;
            if ((amplitudes[i] > 1.0f) || (isnan(amplitudes[i])))
            {
                DBG("HELP ME:");
            }
        }
    }
    
    void changePickupPos()
    {
        for (int i = 0; i < numModes; i++)
        {
            outputWeights[i] = sin((i+1.0f) * pickupPos);
        }
    }
    bool canPlaySound (SynthesiserSound* sound) override
    {
        return dynamic_cast<SineWaveSound*> (sound) != nullptr;
    }

    void startNote (int midiNoteNumber, float velocity,
                    SynthesiserSound*, int /*currentPitchWheelPosition*/) override
    {
        if (!playing)
        {
            auto cyclesPerSecond = MidiMessage::getMidiNoteInHertz (midiNoteNumber);
            for (int i = 0; i < numModes; i++)
            {
                int myMode = i + 1;
                float myModeSquared = myMode * myMode;
                float sig = decay + (decayHighFreq * myModeSquared);
                float w0 = myMode * sqrtf(1.0f + (sliderVal * sliderVal) * myModeSquared);
                float w = w0 * sqrtf(1.0f - ((sig * sig) / (w0 * w0)));
                tCycle_setFreq(&mySine[i], cyclesPerSecond * w);
                setInitialAmplitudes();
                
            }
            playing = 1;
            masterAmplitude = 0.7f;
            DBG("newnote");
            
        }

    }

    void stopNote (float /*velocity*/, bool allowTailOff) override
    {
        
        if (playing)
        {
            masterAmplitude = 0.0f;
            playing = 0;
            DBG("stopppped");
        }
    }

    void pitchWheelMoved (int /*newValue*/) override                              {}
    void controllerMoved (int /*controllerNumber*/, int /*newValue*/) override    {}

    void renderNextBlock (AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override
    {
        while (--numSamples >= 0)
        {
            auto currentSample = 0.0f;
            for (int j = 0; j < numModes; j++)
            {
                int myMode = j + 1;
                float myModeSquared = myMode * myMode;
                float sig = decay + (decayHighFreq * myModeSquared);
                currentSample += tCycle_tick(&mySine[j]) * amplitudes[j] * outputWeights[j] * masterAmplitude;
                double preExp = -sig * leaf->invSampleRate * mySine[j]->freq;
                double multiplier = exp(preExp);
                //double multiplier = 1.0f + preExp;
                //multiplier = 0.999f;
                amplitudes[j] = amplitudes[j] * multiplier;
            }

            for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
                outputBuffer.addSample (i, startSample, currentSample);
            ++startSample;
        }
        //DBG(amplitudes[0]);
    }

    using SynthesiserVoice::renderNextBlock;
    float sliderVal = 0.0f;
    float pluckPos = 0.2f;
    float pickupPos = 0.3f;
private:
    float masterAmplitude = 0.0f;

    const static int numModes = 50;
    double amplitudes[numModes] = {0.0f};
    double outputWeights[numModes] = {0.0f};
    const float decay = 0.001f;
    const float decayHighFreq = 0.001f;

    tCycle mySine[numModes];
    int playing = 0;
    LEAF *leaf;

};

class LabeledSlider : public GroupComponent
{
public:
    LabeledSlider (const String& name)
    {
        setText (name);
        setTextLabelPosition (Justification::centredTop);
        addAndMakeVisible (slider);
    }
    
    void resized() override
    {
        slider.setBounds (getLocalBounds().reduced (.5));
    }
    
    Slider slider { Slider::RotaryHorizontalVerticalDrag, Slider::TextBoxBelow };
};
//==============================================================================
// This is an audio source that streams the output of our demo synth.
struct SynthAudioSource final : public AudioSource, public juce::Slider::Listener
{

    SynthAudioSource (MidiKeyboardState& keyState)  : keyboardState (keyState)
    {
        LEAF_init(&leaf, 44100, leafMemory, 32, []() {return (float)rand() / RAND_MAX; });
        // Add some voices to our synth, to play the sounds..
        for (auto i = 0; i < 1; ++i)
        {
            synth.addVoice (new SineWaveVoice(&leaf));   // These voices will play our custom sine-wave sounds..
        }

        // ..and add a sound for them to play...
        setUsingSineWaveSound();
    }

    void setUsingSineWaveSound()
    {
        synth.clearSounds();
        synth.addSound (new SineWaveSound());
    }

    void sliderValueChanged(juce::Slider* slider) override
    {
        for (auto i = 0; i < 1; ++i)
        {
            SineWaveVoice * voice = (SineWaveVoice*)synth.getVoice(i);
            
            if (slider->getComponentID() == "stiffness")
            {
                voice->sliderVal = slider->getValue();
            }
            if (slider->getComponentID() == "pluck pos")
            {
                voice->pluckPos = slider->getValue();
            }
            if (slider->getComponentID() == "pickup pos")
            {
                voice->pickupPos = slider->getValue();
                voice->changePickupPos();
            }
        }
    }

    void prepareToPlay (int /*samplesPerBlockExpected*/, double sampleRate) override
    {
        midiCollector.reset (sampleRate);
        LEAF_setSampleRate(&leaf, sampleRate);
        synth.setCurrentPlaybackSampleRate (sampleRate);
    }

    void releaseResources() override {}

    void getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill) override
    {
        // the synth always adds its output to the audio buffer, so we have to clear it
        // first..
        bufferToFill.clearActiveBufferRegion();

        // fill a midi buffer with incoming messages from the midi input.
        MidiBuffer incomingMidi;
        midiCollector.removeNextBlockOfMessages (incomingMidi, bufferToFill.numSamples);

        // pass these messages to the keyboard state so that it can update the component
        // to show on-screen which keys are being pressed on the physical midi keyboard.
        // This call will also add midi messages to the buffer which were generated by
        // the mouse-clicking on the on-screen keyboard.
        keyboardState.processNextMidiBuffer (incomingMidi, 0, bufferToFill.numSamples, true);

        // and now get the synth to process the midi events and generate its output.
        synth.renderNextBlock (*bufferToFill.buffer, incomingMidi, 0, bufferToFill.numSamples);
    }

    //==============================================================================
    // this collects real-time midi messages from the midi input device, and
    // turns them into blocks that we can process in our audio callback
    MidiMessageCollector midiCollector;

    // this represents the state of which keys on our on-screen keyboard are held
    // down. When the mouse is clicked on the keyboard component, this object also
    // generates midi messages for this, which we can pass on to our synth.
    MidiKeyboardState& keyboardState;

    // the synth itself!
    Synthesiser synth;
    LEAF leaf;
    char leafMemory[32];
};

//==============================================================================
class Callback final : public AudioIODeviceCallback
{
public:
    Callback (AudioSourcePlayer& playerIn, LiveScrollingAudioDisplay& displayIn)
        : player (playerIn), display (displayIn) {}

    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                           int numInputChannels,
                                           float* const* outputChannelData,
                                           int numOutputChannels,
                                           int numSamples,
                                           const AudioIODeviceCallbackContext& context) override
    {
        player.audioDeviceIOCallbackWithContext (inputChannelData,
                                                 numInputChannels,
                                                 outputChannelData,
                                                 numOutputChannels,
                                                 numSamples,
                                                 context);
        display.audioDeviceIOCallbackWithContext (outputChannelData,
                                                  numOutputChannels,
                                                  nullptr,
                                                  0,
                                                  numSamples,
                                                  context);
    }

    void audioDeviceAboutToStart (AudioIODevice* device) override
    {
        player.audioDeviceAboutToStart (device);
        display.audioDeviceAboutToStart (device);
    }

    void audioDeviceStopped() override
    {
        player.audioDeviceStopped();
        display.audioDeviceStopped();
    }

private:
    AudioSourcePlayer& player;
    LiveScrollingAudioDisplay& display;
};

//==============================================================================
class AudioSynthesiserDemo final : public Component
{
public:
    AudioSynthesiserDemo()
    {
        addAndMakeVisible (keyboardComponent);

        addAndMakeVisible (sineButton);
        sineButton.setRadioGroupId (321);
        sineButton.setToggleState (true, dontSendNotification);
        sineButton.onClick = [this] { synthAudioSource.setUsingSineWaveSound(); };

        addAndMakeVisible (liveAudioDisplayComp);
        
        addAndMakeVisible (stiffness);
        stiffness.setRange (0.0f, 2.0f);
        stiffness.addListener(&synthAudioSource);
        stiffness.setComponentID("stiffness");
        
        addAndMakeVisible (pluckPos);
        pluckPos.setRange (0.01f, PI-0.01f);
        pluckPos.addListener(&synthAudioSource);
        pluckPos.setComponentID("pluck pos");
        
        addAndMakeVisible (pickupPos);
        pickupPos.setRange (0.01f, PI-0.01f);
        pickupPos.addListener(&synthAudioSource);
        pickupPos.setComponentID("pickup pos");
        //openAttachment.reset (new SliderAttachment (valueTreeState, "open_amount", openAmountSlider.slider));
        
        
        
        audioSourcePlayer.setSource (&synthAudioSource);

       #ifndef JUCE_DEMO_RUNNER
        audioDeviceManager.initialise (0, 2, nullptr, true, {}, nullptr);
       #endif

        audioDeviceManager.addAudioCallback (&callback);
        audioDeviceManager.addMidiInputDeviceCallback ({}, &(synthAudioSource.midiCollector));

        setOpaque (true);
        setSize (640, 480);
    }

    ~AudioSynthesiserDemo() override
    {
        audioSourcePlayer.setSource (nullptr);
        audioDeviceManager.removeMidiInputDeviceCallback ({}, &(synthAudioSource.midiCollector));
        audioDeviceManager.removeAudioCallback (&callback);
    }

    //==============================================================================
    void paint (Graphics& g) override
    {
        g.fillAll (getUIColourIfAvailable (LookAndFeel_V4::ColourScheme::UIColour::windowBackground));
    }

    void resized() override
    {
        keyboardComponent   .setBounds (8, 96, getWidth() - 16, 64);
        stiffness    .setBounds (8, 256, 128, 128);
        pluckPos    .setBounds (158, 256, 128, 128);
        pickupPos    .setBounds (308, 256, 128, 128);
        sineButton          .setBounds (16, 176, 150, 24);
        sampledButton       .setBounds (16, 200, 150, 24);
        liveAudioDisplayComp.setBounds (8, 8, getWidth() - 16, 64);
    }

private:
    // if this PIP is running inside the demo runner, we'll use the shared device manager instead
   #ifndef JUCE_DEMO_RUNNER
    AudioDeviceManager audioDeviceManager;
   #else
    AudioDeviceManager& audioDeviceManager { getSharedAudioDeviceManager (0, 2) };
   #endif

    MidiKeyboardState keyboardState;
    AudioSourcePlayer audioSourcePlayer;
    SynthAudioSource synthAudioSource        { keyboardState };
    MidiKeyboardComponent keyboardComponent  { keyboardState, MidiKeyboardComponent::horizontalKeyboard};

    ToggleButton sineButton     { "Use sine wave" };
    ToggleButton sampledButton  { "Use sampled sound" };
    
    Slider stiffness {"stiffness"};
    Slider pluckPos {"pluck pos"};
    Slider pickupPos {"pickup pos"};
    
    LiveScrollingAudioDisplay liveAudioDisplayComp;

    Callback callback { audioSourcePlayer, liveAudioDisplayComp };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioSynthesiserDemo)
};



