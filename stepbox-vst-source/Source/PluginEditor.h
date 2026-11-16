#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class StepGrid : public juce::Component, private juce::Timer
{
public:
    explicit StepGrid (StepboxProcessor& p) : proc (p) { startTimerHz (30); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    void timerCallback() override { repaint(); }
    juce::Rectangle<float> cell (int lane, int step) const;
    StepboxProcessor& proc;
};

class DragZone : public juce::Component
{
public:
    std::function<void()> onDragOut;
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent& e) override { if (e.getDistanceFromDragStart() > 6 && onDragOut) onDragOut(); }
    void mouseEnter (const juce::MouseEvent&) override { setMouseCursor (juce::MouseCursor::DraggingHandCursor); }
};

class StepboxEditor : public juce::AudioProcessorEditor
{
public:
    explicit StepboxEditor (StepboxProcessor&);
    ~StepboxEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    struct Knob { juce::Slider s; juce::Label l; std::unique_ptr<SA> a; };

    void addKnob (Knob&, const juce::String& id, const juce::String& text);
    void dragOut();

    StepboxProcessor& proc;
    StepGrid grid;
    DragZone dragZone;
    juce::ComboBox bars;
    juce::TextButton house { "House" }, minimal { "Minimal" }, newPerc { "New perc layers" }, mutateBtn { "Mutate" };
    juce::ToggleButton evolveBtn { "Evolve every 4 bars" }, freeBtn { "Free run" };
    std::unique_ptr<BA> evolveA, freeA;
    Knob chaosK, velK, timeK, swingK, densK, masterK, tempoK;
    std::array<juce::ToggleButton, stepbox::numLanes> muteB;
    std::array<juce::Slider, stepbox::numLanes> lvlS, chanceS;
    std::vector<std::unique_ptr<SA>> laneSA;
    std::vector<std::unique_ptr<BA>> laneBA;
    juce::Random uiRandom;

    static constexpr int gridX = 150, gridY = 64, laneH = 40, gridW = 560;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepboxEditor)
};
