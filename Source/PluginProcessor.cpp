#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

static const char* kNoteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout NayAudioProcessor::createLayout()
{
    using F = juce::AudioParameterFloat;
    using B = juce::AudioParameterBool;
    using R = juce::NormalisableRange<float>;

    juce::AudioProcessorValueTreeState::ParameterLayout l;

    l.add (std::make_unique<F> (juce::ParameterID { "gain", 1 },     "Gain",         R (0.f, 1.f, 0.001f), 0.8f));
    l.add (std::make_unique<F> (juce::ParameterID { "attack", 1 },   "Attack",       R (0.002f, 1.f, 0.001f, 0.4f), 0.03f));
    l.add (std::make_unique<F> (juce::ParameterID { "release", 1 },  "Release",      R (0.02f, 3.f, 0.001f, 0.4f), 0.25f));
    l.add (std::make_unique<F> (juce::ParameterID { "glide", 1 },    "Legato Glide", R (0.f, 500.f, 1.f, 0.5f), 60.f));
    l.add (std::make_unique<F> (juce::ParameterID { "vibDepth", 1 }, "Vib Depth",    R (0.f, 100.f, 0.1f), 12.f));
    l.add (std::make_unique<F> (juce::ParameterID { "vibRate", 1 },  "Vib Speed",    R (0.5f, 10.f, 0.01f), 5.f));
    l.add (std::make_unique<F> (juce::ParameterID { "tune", 1 },     "Tune Amount",  R (0.f, 100.f, 0.1f), 50.f));

    for (int i = 0; i < 12; ++i)
        l.add (std::make_unique<B> (juce::ParameterID { "qt" + juce::String (i), 1 },
                                    juce::String ("Quarter-tone ") + kNoteNames[i], false));
    return l;
}

//==============================================================================
NayAudioProcessor::NayAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "NAY_STATE", createLayout())
{
    pGainRaw     = apvts.getRawParameterValue ("gain");
    pAttackRaw   = apvts.getRawParameterValue ("attack");
    pReleaseRaw  = apvts.getRawParameterValue ("release");
    pGlideRaw    = apvts.getRawParameterValue ("glide");
    pVibDepthRaw = apvts.getRawParameterValue ("vibDepth");
    pVibRateRaw  = apvts.getRawParameterValue ("vibRate");
    pTuneRaw     = apvts.getRawParameterValue ("tune");

    for (int i = 0; i < 12; ++i)
        qtRaw[i] = apvts.getRawParameterValue ("qt" + juce::String (i));
}

bool NayAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void NayAudioProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    adsr.setSampleRate (sampleRate);
    adsr.reset();
    held.clear();
    current.reset();
}

//==============================================================================
// File names must start with a note: "C4.wav", "F#3.wav", "Bb2.wav" or a MIDI number "60.wav".
int NayAudioProcessor::parseRootNote (const juce::String& fileName)
{
    auto t = fileName.trim().upToFirstOccurrenceOf ("_", false, false)
                            .upToFirstOccurrenceOf (" ", false, false);
    if (t.isEmpty())
        return -1;

    if (t.containsOnly ("0123456789"))
    {
        const int n = t.getIntValue();
        return n <= 127 ? n : -1;
    }

    static const int base[7] = { 9, 11, 0, 2, 4, 5, 7 }; // A B C D E F G
    const auto c = juce::CharacterFunctions::toUpperCase (t[0]);
    if (c < 'A' || c > 'G')
        return -1;

    int pc = base[c - 'A'];
    int idx = 1;
    if (t[idx] == '#')      { ++pc; ++idx; }
    else if (t[idx] == 'b') { --pc; ++idx; }

    const auto oct = t.substring (idx);
    if (oct.isEmpty() || ! oct.containsOnly ("-0123456789"))
        return -1;

    const int note = (oct.getIntValue() + 1) * 12 + pc;
    return (note >= 0 && note <= 127) ? note : -1;
}

int NayAudioProcessor::loadSampleFolder (const juce::File& folder)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();

    auto newBank = std::make_shared<NayBank>();

    for (const auto& f : folder.findChildFiles (juce::File::findFiles, false, "*.wav;*.aif;*.aiff;*.flac"))
    {
        const int root = parseRootNote (f.getFileNameWithoutExtension());
        if (root < 0)
            continue;

        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (f));
        if (reader == nullptr || reader->lengthInSamples < 4)
            continue;

        const int len = (int) juce::jmin<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 30.0));
        const int chans = (int) reader->numChannels;

        juce::AudioBuffer<float> tmp (chans, len);
        reader->read (&tmp, 0, len, 0, true, true);

        auto s = std::make_shared<NaySample>();
        s->rootNote = root;
        s->sampleRate = reader->sampleRate;
        s->data.setSize (1, len);
        s->data.clear();
        for (int ch = 0; ch < chans; ++ch)
            s->data.addFrom (0, 0, tmp, ch, 0, len, 1.0f / (float) chans);

        newBank->push_back (s);
    }

    std::sort (newBank->begin(), newBank->end(),
               [] (const auto& a, const auto& b) { return a->rootNote < b->rootNote; });

    const int count = (int) newBank->size();

    {
        juce::SpinLock::ScopedLockType lock (bankLock);
        bank = newBank;
    }

    samplePath = folder.getFullPathName();
    sampleInfo = count > 0 ? juce::String (count) + " samples loaded"
                           : juce::String ("No valid samples (name files like C4.wav)");
    return count;
}

