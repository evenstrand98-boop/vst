# Stepbox

A drum machine VST3 instrument: house and minimal patterns, perc layers, built-in randomness.
It synthesizes its own drums, follows the host transport, and can drag a MIDI clip of its pattern onto any track.

Why an instrument and not a MIDI effect: Ableton Live can't host VST MIDI effects or route MIDI out of a VST,
so Stepbox makes the sound itself. Load it on a MIDI track like any instrument and press play.

## Get the plugin (no coding needed)
1. Create a GitHub repo and push this folder to it.
2. Open the Actions tab, run "Build Stepbox" (it also runs on every push).
3. Download the artifact for your OS (Windows or macOS), a `Stepbox.vst3` folder.
4. Install:
   - Windows: copy `Stepbox.vst3` to `C:\Program Files\Common Files\VST3\`
   - macOS: copy to `/Library/Audio/Plug-Ins/VST3/` (or `~/Library/Audio/Plug-Ins/VST3/`), then run
     `xattr -dr com.apple.quarantine Stepbox.vst3` because the build isn't notarized.
5. In Live: Preferences > Plug-ins, enable VST3 and rescan. Stepbox appears under Plug-ins > Instruments.

## Build locally
Needs CMake 3.22+ and a C++17 compiler (Visual Studio 2022 or Xcode).

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release --target Stepbox_VST3

To auto-install after building, set `COPY_PLUGIN_AFTER_BUILD TRUE` in CMakeLists.txt. JUCE 8.0.8 is fetched automatically.
Optional headless test: `-DSTEPBOX_TESTS=ON`, build target `StepboxSmoke`.

## Using it
- Play in Live: the pattern follows the transport and locks to bar lines.
- Free run: ignores the host and runs its own clock (handy standalone or for auditioning).
- House / Minimal load a preset (also sets swing and tempo for free run).
- Click steps to cycle ghost, normal, accent, off; right-click clears. Pink lanes are perc layers.
- Per lane: mute, level, chance (how often its steps fire).
- Chaos drops hits and adds ghost notes on hats, rim and perc; kick is never dropped by chaos.
- Velocity/timing humanize and swing apply live. Evolve mutates the pattern every 4 bars.
- New perc layers regenerates Euclidean perc rhythms (density sets how busy). Mutate nudges the whole pattern.
- Drag MIDI clip: drag the box into an Ableton MIDI track to get 1 to 16 bars with the randomness baked in.
- MIDI in: notes C1, D#1, C#1, F#1, A#1, D2, G#2, A#3 trigger the lanes (Drum Rack layout), so you can also play it.
- Pattern and all settings are saved with your Live set.
