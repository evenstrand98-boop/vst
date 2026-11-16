#include "PluginEditor.h"
using namespace stepbox;

namespace
{
const juce::Colour bg (0xff12171b), panel (0xff1b2228), cellC (0xff26303a), line (0xff2f3b43),
                   ink (0xffe5ebee), mute (0xff8fa1ab), acc (0xff7f98ff), percC (0xffff7ba8);
}

// ------------------------------------------------------------------ step grid
juce::Rectangle<float> StepGrid::cell (int lane, int step) const
{
    const float gap = 10.0f, cw = ((float) getWidth() - 3.0f * gap) / 16.0f, lh = (float) getHeight() / (float) numLanes;
    const float x = (float) (step / 4) * (4.0f * cw + gap) + (float) (step % 4) * cw;
    return { x + 1.5f, (float) lane * lh + 4.0f, cw - 3.0f, lh - 8.0f };
}

void StepGrid::paint (juce::Graphics& g)
{
    const int now = proc.currentStep.load();
    for (int l = 0; l < numLanes; ++l)
    {
        const auto base = lanePerc[l] ? percC : acc;
        const bool muted = proc.apvts.getRawParameterValue ("mute" + juce::String (l))->load() > 0.5f;
        for (int s = 0; s < numSteps; ++s)
        {
            const int v = proc.pat[(size_t) l][(size_t) s].load();
            const float mix[4] = { 0.0f, 0.35f, 0.75f, 1.0f };
            auto c = cellC.interpolatedWith (base, mix[v]);
            if (muted) c = c.withAlpha (0.35f);
            const auto r = cell (l, s);
            g.setColour (c);          g.fillRoundedRectangle (r, 3.0f);
            g.setColour (v == 3 ? ink : line); g.drawRoundedRectangle (r, 3.0f, 1.0f);
            if (s == now) { g.setColour (ink); g.drawRoundedRectangle (r.expanded (1.0f), 3.0f, 2.0f); }
        }
    }
}

void StepGrid::mouseDown (const juce::MouseEvent& e)
{
    for (int l = 0; l < numLanes; ++l)
        for (int s = 0; s < numSteps; ++s)
            if (cell (l, s).contains (e.position))
            {
                auto& c = proc.pat[(size_t) l][(size_t) s];
                c.store (e.mods.isRightButtonDown() ? (uint8_t) 0 : (uint8_t) ((c.load() + 1) % 4));
                repaint();
                return;
            }
}

void DragZone::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (cellC); g.fillRoundedRectangle (r, 4.0f);
    g.setColour (acc);   g.drawRoundedRectangle (r, 4.0f, 1.5f);
    g.setColour (ink);   g.setFont (14.0f);
    g.drawText ("Drag MIDI clip to a track", getLocalBounds(), juce::Justification::centred);
}

// ------------------------------------------------------------------ editor
void StepboxEditor::addKnob (Knob& k, const juce::String& id, const juce::String& text)
{
    k.s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 18);
    k.s.setColour (juce::Slider::rotarySliderFillColourId, acc);
    k.s.setColour (juce::Slider::thumbColourId, ink);
    k.l.setText (text, juce::dontSendNotification);
    k.l.setJustificationType (juce::Justification::centred);
    k.l.setColour (juce::Label::textColourId, mute);
    addAndMakeVisible (k.s); addAndMakeVisible (k.l);
    k.a = std::make_unique<SA> (proc.apvts, id, k.s);
}