//==============================================================================
double NayAudioProcessor::noteTarget (int note) const
{
    // A toggled key is lowered by Tune Amount cents (default 50 = quarter tone).
    return (double) note - (qt[note % 12] ? (double) pTune * 0.01 : 0.0);
}

void NayAudioProcessor::handleMidi (const juce::MidiMessage& msg, const std::shared_ptr<const NayBank>& b)
{
    if (msg.isNoteOn())
    {
        const int n = msg.getNoteNumber();
        const bool legato = ! held.empty() && adsr.isActive();

        held.erase (std::remove (held.begin(), held.end(), n), held.end());
        held.push_back (n);
        target = noteTarget (n);

        if (! legato && b != nullptr && ! b->empty())
        {
            std::shared_ptr<const NaySample> best;
            int bestDist = 1000;
            for (const auto& s : *b)
            {
                const int d = std::abs (s->rootNote - n);
                if (d < bestDist) { bestDist = d; best = s; }
            }

            current = best;
            playPos = 0.0;
            pitch = target;
            adsr.noteOn();
        }
    }
    else if (msg.isNoteOff())
    {
        const int n = msg.getNoteNumber();
        held.erase (std::remove (held.begin(), held.end(), n), held.end());

        if (held.empty())
            adsr.noteOff();
        else
            target = noteTarget (held.back()); // glide back to the previous held note
    }
    else if (msg.isController() && msg.getControllerNumber() == 1)
    {
        modWheel = (float) msg.getControllerValue() / 127.0f;
    }
    else if (msg.isPitchWheel())
    {
        bendSemis = ((float) msg.getPitchWheelValue() - 8192.0f) / 8192.0f * 2.0f;
    }
    else if (msg.isAllNotesOff() || msg.isAllSoundOff())
    {
        held.clear();
        adsr.reset();
    }
}

void NayAudioProcessor::render (juce::AudioBuffer<float>& buf, int start, int num)
{
    if (num <= 0 || current == nullptr || ! adsr.isActive())
        return;

    const auto& s = *current;
    const int len = s.data.getNumSamples();
    if (len < 4)
        return;

    const float* src = s.data.getReadPointer (0);
    float* L = buf.getWritePointer (0, start);
    float* R = buf.getNumChannels() > 1 ? buf.getWritePointer (1, start) : nullptr;

    // Sustain loop: second half of the sample repeats while the key is held.
    const double loopEnd = (double) len - 1.0;
    const double loopStart = juce::jmin (loopEnd - 1.0, (double) len * 0.5);
    const double glideCoef = pGlide < 1.0f ? 1.0 : 1.0 - std::exp (-1.0 / ((double) pGlide * 0.001 * sr));
    const double rateFix = s.sampleRate / sr;
    const double depthCents = juce::jmin (100.0, (double) pVibDepth + (double) modWheel * 50.0);

    for (int i = 0; i < num; ++i)
    {
        pitch += (target - pitch) * glideCoef;

        vibPhase += (double) pVibRate / sr;
        if (vibPhase >= 1.0)
            vibPhase -= 1.0;
        const double vib = std::sin (juce::MathConstants<double>::twoPi * vibPhase) * depthCents * 0.01;

        const double semis = pitch + (double) bendSemis + vib - (double) s.rootNote;

        while (playPos >= loopEnd)
            playPos -= (loopEnd - loopStart);

        const int i0 = (int) playPos;
        const float frac = (float) (playPos - (double) i0);
        const float smp = src[i0] + frac * (src[i0 + 1] - src[i0]);
        const float out = smp * adsr.getNextSample() * pGain;

        L[i] = out;
        if (R != nullptr)
            R[i] = out;

        playPos += std::pow (2.0, semis / 12.0) * rateFix;

        if (! adsr.isActive())
            break;
    }
}

void NayAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    std::shared_ptr<const NayBank> localBank;
    {
        juce::SpinLock::ScopedTryLockType lock (bankLock);
        if (lock.isLocked())
            localBank = bank;
    }

    pGain     = pGainRaw->load();
    pGlide    = pGlideRaw->load();
    pVibDepth = pVibDepthRaw->load();
    pVibRate  = pVibRateRaw->load();
    pTune     = pTuneRaw->load();
    for (int i = 0; i < 12; ++i)
        qt[i] = qtRaw[i]->load() > 0.5f;

    adsr.setParameters ({ pAttackRaw->load(), 0.05f, 1.0f, pReleaseRaw->load() });

    if (! held.empty())
        target = noteTarget (held.back());

    int pos = 0;
    const int total = buffer.getNumSamples();

    for (const auto metadata : midi)
    {
        const int p = juce::jlimit (0, total, metadata.samplePosition);
        render (buffer, pos, p - pos);
        pos = p;
        handleMidi (metadata.getMessage(), localBank);
    }

    render (buffer, pos, total - pos);
}

//==============================================================================
juce::AudioProcessorEditor* NayAudioProcessor::createEditor()
{
    return new NayAudioProcessorEditor (*this);
}

void NayAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("samplePath", samplePath, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void NayAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto st = juce::ValueTree::fromXml (*xml);
            apvts.replaceState (st);

            const auto path = st.getProperty ("samplePath").toString();
            if (path.isNotEmpty() && juce::File (path).isDirectory())
                loadSampleFolder (juce::File (path));
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NayAudioProcessor();
}
