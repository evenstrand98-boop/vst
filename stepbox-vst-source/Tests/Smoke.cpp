#include "../Source/PluginProcessor.h"
#include <cstdio>
int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    StepboxProcessor p;
    p.setPlayConfigDetails (0, 2, 44100.0, 512);
    p.prepareToPlay (44100.0, 512);
    auto* fr = p.apvts.getParameter ("freeRun"); fr->setValueNotifyingHost (1.0f);
    juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer midi;
    float peak = 0; bool bad = false; int nonzeroBlocks = 0;
    for (int b = 0; b < 400; ++b)
    {
        buf.clear(); p.processBlock (buf, midi);
        float bp = buf.getMagnitude (0, 512);
        if (std::isnan (bp) || bp > 1.01f) bad = true;
        if (bp > 0.01f) ++nonzeroBlocks;
        peak = juce::jmax (peak, bp);
    }
    std::printf ("peak=%.3f nonzeroBlocks=%d/400 bad=%d step=%d\n", peak, nonzeroBlocks, (int) bad, p.currentStep.load());

    juce::MemoryBlock st; p.getStateInformation (st);
    StepboxProcessor q; q.prepareToPlay (44100.0, 512);
    p.loadPreset (1); // change p, then restore q from p's earlier state
    q.setStateInformation (st.getData(), (int) st.getSize());
    int diff = 0; for (int l = 0; l < 8; ++l) for (int s = 0; s < 16; ++s) diff += (p.pat[l][s] != q.pat[l][s]);
    std::printf ("state roundtrip: %d cells differ from minimal (expect >0 since q restored house/mutated)\n", diff);

    auto mf = p.buildMidi (2); juce::MemoryOutputStream mo; mf.writeTo (mo);
    juce::MidiFile back; juce::MemoryInputStream mi (mo.getData(), mo.getDataSize(), false); back.readFrom (mi);
    std::printf ("midi bytes=%d tracks=%d events=%d ppq=%d\n", (int) mo.getDataSize(), back.getNumTracks(), back.getTrack (0)->getNumEvents(), back.getTimeFormat());
    return bad || nonzeroBlocks < 20;
}