StepboxEditor::StepboxEditor (StepboxProcessor& p) : AudioProcessorEditor (&p), proc (p), grid (p)
{
    setSize (980, 620);
    getLookAndFeel().setColour (juce::ResizableWindow::backgroundColourId, bg);
    addAndMakeVisible (grid);

    for (auto* b : { &house, &minimal, &newPerc, &mutateBtn }) addAndMakeVisible (b);
    house.onClick   = [this] { proc.loadPreset (0); };
    minimal.onClick = [this] { proc.loadPreset (1); };
    newPerc.onClick = [this] { proc.newPercLayers (uiRandom); };
    mutateBtn.onClick = [this] { proc.mutate (juce::jmax (0.5f, proc.apvts.getRawParameterValue ("chaos")->load() / 100.0f) * 2.0f, uiRandom); };

    addAndMakeVisible (evolveBtn); addAndMakeVisible (freeBtn);
    evolveA = std::make_unique<BA> (proc.apvts, "evolve", evolveBtn);
    freeA   = std::make_unique<BA> (proc.apvts, "freeRun", freeBtn);

    addKnob (chaosK, "chaos", "Chaos");        addKnob (velK, "velHuman", "Velocity humanize");
    addKnob (timeK, "timeHuman", "Timing humanize"); addKnob (swingK, "swing", "Swing");
    addKnob (densK, "density", "Perc density"); addKnob (masterK, "master", "Master");
    addKnob (tempoK, "freeBpm", "Free-run tempo");

    for (int i = 0; i < numLanes; ++i)
    {
        auto& m = muteB[(size_t) i]; m.setButtonText ("M"); addAndMakeVisible (m);
        laneBA.push_back (std::make_unique<BA> (proc.apvts, "mute" + juce::String (i), m));
        for (auto* s : { &lvlS[(size_t) i], &chanceS[(size_t) i] })
        {
            s->setSliderStyle (juce::Slider::LinearHorizontal);
            s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            s->setColour (juce::Slider::trackColourId, lanePerc[i] ? percC : acc);
            addAndMakeVisible (*s);
        }
        laneSA.push_back (std::make_unique<SA> (proc.apvts, "lvl" + juce::String (i), lvlS[(size_t) i]));
        laneSA.push_back (std::make_unique<SA> (proc.apvts, "chance" + juce::String (i), chanceS[(size_t) i]));
    }

    bars.addItemList ({ "1 bar", "2 bars", "4 bars", "8 bars", "16 bars" }, 1);
    bars.setSelectedId (4, juce::dontSendNotification);
    addAndMakeVisible (bars);
    addAndMakeVisible (dragZone);
    dragZone.onDragOut = [this] { dragOut(); };
}

StepboxEditor::~StepboxEditor() = default;

void StepboxEditor::dragOut()
{
    static const int counts[] = { 1, 2, 4, 8, 16 };
    const int nb = counts[juce::jlimit (0, 4, bars.getSelectedId() - 1)];
    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("Stepbox_" + juce::String (proc.styleIndex.load() == 0 ? "house_" : "minimal_")
                                         + juce::String (juce::roundToInt (proc.lastBpm.load())) + "bpm_" + juce::String (nb) + "bars.mid");
    file.deleteFile();
    if (auto os = file.createOutputStream())
    {
        proc.buildMidi (nb).writeTo (*os);
        os.reset();
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);
    }
}

void StepboxEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);
    g.setColour (panel); g.fillRect (0, 0, getWidth(), 54); g.fillRect (0, 64 + laneH * numLanes + 10, getWidth(), getHeight());
    g.setColour (ink); g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawText ("Stepbox", 16, 8, 140, 38, juce::Justification::centredLeft);
    g.setFont (13.0f);
    for (int i = 0; i < numLanes; ++i)
    {
        const int y = gridY + i * laneH;
        g.setColour (lanePerc[i] ? percC : ink);
        g.drawText (laneNames[i], 46, y, 76, laneH, juce::Justification::centredLeft);
        g.setColour (mute);
        g.drawText (juce::MidiMessage::getMidiNoteName (laneNotes[i], true, true, 3), 112, y, 36, laneH, juce::Justification::centredLeft);
    }
    g.setColour (mute);
    g.drawText ("Level", gridX + gridW + 12, gridY - 18, 110, 16, juce::Justification::centred);
    g.drawText ("Chance", gridX + gridW + 126, gridY - 18, 110, 16, juce::Justification::centred);
    g.drawText ("Click a step to cycle ghost, normal, accent, off. Right-click clears. Pink lanes are the perc layers.",
                16, gridY + laneH * numLanes + 14, 700, 18, juce::Justification::centredLeft);
}

void StepboxEditor::resized()
{
    house.setBounds (150, 12, 80, 30);  minimal.setBounds (236, 12, 80, 30);
    newPerc.setBounds (330, 12, 120, 30); mutateBtn.setBounds (456, 12, 80, 30);
    evolveBtn.setBounds (548, 12, 160, 30); freeBtn.setBounds (712, 12, 90, 30);
    bars.setBounds (812, 12, 80, 30);
    dragZone.setBounds (getWidth() - 214, 62 + laneH * numLanes + 36, 198, 34);

    grid.setBounds (gridX, gridY, gridW, laneH * numLanes);
    for (int i = 0; i < numLanes; ++i)
    {
        const int y = gridY + i * laneH;
        muteB[(size_t) i].setBounds (14, y + 8, 30, 24);
        lvlS[(size_t) i].setBounds (gridX + gridW + 12, y, 110, laneH);
        chanceS[(size_t) i].setBounds (gridX + gridW + 126, y, 110, laneH);
    }
    Knob* ks[] = { &chaosK, &velK, &timeK, &swingK, &densK, &masterK, &tempoK };
    const int ky = gridY + laneH * numLanes + 46;
    for (int i = 0; i < 7; ++i)
    {
        ks[i]->l.setBounds (16 + i * 112, ky, 104, 16);
        ks[i]->s.setBounds (16 + i * 112, ky + 16, 104, 100);
    }
}
