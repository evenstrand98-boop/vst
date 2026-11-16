#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include "Voices.h"

class StepboxProcessor : public juce::AudioProcessor
{
public:
    StepboxProcessor();
    ~StepboxProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Stepbox"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- pattern + randomness (shared between audio thread and UI) ----
    juce::AudioProcessorValueTreeState apvts;
    std::array<std::array<std::atomic<uint8_t>, stepbox::numSteps>, stepbox::numLanes> pat;
    std::atomic<int> currentStep { -1 };
    std::atomic<double> lastBpm { 124.0 };
    std::atomic<int> styleIndex { 0 };

    void loadPreset (int style);
    void newPercLayers (juce::Random&);
    void mutate (float amount, juce::Random&);
    int  rollHit (int lane, int step, juce::Random&) const;   // 0 = no hit, else velocity 1..127
    juce::MidiFile buildMidi (int bars) const;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    void setParam (const juce::String& id, float plainValue);
    void onStep (int step, int stepIndexAbs, double bpm);

    struct Pending { int lane; float vel; int delay; };
    std::array<Pending, 512> pending {};
    int numPending = 0;

    std::array<stepbox::DrumVoice, stepbox::numLanes> voices;
    juce::Random audioRandom;
    double sampleRate = 44100.0, internalPpq = 0.0;
    int lastStep = -1;
    bool wasPlaying = false;
    float laneGainL[stepbox::numLanes] {}, laneGainR[stepbox::numLanes] {};

    std::atomic<float> *chaos {}, *velHuman {}, *timeHuman {}, *swing {}, *density {}, *evolve {}, *master {}, *freeRun {}, *freeBpm {};
    std::atomic<float> *lvl[stepbox::numLanes] {}, *chance[stepbox::numLanes] {}, *mute[stepbox::numLanes] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepboxProcessor)
};
