# Nay VST3 (JUCE sampler)

Monophonic legato sampler. UI: 12 piano-style yellow buttons (5 sharps on top, 7 naturals below).
A lit (blue) button lowers that note by "Tune Amount" cents (default 50 = quarter tone).

## Build (Windows, Visual Studio 2022 + CMake)
    cmake -B build -G "Visual Studio 17 2022"
    cmake --build build --config Release
Output: build/NayVST_artefacts/Release/VST3/Nay.vst3  -> copy to C:\Program Files\Common Files\VST3

## Samples
Put WAV/AIFF/FLAC files in one folder, named by the note they were recorded at:
C4.wav, D#4.wav, Bb3.wav ... or MIDI numbers: 60.wav, 63.wav
Click "Load Samples..." and pick the folder. The nearest sample is pitch-shifted.
The second half of each sample is looped while a key is held, so use samples with a steady sustain.

## Controls
Glide (legato portamento), Vib Depth/Speed, Tune Amt, Attack, Release, Gain.
Mod wheel (CC1) adds vibrato depth. Pitch wheel = +/-2 semitones.
