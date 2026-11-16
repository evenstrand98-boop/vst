#include "PluginProcessor.h"
#include "PluginEditor.h"
using namespace stepbox;

namespace
{
const float velTable[4] = { 0.0f, 55.0f, 96.0f, 124.0f };

struct Preset { double bpm; float swing; const char* rows[numLanes]; };
const Preset presets[2] = {
    { 124.0, 14.0f, { "X...X...X...X...", "....X.......X...", "..............g.", "g...g...g...g.g.",
                      "..x...x...x...x.", "...x..g....x..g.", ".g..g..g.g..g...", "g.gxg.gxg.gxg.gx" } },
    { 126.0, 0.0f,  { "X...X...X...X...", "............x...", "g..x..g...x..g..", "..g..x..g..x..g.",
                      ".......g.......x", ".x....g..x...g..", "..g.....g..x.g..", ".g.g..g.g...g.g." } }
};
int charToVel (char c) { return c == 'X' ? 3 : c == 'x' ? 2 : c == 'g' ? 1 : 0; }
}

juce::AudioProcessorValueTreeState::ParameterLayout StepboxProcessor::createLayout()
{
    using F = juce::AudioParameterFloat;
    using B = juce::AudioParameterBool;
    using R = juce::NormalisableRange<float>;
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<F> (juce::ParameterID { "chaos", 1 },     "Chaos",             R (0.f, 100.f, 1.f), 30.f));
    l.add (std::make_unique<F> (juce::ParameterID { "velHuman", 1 },  "Velocity humanize", R (0.f, 30.f, 1.f),  12.f));
    l.add (std::make_unique<F> (juce::ParameterID { "timeHuman", 1 }, "Timing humanize",   R (0.f, 24.f, 1.f),  6.f));
    l.add (std::make_unique<F> (juce::ParameterID { "swing", 1 },     "Swing",             R (0.f, 60.f, 1.f),  14.f));
    l.add (std::make_unique<F> (juce::ParameterID { "density", 1 },   "Perc density",      R (1.f, 8.f, 1.f),   4.f));
    l.add (std::make_unique<B> (juce::ParameterID { "evolve", 1 },    "Evolve",            true));
    l.add (std::make_unique<F> (juce::ParameterID { "master", 1 },    "Master",            R (0.f, 1.2f, 0.01f), 0.8f));
    l.add (std::make_unique<B> (juce::ParameterID { "freeRun", 1 },   "Free run",          false));
    l.add (std::make_unique<F> (juce::ParameterID { "freeBpm", 1 },   "Free-run tempo",    R (70.f, 160.f, 1.f), 124.f));
    for (int i = 0; i < numLanes; ++i)
    {
        const juce::String n (laneNames[i]);
        l.add (std::make_unique<F> (juce::ParameterID { "lvl" + juce::String (i), 1 },    n + " level",  R (0.f, 1.5f, 0.01f), 1.f));
        l.add (std::make_unique<F> (juce::ParameterID { "chance" + juce::String (i), 1 }, n + " chance", R (0.f, 100.f, 1.f), lanePerc[i] ? 80.f : (i == 2 ? 85.f : 100.f)));
        l.add (std::make_unique<B> (juce::ParameterID { "mute" + juce::String (i), 1 },   n + " mute",   false));
    }
    return l;
}

StepboxProcessor::StepboxProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    auto raw = [this] (const juce::String& id) { return apvts.getRawParameterValue (id); };
    chaos = raw ("chaos"); velHuman = raw ("velHuman"); timeHuman = raw ("timeHuman"); swing = raw ("swing");
    density = raw ("density"); evolve = raw ("evolve"); master = raw ("master"); freeRun = raw ("freeRun"); freeBpm = raw ("freeBpm");
    for (int i = 0; i < numLanes; ++i)
    {
        lvl[i] = raw ("lvl" + juce::String (i)); chance[i] = raw ("chance" + juce::String (i)); mute[i] = raw ("mute" + juce::String (i));
        const float a = lanePan[i] * juce::MathConstants<float>::halfPi;
        laneGainL[i] = std::cos (a) * 1.41421356f; laneGainR[i] = std::sin (a) * 1.41421356f;
        voices[(size_t) i].rng.setSeed (1234 + i);
    }
    for (auto& row : pat) for (auto& c : row) c.store (0);
    audioRandom.setSeedRandomly();
    loadPreset (0);
}

void StepboxProcessor::setParam (const juce::String& id, float v)
{
    if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
}

