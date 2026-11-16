#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace stepbox
{
constexpr int numLanes = 8;
constexpr int numSteps = 16;
inline const char* laneNames[numLanes] = { "Kick", "Clap", "Rim", "Closed hat", "Open hat", "Perc A", "Perc B", "Shaker" };
inline const int laneNotes[numLanes]   = { 36, 39, 37, 42, 46, 47, 56, 70 };
inline const bool laneExtra[numLanes]  = { false, false, true, true, true, true, true, true }; // lanes that can get chaos ghost notes
inline const bool lanePerc[numLanes]   = { false, false, false, false, false, true, true, true };
inline const float lanePan[numLanes]   = { 0.50f, 0.50f, 0.40f, 0.58f, 0.45f, 0.33f, 0.67f, 0.56f };

// Cytomic/Zavalishin TPT state-variable filter
struct Svf
{
    float ic1 = 0, ic2 = 0, g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0;
    void set (float freq, float q, double sr)
    {
        g = std::tan (juce::MathConstants<float>::pi * freq / (float) sr);
        k = 1.0f / q;
        a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    void reset() { ic1 = ic2 = 0; }
    inline void run (float x, float& band, float& high)
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
        band = v1; high = x - k * v1 - v2;
    }
};

// One monophonic synthesized drum voice. Each lane owns one; retriggering restarts it.
struct DrumVoice
{
    int lane = 0;
    bool active = false;
    float vel = 0, t = 0, dt = 0, phase = 0, phase2 = 0, choke = 1.0f, chokeCoef = 1.0f;
    double sr = 44100;
    Svf filt;
    juce::Random rng;

    void trigger (int l, float v, double sampleRate)
    {
        lane = l; vel = v; sr = sampleRate; dt = (float) (1.0 / sampleRate);
        t = 0; phase = phase2 = 0; choke = 1.0f; chokeCoef = 1.0f; active = true; filt.reset();
        switch (lane)
        {
            case 1: filt.set (1400.0f, 1.2f, sr); break;
            case 2: filt.set (3000.0f, 2.0f, sr); break;
            case 3: case 4: filt.set (7500.0f, 0.7f, sr); break;
            case 6: filt.set (680.0f, 1.5f, sr); break;
            case 7: filt.set (6500.0f, 0.9f, sr); break;
            default: break;
        }
    }
    void chokeNow() { if (active) chokeCoef = std::exp (-1.0f / (0.004f * (float) sr)); }

    inline float noise() { return rng.nextFloat() * 2.0f - 1.0f; }

    float render()
    {
        if (! active) return 0.0f;
        constexpr float twoPi = juce::MathConstants<float>::twoPi;
        float out = 0, band = 0, high = 0, e = 0;

        switch (lane)
        {
            case 0: // kick
            {
                const float f = 42.0f + 118.0f * std::exp (-t / 0.03f);
                phase += twoPi * f * dt; if (phase > twoPi) phase -= twoPi;
                e = std::exp (-t / 0.11f);
                out = std::tanh (1.4f * std::sin (phase) * e) * 0.95f + (t < 0.002f ? noise() * 0.15f : 0.0f);
                if (e < 0.001f) active = false;
                break;
            }
            case 1: // clap: three quick bursts and a tail
            {
                filt.run (noise(), band, high);
                e = t < 0.033f ? 0.6f * std::exp (-std::fmod (t, 0.011f) / 0.0029f)
                               : 0.7f * std::exp (-(t - 0.033f) / 0.026f);
                out = band * e * 2.2f;
                if (t > 0.3f) active = false;
                break;
            }
            case 2: // rim
            {
                const float f = 1200.0f + 550.0f * std::exp (-t / 0.012f);
                phase += twoPi * f * dt; if (phase > twoPi) phase -= twoPi;
                filt.run (noise(), band, high);
                out = ((phase < juce::MathConstants<float>::pi ? 1.0f : -1.0f) * 0.25f * std::exp (-t / 0.006f)
                      + band * 0.9f * std::exp (-t / 0.005f));
                if (t > 0.08f) active = false;
                break;
            }
            case 3: case 4: // closed / open hat
            {
                filt.run (noise(), band, high);
                e = std::exp (-t / (lane == 3 ? 0.02f : 0.13f));
                out = high * 0.35f * e;
                if (e < 0.001f) active = false;
                break;
            }
            case 5: // perc A: tom-ish blip
            {
                const float f = 190.0f + 150.0f * std::exp (-t / 0.04f);
                phase += twoPi * f * dt; if (phase > twoPi) phase -= twoPi;
                e = std::exp (-t / 0.05f);
                out = std::sin (phase) * e * 0.7f;
                if (e < 0.001f) active = false;
                break;
            }
            case 6: // perc B: cowbell-ish
            {
                phase  += twoPi * 540.0f * dt; if (phase  > twoPi) phase  -= twoPi;
                phase2 += twoPi * 810.0f * dt; if (phase2 > twoPi) phase2 -= twoPi;
                const float sq = (phase < juce::MathConstants<float>::pi ? 1.0f : -1.0f) + (phase2 < juce::MathConstants<float>::pi ? 1.0f : -1.0f);
                filt.run (sq * 0.3f, band, high);
                e = std::exp (-t / 0.04f);
                out = band * e * 1.2f;
                if (e < 0.001f) active = false;
                break;
            }
            default: // shaker
            {
                filt.run (noise(), band, high);
                e = (1.0f - std::exp (-t / 0.004f)) * std::exp (-t / 0.022f);
                out = band * e * 1.3f;
                if (t > 0.25f) active = false;
                break;
            }
        }
        t += dt;
        choke *= chokeCoef;
        if (choke < 0.001f) active = false;
        return out * vel * choke;
    }
};
}