void StepboxProcessor::loadPreset (int style)
{
    style = juce::jlimit (0, 1, style);
    styleIndex.store (style);
    const auto& p = presets[style];
    for (int l = 0; l < numLanes; ++l)
        for (int s = 0; s < numSteps; ++s)
            pat[(size_t) l][(size_t) s].store ((uint8_t) charToVel (p.rows[l][s]));
    setParam ("swing", p.swing);
    setParam ("freeBpm", (float) p.bpm);
    for (int i = 0; i < numLanes; ++i)
        setParam ("chance" + juce::String (i), lanePerc[i] ? 80.f : (i == 2 ? 85.f : 100.f));
}

void StepboxProcessor::newPercLayers (juce::Random& r)
{
    const int dens = (int) density->load();
    for (int l = 0; l < numLanes; ++l)
    {
        if (! lanePerc[l]) continue;
        const bool shaker = (l == 7);
        const int k = shaker ? juce::jmin (14, dens + 5) : 2 + r.nextInt (dens);
        const int rot = r.nextInt (numSteps);
        const int pool[2][4] = { { 1, 2, 2, 3 }, { 1, 1, 2, 3 } };
        for (int s = 0; s < numSteps; ++s)
        {
            const bool hit = (((s + rot) % numSteps) * k) % numSteps < k;
            pat[(size_t) l][(size_t) s].store (hit ? (uint8_t) pool[shaker ? 1 : 0][r.nextInt (4)] : (uint8_t) 0);
        }
    }
}

void StepboxProcessor::mutate (float amount, juce::Random& r)
{
    for (int l = 1; l < numLanes; ++l)
        for (int s = 0; s < numSteps; ++s)
        {
            if (r.nextFloat() > amount * 0.12f) continue;
            const int v = pat[(size_t) l][(size_t) s].load();
            int nv = v;
            if (v > 0) nv = r.nextBool() ? 0 : juce::jlimit (1, 3, v + (r.nextBool() ? -1 : 1));
            else if (laneExtra[l]) nv = 1 + r.nextInt (2);
            pat[(size_t) l][(size_t) s].store ((uint8_t) nv);
        }
}

int StepboxProcessor::rollHit (int lane, int step, juce::Random& r) const
{
    int v = pat[(size_t) lane][(size_t) step].load();
    const float c = chance[lane]->load() / 100.0f, k = chaos->load() / 100.0f;
    if (v > 0)
    {
        if (r.nextFloat() > c) return 0;
        if (lane > 0 && r.nextFloat() < k * 0.12f) return 0;
    }
    else if (laneExtra[lane] && r.nextFloat() < k * 0.15f) v = 1;
    else return 0;
    const float vel = velTable[v] + (r.nextFloat() * 2.0f - 1.0f) * velHuman->load();
    return juce::jlimit (1, 127, (int) std::lround (vel));
}

void StepboxProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr;
    numPending = 0; lastStep = -1; wasPlaying = false;
    for (auto& v : voices) v.active = false;
}

bool StepboxProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void StepboxProcessor::onStep (int step, int stepAbs, double bpm)
{
    currentStep.store (step);
    if (step == 0 && stepAbs >= 64 && (stepAbs / 16) % 4 == 0 && evolve->load() > 0.5f)
        mutate (juce::jmax (0.4f, chaos->load() / 100.0f) * 2.0f, audioRandom);

    const double stepSamples = sampleRate * 60.0 / bpm / 4.0;
    for (int l = 0; l < numLanes; ++l)
    {
        if (mute[l]->load() > 0.5f) continue;
        const int vel = rollHit (l, step, audioRandom);
        if (vel == 0 || numPending >= (int) pending.size()) continue;
        double d = (step & 1) ? swing->load() / 100.0 * stepSamples : 0.0;
        d += (audioRandom.nextFloat() * 2.0 - 1.0) * timeHuman->load() / 480.0 * sampleRate * 60.0 / bpm;
        pending[(size_t) numPending++] = { l, (float) vel / 127.0f * lvl[l]->load(), juce::jmax (0, (int) d) };
    }
}

void StepboxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    const int n = buffer.getNumSamples();

    // live MIDI triggers (Drum Rack-style note map), so the plugin also works as a plain drum instrument
    for (const auto m : midi)
    {
        const auto msg = m.getMessage();
        if (! msg.isNoteOn() || numPending >= (int) pending.size()) continue;
        for (int l = 0; l < numLanes; ++l)
            if (laneNotes[l] == msg.getNoteNumber())
                pending[(size_t) numPending++] = { l, msg.getFloatVelocity() * lvl[l]->load(), m.samplePosition };
    }

    // transport: host playhead, or the internal clock when "Free run" is on
    bool playing = false; double bpm = 120.0, ppq = 0.0;
    if (freeRun->load() > 0.5f) { playing = true; bpm = freeBpm->load(); ppq = internalPpq; }
    else if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            playing = pos->getIsPlaying();
            bpm = pos->getBpm().orFallback (120.0);
            ppq = pos->getPpqPosition().orFallback (0.0);
        }
    bpm = juce::jmax (20.0, bpm);
    lastBpm.store (bpm);
    if (! playing || ! wasPlaying) lastStep = -1;
    wasPlaying = playing;
    if (! playing) currentStep.store (-1);

    const double ppqPerSample = bpm / 60.0 / sampleRate;
    const float mst = master->load();
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < n; ++i)
    {
        if (playing)
        {
            const int st = (int) std::floor ((ppq + i * ppqPerSample) * 4.0 + 1e-6);
            if (st != lastStep) { lastStep = st; onStep (((st % 16) + 16) % 16, st, bpm); }
        }
        for (int p = 0; p < numPending;)
        {
            if (pending[(size_t) p].delay <= 0)
            {
                const auto h = pending[(size_t) p];
                voices[(size_t) h.lane].trigger (h.lane, h.vel, sampleRate);
                if (h.lane == 3) voices[4].chokeNow();       // closed hat chokes open hat
                pending[(size_t) p] = pending[(size_t) --numPending];
            }
            else { --pending[(size_t) p].delay; ++p; }
        }
        float sl = 0.0f, sr = 0.0f;
        for (int l = 0; l < numLanes; ++l)
        {
            const float v = voices[(size_t) l].render();
            sl += v * laneGainL[l]; sr += v * laneGainR[l];
        }
        L[i] = std::tanh (sl * mst);
        if (R) R[i] = std::tanh (sr * mst);
    }
    if (freeRun->load() > 0.5f) internalPpq += n * ppqPerSample;
    else internalPpq = 0.0;
}

juce::MidiFile StepboxProcessor::buildMidi (int bars) const
{
    juce::Random r;
    juce::MidiMessageSequence seq;
    const int end = bars * 1920;
    seq.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::lround (60000000.0 / lastBpm.load())), 0);
    seq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0);
    for (int b = 0; b < bars; ++b)
        for (int s = 0; s < numSteps; ++s)
            for (int l = 0; l < numLanes; ++l)
            {
                if (mute[l]->load() > 0.5f) continue;
                const int vel = rollHit (l, s, r);
                if (vel == 0) continue;
                double tick = (b * 16 + s) * 120.0 + ((s & 1) ? swing->load() / 100.0 * 120.0 : 0.0) + (r.nextFloat() * 2.0 - 1.0) * timeHuman->load();
                tick = juce::jlimit (0.0, (double) end - 2.0, tick);
                auto on = juce::MidiMessage::noteOn (1, laneNotes[l], (juce::uint8) vel); on.setTimeStamp (tick);
                auto off = juce::MidiMessage::noteOff (1, laneNotes[l]);                  off.setTimeStamp (juce::jmin ((double) end - 1.0, tick + 50.0));
                seq.addEvent (on); seq.addEvent (off);
            }
    auto eot = juce::MidiMessage::endOfTrack(); eot.setTimeStamp ((double) end);
    seq.addEvent (eot);
    juce::MidiFile f;
    f.setTicksPerQuarterNote (480);
    f.addTrack (seq);
    return f;
}

void StepboxProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    juce::String s;
    for (auto& row : pat) for (auto& c : row) s << juce::String::charToString ((juce::juce_wchar) ('0' + c.load()));
    state.setProperty ("pattern", s, nullptr);
    state.setProperty ("style", styleIndex.load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void StepboxProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            apvts.replaceState (vt);
            const auto s = vt.getProperty ("pattern").toString();
            if (s.length() == numLanes * numSteps)
                for (int l = 0; l < numLanes; ++l)
                    for (int st = 0; st < numSteps; ++st)
                        pat[(size_t) l][(size_t) st].store ((uint8_t) juce::jlimit (0, 3, (int) s[l * numSteps + st] - '0'));
            styleIndex.store ((int) vt.getProperty ("style", 0));
        }
}

juce::AudioProcessorEditor* StepboxProcessor::createEditor() { return new StepboxEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new StepboxProcessor(); }
