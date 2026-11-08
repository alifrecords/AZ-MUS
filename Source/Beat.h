// =============================================================================
//  AZ MUS — AZ BEAT: beat yaratish treki
//  · Battery tuzilmasi: 4×5 = 20 pad (C1–G2), har padda sample, gain/pan/tune, start/end, attack/decay,
//    reverse, choke guruhi, mute/solo, temp bilan sinxron loop
//  · Xfer Nerve mexanizmi: har pad uchun step-sekvenser (velocity, ratchet), swing, uzunlik 16/32/64,
//    loopni transient bo'yicha 20 padga bo'lish (Slice) + patternni avtomatik yozish, Pattern → MIDI
// =============================================================================
#pragma once
#include <complex>
#include "SampleSync.h"
#include "Theme.h"

namespace az
{
namespace beat
{
constexpr int numPads = 20, maxSteps = 64, firstNote = 36, numBuses = 4, numLanes = 7;
// Nerve kabi step-lane'lar (velocity alohida — "steps"): har step uchun 0..127
inline const char* laneIds[]   = { "l_cut", "l_res", "l_pitch", "l_pan", "l_gate", "l_late", "l_fx" };
inline const char* laneNames[] = { "cutoff", "resonance", "pitch", "pan", "gate", "late", "fx send" };
inline const int   laneDef[]   = { 127, 0, 64, 64, 127, 0, 0 };
enum Lane { laneCut = 0, laneRes, lanePitch, lanePan, laneGate, laneLate, laneFx };
inline const juce::Identifier PAD ("PAD");
inline const char* rateNames[] = { "1/16", "1/8", "1/32", "1/16T" };
inline double rateBeats (int r) { return r == 1 ? 0.5 : r == 2 ? 0.125 : r == 3 ? 0.25 * 2.0 / 3.0 : 0.25; }

struct PadRT
{
    std::shared_ptr<const juce::AudioBuffer<float>> sample; double sampleRate = 48000;
    float gain = 1, pan = 0, tune = 0, start = 0, end = 1, attackMs = 0, decayMs = 30, delayMs = 0;
    float holdMs = 0, envDecMs = 0, sustain = 1;   // Battery amp envelope: Attack - Hold - Decay - Sustain (Release = decayMs, bo'lak oxirida)
    bool reverse = false, mute = false, solo = false; int choke = 0;
    // Battery: pitch envelope va filtr/EQ, chiqish shinasi
    float peAmt = 0, peD1 = 60, peBrk = 0.3f, peD2 = 300;
    int fType = 0, fSlope = 1; float fCut = 20000, fRes = 0.1f, fGain = 0;   // fType: 0 o'chiq, 1 LP, 2 HP, 3 BP; fSlope: 0=1P,1=2P,2=4P
    int bus = 0;                                                                // 0 = Master, 1..4 = Bus
    juce::uint8 steps[maxSteps] {}, ratchet[maxSteps] {};
    juce::uint8 lane[numLanes][maxSteps] {};
};
struct Snapshot
{
    PadRT pads[numPads];
    bool seqOn = true, anySolo = false; int length = 16, rate = 0; float swing = 0, master = 1;
    float busGain[numBuses] { 1, 1, 1, 1 }, fxReturn = 0.35f;
};

// ---------------------------------------------------------------- standart kit (sintez qilingan, birinchi ishga tushishda)
inline juce::File defaultKitFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("AZ MUS").getChildFile ("AZ BEAT Kit");
}
inline juce::Array<juce::File> ensureDefaultKit()
{
    auto dir = defaultKitFolder(); dir.createDirectory();
    const char* names[] = { "Kick", "Snare", "Clap", "Closed Hat", "Open Hat", "Rim", "Tom Low", "Tom High",
                            "808", "Shaker", "Crash", "Perc", "Snap", "Kick 2", "Snare 2", "Ride",
                            "Conga", "Cowbell", "Tambourine", "FX Zap" };
    juce::Array<juce::File> files;
    juce::Random rnd (7);
    for (int p = 0; p < numPads; ++p)
    {
        auto f = dir.getChildFile (juce::String (p + 1).paddedLeft ('0', 2) + " " + names[p] + ".wav");
        files.add (f);
        if (f.existsAsFile()) continue;
        const int sr = 48000; const double dur = p == 4 || p == 10 || p == 15 ? 1.1 : p == 8 ? 1.4 : p == 19 ? 0.9 : 0.6;
        const int n = (int) (dur * sr);
        juce::AudioBuffer<float> b (2, n); b.clear();
        double ph = 0; float lp = 0, hp = 0, prev = 0;
        for (int i = 0; i < n; ++i)
        {
            const double t = (double) i / sr; float v = 0;
            const float noise = rnd.nextFloat() * 2 - 1;
            hp = 0.92f * (hp + noise - prev); prev = noise;          // yuqori chastota shovqini
            lp += 0.25f * (noise - lp);
            switch (p)
            {
                case 0: case 13: { const double f = (p == 0 ? 48 : 56) + 140 * std::exp (-t * 38); ph += f / sr; v = (float) (std::sin (juce::MathConstants<double>::twoPi * ph) * std::exp (-t * (p == 0 ? 7 : 11))) + 0.25f * noise * (float) std::exp (-t * 300); break; }
                case 1: case 14: { ph += 190.0 / sr; v = 0.5f * (float) (std::sin (juce::MathConstants<double>::twoPi * ph) * std::exp (-t * 30)) + 0.6f * hp * (float) std::exp (-t * (p == 1 ? 16 : 24)); break; }
                case 2:  { const double e = std::exp (-std::fmod (t, 0.012) * 260) * (t < 0.036 ? 1 : 0) + std::exp (-(t - 0.036) * 18) * (t >= 0.036 ? 1 : 0); v = 0.7f * hp * (float) e; break; }
                case 3:  v = 0.55f * hp * (float) std::exp (-t * 60); break;
                case 4:  v = 0.45f * hp * (float) std::exp (-t * 5); break;
                case 5:  { ph += 1700.0 / sr; v = 0.6f * (float) (std::sin (juce::MathConstants<double>::twoPi * ph) * std::exp (-t * 120)) + 0.3f * hp * (float) std::exp (-t * 140); break; }
                case 6: case 7: { const double f = (p == 6 ? 95 : 150) * (1 + 0.5 * std::exp (-t * 25)); ph += f / sr; v = 0.8f * (float) (std::sin (juce::MathConstants<double>::twoPi * ph) * std::exp (-t * 9)); break; }
                case 8:  { const double f = 49 * (1 + 0.6 * std::exp (-t * 30)); ph += f / sr; v = (float) std::tanh (1.6 * std::sin (juce::MathConstants<double>::twoPi * ph)) * (float) std::exp (-t * 2.2) * 0.8f; break; }   // G1 808
                case 9:  v = 0.4f * hp * (float) (std::sin (juce::MathConstants<double>::pi * juce::jmin (1.0, t / 0.08)) * std::exp (-t * 25)); break;
                case 10: v = 0.4f * (hp + 0.3f * noise) * (float) std::exp (-t * 3.2); break;
                case 11: { ph += 520.0 / sr; v = 0.6f * (float) (std::sin (juce::MathConstants<double>::twoPi * ph + 2 * std::sin (juce::MathConstants<double>::twoPi * 760 * t)) * std::exp (-t * 40)); break; }
                case 12: v = 0.8f * hp * (float) std::exp (-t * 90); break;
                case 15: { ph += 3200.0 / sr; v = 0.2f * (float) std::sin (juce::MathConstants<double>::twoPi * ph) * (float) std::exp (-t * 4) + 0.25f * hp * (float) std::exp (-t * 6); break; }
                case 16: { const double f = 210 * (1 + 0.25 * std::exp (-t * 40)); ph += f / sr; v = 0.75f * (float) (std::sin (juce::MathConstants<double>::twoPi * ph) * std::exp (-t * 14)); break; }   // conga
                case 17: { const double a1 = std::fmod (t * 540, 1.0) < 0.5 ? 1 : -1, a2 = std::fmod (t * 800, 1.0) < 0.5 ? 1 : -1; v = 0.22f * (float) ((a1 + a2) * std::exp (-t * 12)); break; }   // cowbell
                case 18: v = 0.5f * hp * (float) (std::exp (-std::fmod (t, 0.045) * 70) * std::exp (-t * 7)); break;   // tambourine
                case 19: { const double f = 2400 * std::exp (-t * 7) + 60; ph += f / sr; v = 0.5f * (float) (std::sin (juce::MathConstants<double>::twoPi * ph) * std::exp (-t * 4)); break; }   // zap

                default: break;
            }
            v = juce::jlimit (-1.0f, 1.0f, v * 0.9f);
            b.setSample (0, i, v); b.setSample (1, i, v);
        }
        juce::WavAudioFormat wav;
        if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 24, {}, 0)))
            w->writeFromAudioSampleBuffer (b, 0, n);
    }
    return files;
}

// ---------------------------------------------------------------- transient bo'yicha bo'lish (Slice)
inline juce::Array<double> detectOnsets (const juce::AudioBuffer<float>& b, double sr, int maxSlices)
{
    const int hop = (int) (sr * 0.005), win = hop * 2;
    std::vector<double> env;
    for (int s = 0; s + win < b.getNumSamples(); s += hop)
    {
        double sum = 0; for (int c = 0; c < b.getNumChannels(); ++c) { auto* d = b.getReadPointer (c, s); for (int i = 0; i < win; ++i) sum += d[i] * d[i]; }
        env.push_back (10 * std::log10 (juce::jmax (1e-12, sum / (win * b.getNumChannels()))));
    }
    juce::Array<double> on; on.add (0.0);
    const int minGap = (int) (0.07 / 0.005);
    int last = 0;
    for (int i = 4; i < (int) env.size(); ++i)
    {
        double before = -200; for (int k = 2; k <= 4; ++k) before = juce::jmax (before, env[(size_t) i - (size_t) k]);
        if (env[(size_t) i] > -45 && env[(size_t) i] - before > 6.0 && i - last > minGap) { on.add (i * 0.005); last = i; }
    }
    // juda ko'p bo'lsa: eng kuchli o'tishlarni qoldiramiz (bir tekis tanlash)
    while (on.size() > maxSlices)
    {
        int best = 1; double bg = 1e9;
        for (int i = 1; i < on.size(); ++i) if (on[i] - on[i - 1] < bg) { bg = on[i] - on[i - 1]; best = i; }
        on.remove (best);
    }
    return on;
}
} // namespace beat

// =============================================================================  AZ BEAT plagini
class AzBeatPlugin : public te::Plugin, private juce::Timer
{
public:
    static const char* getPluginName() { return "AZ BEAT"; }
    static inline const char* xmlTypeName = "azbeat";

    explicit AzBeatPlugin (te::PluginCreationInfo info) : te::Plugin (info)
    {
        for (int i = 0; i < beat::numPads; ++i) { pad (i); padPos[i] = -1.0f; }
        if (! state.hasProperty ("seqOn")) { state.setProperty ("seqOn", true, nullptr); state.setProperty ("length", 16, nullptr); state.setProperty ("rate", 0, nullptr);
                                             state.setProperty ("swing", 0.0, nullptr); state.setProperty ("master", 0.0, nullptr); }
        rebuildAll();
        startTimerHz (12);
    }
    ~AzBeatPlugin() override { stopTimer(); notifyListenersOfDeletion(); }

    juce::String getName() const override                 { return getPluginName(); }
    juce::String getPluginType() override                 { return xmlTypeName; }
    juce::String getSelectableDescription() override      { return getName(); }
    bool takesMidiInput() override                        { return true; }
    bool isSynth() override                               { return true; }
    bool producesAudioWhenNoAudioInput() override         { return true; }
    bool noTail() override                                { return false; }
    double getTailLength() const override                 { return 2.0; }
    int getNumOutputChannelsGivenInputs (int) override    { return 2; }
    BusLayout getBusses() const override                  { return BusLayout::singleStereoInOut(); }
    std::unique_ptr<EditorComponent> createEditor() override;

    void initialise (const te::PluginInitialisationInfo& info) override
    {
        outRate = info.sampleRate > 0 ? info.sampleRate : 48000.0; for (auto& v : voices) v.active = false;
        busBuf.setSize (12, juce::jmax (8192, info.blockSizeSamples * 2), false, false, true);
        reverb.setSampleRate (outRate);
        juce::Reverb::Parameters rp; rp.roomSize = 0.62f; rp.damping = 0.45f; rp.wetLevel = 1.0f; rp.dryLevel = 0.0f; rp.width = 1.0f; reverb.setParameters (rp); reverb.reset();
    }
    void deinitialise() override {}
    void reset() override { for (auto& v : voices) v.active = false; }

    // ---- model (ValueTree; xabar oqimida)
    juce::ValueTree pad (int i)
    {
        for (auto c : state) if (c.hasType (beat::PAD) && (int) c["idx"] == i) return c;
        juce::ValueTree p (beat::PAD);
        p.setProperty ("idx", i, nullptr); p.setProperty ("file", "", nullptr); p.setProperty ("name", "", nullptr);
        for (auto [k, v] : std::initializer_list<std::pair<const char*, double>> { { "gain", 0 }, { "pan", 0 }, { "tune", 0 }, { "fine", 0 }, { "start", 0 }, { "end", 1 }, { "attack", 0 }, { "decay", 30 }, { "delay", 0 },
                                                                                   { "hold", 0 }, { "env_dec", 0 }, { "sustain", 1 } })
            p.setProperty (k, v, nullptr);
        for (auto k : { "reverse", "mute", "solo", "sync", "norm" }) p.setProperty (k, false, nullptr);
        p.setProperty ("choke", 0, nullptr);
        for (auto [k, v] : std::initializer_list<std::pair<const char*, double>> { { "pe_amt", 0 }, { "pe_d1", 60 }, { "pe_brk", 0.3 }, { "pe_d2", 300 },
                                                                                    { "f_type", 0 }, { "f_slope", 1 }, { "f_cut", 20000 }, { "f_res", 0.1 }, { "f_gain", 0 }, { "bus", 0 } })
            p.setProperty (k, v, nullptr);
        p.setProperty ("steps", juce::String::repeatedString ("0,", beat::maxSteps), nullptr);
        p.setProperty ("ratchet", juce::String::repeatedString ("1,", beat::maxSteps), nullptr);
        state.addChild (p, -1, nullptr);
        return p;
    }
    static juce::Array<int> intList (const juce::String& s, int def)
    {
        juce::Array<int> r; for (auto& t : juce::StringArray::fromTokens (s, ",", {})) if (t.isNotEmpty()) r.add (t.getIntValue());
        while (r.size() < beat::maxSteps) r.add (def);
        return r;
    }
    static juce::String listString (const juce::Array<int>& a) { juce::String s; for (int i = 0; i < beat::maxSteps; ++i) s << juce::String (a[i]) << ","; return s; }
    int stepVel (int p, int s) { return intList (pad (p)["steps"], 0)[s]; }
    void setStep (int p, int s, int vel, juce::UndoManager* um = nullptr)
    {
        auto a = intList (pad (p)["steps"], 0); a.set (s, juce::jlimit (0, 127, vel)); pad (p).setProperty ("steps", listString (a), um);
    }
    int stepRatchet (int p, int s) { return juce::jlimit (1, 4, intList (pad (p)["ratchet"], 1)[s]); }
    void setRatchet (int p, int s, int r) { auto a = intList (pad (p)["ratchet"], 1); a.set (s, juce::jlimit (1, 4, r)); pad (p).setProperty ("ratchet", listString (a), nullptr); }
    int laneVal (int p, int lane, int s) { auto v = pad (p)[beat::laneIds[lane]]; return v.isVoid() ? beat::laneDef[lane] : juce::jlimit (0, 127, intList (v, beat::laneDef[lane])[s]); }
    void setLane (int p, int lane, int s, int val, juce::UndoManager* u = nullptr)
    {
        auto pv = pad (p); auto cur = pv[beat::laneIds[lane]];
        auto a = cur.isVoid() ? intList ({}, beat::laneDef[lane]) : intList (cur, beat::laneDef[lane]);
        a.set (s, juce::jlimit (0, 127, val)); pv.setProperty (beat::laneIds[lane], listString (a), u);
    }
    int length() const   { return juce::jlimit (1, beat::maxSteps, (int) state["length"]); }
    juce::UndoManager* um() { return getUndoManager(); }

    bool loadSample (int p, const juce::File& f, double startN = 0, double endN = 1)
    {
        if (! f.existsAsFile()) return false;
        auto v = pad (p);
        v.setProperty ("file", f.getFullPathName(), um()); v.setProperty ("name", f.getFileNameWithoutExtension(), um());
        v.setProperty ("start", startN, um()); v.setProperty ("end", endN, um());
        cache.erase (f.getFullPathName());
        rebuildAll();
        return true;
    }
    void clearPad (int p) { auto v = pad (p); v.setProperty ("file", "", um()); v.setProperty ("name", "", um()); }
    void loadDefaultKit (bool withPattern)
    {
        auto files = beat::ensureDefaultKit();
        for (int i = 0; i < beat::numPads; ++i) loadSample (i, files[i]);
        pad (3).setProperty ("choke", 1, nullptr); pad (4).setProperty ("choke", 1, nullptr);   // yopiq/ochiq hat
        if (withPattern)
        {
            const int kick[] = { 0, 6, 10 }, snare[] = { 4, 12 };
            for (int s : kick) setStep (0, s, 115);
            for (int s : snare) setStep (2, s, 110);
            for (int s = 0; s < 16; s += 2) setStep (3, s, s % 4 == 0 ? 100 : 72);
            setStep (4, 14, 90); setStep (3, 15, 60); setRatchet (3, 15, 3);
        }
        state.setProperty ("kit", "AZ Default", nullptr);
        rebuildAll();
    }

    // ---- Slice: loopni transientlar bo'yicha padlarga bo'lib, patternni yozadi
    juce::String sliceLoop (const juce::File& f, double projBpm)
    {
        juce::AudioBuffer<float> b; auto fi = sync::analyse (edit.engine, f, projBpm, &b);
        if (b.getNumSamples() == 0) return juce::String::fromUTF8 ("Faylni o'qib bo'lmadi");
        const double bpm = fi.bpm > 0 ? fi.bpm : projBpm;
        auto on = beat::detectOnsets (b, fi.sampleRate, beat::numPads);
        const double len = (double) b.getNumSamples() / fi.sampleRate;
        const double stepSec = 60.0 / bpm / 4.0;
        const int steps = juce::jlimit (16, beat::maxSteps, (int) std::ceil (len / stepSec / 16.0 - 1e-6) * 16);
        for (int p = 0; p < beat::numPads; ++p) { pad (p).setProperty ("steps", juce::String::repeatedString ("0,", beat::maxSteps), nullptr); clearPad (p); }
        for (int i = 0; i < on.size(); ++i)
        {
            const double a = on[i], z = i + 1 < on.size() ? on[i + 1] : len;
            loadSample (i, f, a / len, z / len);
            pad (i).setProperty ("name", f.getFileNameWithoutExtension().substring (0, 10) + " #" + juce::String (i + 1), nullptr);
            const int st = (int) std::round (a / stepSec);
            if (st < beat::maxSteps) setStep (i, st, 110);
        }
        state.setProperty ("length", steps, nullptr); state.setProperty ("rate", 0, nullptr);
        state.setProperty ("kit", f.getFileNameWithoutExtension() + " (slice)", nullptr);
        rebuildAll();
        return juce::String (on.size()) + juce::String::fromUTF8 (" bo'lak \xc2\xb7 ") + juce::String (bpm, 1) + " BPM" + (fi.bpm > 0 ? "" : juce::String::fromUTF8 (" (loyiha tempi)"));
    }

    // ---- UI'dan pad tinglash (lock-free navbat)
    void previewPad (int p, int vel = 110)
    {
        int s1, n1, s2, n2; uiFifo.prepareToWrite (1, s1, n1, s2, n2);
        if (n1 > 0) { uiQueue[(size_t) s1] = p * 1000 + vel; uiFifo.finishedWrite (1); }
    }
    std::atomic<int> playStep { -1 };
    std::atomic<juce::uint32> padHit[beat::numPads] {};

    // ---- audio
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return;
        auto& out = *fc.destBuffer;
        const int s0 = fc.bufferStartSample, n = fc.bufferNumSamples;
        for (int c = 0; c < out.getNumChannels(); ++c) out.clear (c, s0, n);
        std::shared_ptr<const beat::Snapshot> sn;
        { const juce::SpinLock::ScopedTryLockType l (snapLock); if (l.isLocked()) sn = snap; }
        if (sn == nullptr) return;
        // UI tinglash
        {
            int a1, m1, a2, m2; const int ready = uiFifo.getNumReady(); uiFifo.prepareToRead (ready, a1, m1, a2, m2);
            for (int i = 0; i < m1; ++i) trigger (*sn, uiQueue[(size_t) (a1 + i)] / 1000, uiQueue[(size_t) (a1 + i)] % 1000, 0);
            for (int i = 0; i < m2; ++i) trigger (*sn, uiQueue[(size_t) (a2 + i)] / 1000, uiQueue[(size_t) (a2 + i)] % 1000, 0);
            uiFifo.finishedRead (m1 + m2);
        }
        // MIDI kirish (C1 = pad 1)
        if (fc.bufferForMidiMessages != nullptr)
        {
            if (fc.bufferForMidiMessages->isAllNotesOff) for (auto& v : voices) v.releasing = true;
            for (auto& m : *fc.bufferForMidiMessages)
                if (m.isNoteOn())
                {
                    const int p = m.getNoteNumber() - beat::firstNote;
                    if (p >= 0 && p < beat::numPads)
                        trigger (*sn, p, m.getVelocity(), juce::jlimit (0, n - 1, juce::roundToInt ((m.getTimeStamp() + fc.midiBufferOffset) * outRate))
                                                          + juce::jmax (0, (int) (sn->pads[p].delayMs * 0.001 * outRate)));
                }
        }
        // ichki sekvenser (Nerve)
        if (sn->seqOn && fc.isPlaying)
        {
            const double bpm = bpmCache.load();
            const double b0 = fc.editTime.getStart().inSeconds() * bpm / 60.0, b1 = fc.editTime.getEnd().inSeconds() * bpm / 60.0;
            const double rb = beat::rateBeats (sn->rate);
            const int L = sn->length;
            const double maxShift = 0.25 * bpm / 60.0 + beat::rateBeats (sn->rate);   // delay +-250 ms + late
            if (b1 > b0)
            {
                for (long k = (long) std::floor ((b0 - maxShift) / rb) - 1; ; ++k)
                {
                    const double base = k * rb;
                    if (base >= b1 + maxShift) break;
                    if (base < 0) continue;
                    const int idx = (int) (((k % L) + L) % L);
                    const double swing = (idx % 2 == 1) ? sn->swing * rb * 0.5 : 0.0;
                    for (int p = 0; p < beat::numPads; ++p)
                    {
                        const int vel = sn->pads[p].steps[idx];
                        if (vel == 0) continue;
                        const int rat = juce::jmax (1, (int) sn->pads[p].ratchet[idx]);
                        for (int r = 0; r < rat; ++r)
                        {
                            const double late = sn->pads[p].lane[beat::laneLate][idx] / 127.0 * rb * 0.5;   // Nerve "late": step'ning yarmigacha
                            const double at = base + swing + late + r * rb / rat + sn->pads[p].delayMs * 0.001 * bpm / 60.0;
                            if (at < b0 || at >= b1) continue;
                            trigger (*sn, p, juce::jmax (1, vel - r * 12), juce::jlimit (0, n - 1, (int) ((at - b0) * 60.0 / bpm * outRate)), idx, rb * 60.0 / bpm);
                        }
                    }
                    if (base >= b0 && base < b1) playStep = idx;
                }
            }
        }
        else if (! fc.isPlaying) playStep = -1;
        render (*sn, out, s0, n);
    }

    void restorePluginStateFromValueTree (const juce::ValueTree& v) override
    {
        state.copyPropertiesFrom (v, nullptr);
        for (int i = state.getNumChildren(); --i >= 0;) if (state.getChild (i).hasType (beat::PAD)) state.removeChild (i, nullptr);
        for (auto c : v) if (c.hasType (beat::PAD)) state.addChild (c.createCopy(), -1, nullptr);
        for (int i = 0; i < beat::numPads; ++i) pad (i);
        rebuildAll();
    }

    // kit saqlash / yuklash (.azkit)
    bool saveKit (const juce::File& f) { if (auto x = state.createXml()) return x->writeTo (f); return false; }
    bool loadKit (const juce::File& f)
    {
        auto x = juce::XmlDocument::parse (f); if (! x) return false;
        auto v = juce::ValueTree::fromXml (*x); if (! v.isValid()) return false;
        restorePluginStateFromValueTree (v); state.setProperty ("kit", f.getFileNameWithoutExtension(), nullptr);
        return true;
    }
    void rebuildForTest() { rebuildAll(); }
    double sampleSeconds (int p)
    {
        auto it = cache.find (pad (p)["file"].toString());
        return it != cache.end() && it->second.buf ? it->second.buf->getNumSamples() / it->second.sr : 0.0;
    }
    std::shared_ptr<const juce::AudioBuffer<float>> sampleOf (int p, double& sr)
    {
        const juce::String file = pad (p)["file"];
        if (file.isEmpty()) return {};
        if (cache.find (file) == cache.end() || cache[file].buf == nullptr) rebuildAll();
        auto it = cache.find (file);
        if (it == cache.end()) return {};
        sr = it->second.sr; return it->second.buf;
    }

private:
    struct Cached { std::shared_ptr<const juce::AudioBuffer<float>> buf, synced; double sr = 48000, syncedBpm = 0; };
    std::map<juce::String, Cached> cache;

    void valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& id) override
    {
        te::Plugin::valueTreePropertyChanged (v, id);
        if (v == state || v.hasType (beat::PAD)) dirty = true;
    }
    void valueTreeChildAdded (juce::ValueTree& a, juce::ValueTree& b) override { te::Plugin::valueTreeChildAdded (a, b); dirty = true; }
    void valueTreeChildRemoved (juce::ValueTree& a, juce::ValueTree& b, int i) override { te::Plugin::valueTreeChildRemoved (a, b, i); dirty = true; }
    void timerCallback() override
    {
        const double b = edit.tempoSequence.getTempo (0)->getBpm();
        if (std::abs (b - bpmCache.load()) > 1e-6) { bpmCache = b; dirty = true; }
        if (dirty) rebuildAll();
    }
    void rebuildAll()
    {
        dirty = false;
        bpmCache = edit.tempoSequence.getTempo (0)->getBpm();
        auto s = std::make_shared<beat::Snapshot>();
        s->seqOn = state["seqOn"]; s->length = length(); s->rate = juce::jlimit (0, 3, (int) state["rate"]);
        s->swing = juce::jlimit (0.0f, 0.75f, (float) (double) state["swing"]);
        s->master = juce::Decibels::decibelsToGain ((float) (double) state["master"]);
        for (int b = 0; b < beat::numBuses; ++b) s->busGain[b] = juce::Decibels::decibelsToGain ((float) (double) state.getProperty ("bus" + juce::String (b + 1), 0.0), -80.0f);
        s->fxReturn = juce::Decibels::decibelsToGain ((float) (double) state.getProperty ("fxReturn", -6.0), -80.0f);
        for (int i = 0; i < beat::numPads; ++i)
        {
            auto v = pad (i); auto& p = s->pads[i];
            const juce::String file = v["file"];
            if (file.isNotEmpty())
            {
                auto& c = cache[file];
                if (c.buf == nullptr)
                {
                    auto b = std::make_shared<juce::AudioBuffer<float>>();
                    std::unique_ptr<juce::AudioFormatReader> r (edit.engine.getAudioFileFormatManager().readFormatManager.createReaderFor (juce::File (file)));
                    if (r) { b->setSize (2, (int) std::min<juce::int64> (r->lengthInSamples, (juce::int64) (r->sampleRate * 60))); r->read (b.get(), 0, b->getNumSamples(), 0, true, true); c.sr = r->sampleRate; }
                    c.buf = b;
                }
                p.sample = c.buf; p.sampleRate = c.sr;
                if ((bool) v["sync"] && c.buf && c.buf->getNumSamples() > 0)
                {
                    // loopni loyiha tempiga cho'zish (Nerve "sync")
                    const double fileBpm = [&] { double bb = sync::bpmFromName (juce::File (file).getFileNameWithoutExtension());
                                                 return bb > 0 ? bb : sync::guessBpmFromLength (c.buf->getNumSamples() / c.sr, bpmCache.load()); }();
                    if (fileBpm > 0)
                    {
                        if (c.synced == nullptr || std::abs (c.syncedBpm - bpmCache.load()) > 1e-3)
                        { c.synced = std::make_shared<juce::AudioBuffer<float>> (sync::stretch (*c.buf, c.sr, fileBpm / bpmCache.load(), 0)); c.syncedBpm = bpmCache.load(); }
                        p.sample = c.synced;
                    }
                }
            }
            p.gain = juce::Decibels::decibelsToGain ((float) (double) v["gain"]); p.pan = (float) (double) v["pan"];
            p.delayMs = (float) juce::jlimit (-250.0, 250.0, (double) v["delay"]);
            if ((bool) v["norm"] && p.sample && p.sample->getNumSamples() > 0)
            {   // Normalize: tanlangan bo'lak cho'qqisi -1 dBFS
                const int N = p.sample->getNumSamples();
                const int a = juce::jlimit (0, N - 1, (int) ((double) v["start"] * N)), b = juce::jlimit (a + 1, N, (int) ((double) v["end"] * N));
                float pk = 0; for (int c = 0; c < p.sample->getNumChannels(); ++c) pk = juce::jmax (pk, p.sample->getMagnitude (c, a, b - a));
                if (pk > 1e-5f) p.gain *= juce::jmin (63.0f, 0.891f / pk);
            }
            p.tune = (float) ((double) v["tune"] + (double) v["fine"] / 100.0);
            p.start = (float) (double) v["start"]; p.end = (float) (double) v["end"];
            p.attackMs = (float) (double) v["attack"]; p.decayMs = (float) (double) v["decay"];
            p.holdMs = (float) (double) v.getProperty ("hold", 0.0); p.envDecMs = (float) (double) v.getProperty ("env_dec", 0.0);
            p.sustain = (float) juce::jlimit (0.0, 1.0, (double) v.getProperty ("sustain", 1.0));
            p.reverse = v["reverse"]; p.mute = v["mute"]; p.solo = v["solo"]; p.choke = v["choke"];
            s->anySolo |= p.solo;
            auto st = intList (v["steps"], 0), ra = intList (v["ratchet"], 1);
            for (int k = 0; k < beat::maxSteps; ++k) { p.steps[k] = (juce::uint8) juce::jlimit (0, 127, st[k]); p.ratchet[k] = (juce::uint8) juce::jlimit (1, 4, ra[k]); }
            for (int l = 0; l < beat::numLanes; ++l)
            {
                auto lv = v[beat::laneIds[l]];
                auto arr = intList (lv.isVoid() ? juce::String() : lv.toString(), beat::laneDef[l]);
                for (int k = 0; k < beat::maxSteps; ++k) p.lane[l][k] = (juce::uint8) juce::jlimit (0, 127, arr[k]);
            }
            auto num = [&] (const char* id, double def) { auto x = v[id]; return x.isVoid() ? def : (double) x; };
            p.peAmt = (float) num ("pe_amt", 0); p.peD1 = (float) num ("pe_d1", 60); p.peBrk = (float) num ("pe_brk", 0.3); p.peD2 = (float) num ("pe_d2", 300);
            p.fType = (int) num ("f_type", 0); p.fSlope = (int) num ("f_slope", 1); p.fCut = (float) num ("f_cut", 20000); p.fRes = (float) num ("f_res", 0.1); p.fGain = (float) num ("f_gain", 0);
            p.bus = juce::jlimit (0, beat::numBuses, (int) num ("bus", 0));
        }
        const juce::SpinLock::ScopedLockType l (snapLock);
        snap = s;
    }

    struct SVF   // TPT state-variable filter (Cytomic)
    {
        float ic1[2] {}, ic2[2] {}, g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0;
        void set (double fc, double q, double sr)
        {
            g = (float) std::tan (juce::MathConstants<double>::pi * juce::jlimit (10.0, sr * 0.49, fc) / sr); k = (float) (1.0 / q);
            a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
        }
        float process (float x, int ch, int type)
        {
            const float v3 = x - ic2[ch], v1 = a1 * ic1[ch] + a2 * v3, v2 = ic2[ch] + a2 * ic1[ch] + a3 * v3;
            ic1[ch] = 2 * v1 - ic1[ch]; ic2[ch] = 2 * v2 - ic2[ch];
            return type == 1 ? v2 : type == 2 ? x - k * v1 - v2 : v1;   // LP / HP / BP
        }
        void reset() { ic1[0] = ic1[1] = ic2[0] = ic2[1] = 0; }
    };
    struct OnePole { float z[2] {}; float a = 0; void set (double fc, double sr) { a = (float) std::exp (-juce::MathConstants<double>::twoPi * juce::jlimit (10.0, sr * 0.49, fc) / sr); }
                     float lp (float x, int ch) { z[ch] = x * (1 - a) + z[ch] * a; return z[ch]; } };
    struct Voice
    {
        bool active = false, releasing = false;
        int pad = 0, delay = 0; double pos = 0, inc = 1, a = 0, b = 0; float gl = 1, gr = 1, env = 0, atkInc = 1, relInc = 0.01f;
        int decayLen = 0; bool reverse = false;
        double ahdA = 0, ahdH = 0, ahdD = 0; float sus = 1;   // amp envelope (namunalar, chiqish tezligida)
        // pitch envelope
        float peAmt = 0, peLevel = 1, peK1 = 0, peK2 = 0, peBrk = 0.3f; bool peStage2 = false;
        // filtr
        int fType = 0, fStages = 1; bool onePole = false; SVF svf[2]; OnePole op; float fGain = 1;
        // gate, fx, shina
        double gateLen = 1e18, played = 0; float fxSend = 0; int bus = 0;
        juce::uint32 startedAt = 0;
        std::shared_ptr<const juce::AudioBuffer<float>> buf;
    };
    // stepIdx >= 0 — sekvenserdan (lane qiymatlari qo'llanadi), stepSec — step davomiyligi
    void trigger (const beat::Snapshot& s, int p, int vel, int offset, int stepIdx = -1, double stepSec = 0.125)
    {
        if (p < 0 || p >= beat::numPads) return;
        const auto& pd = s.pads[p];
        if (pd.sample == nullptr || pd.sample->getNumSamples() < 2 || pd.mute || (s.anySolo && ! pd.solo)) return;
        if (pd.choke > 0) for (auto& v : voices) if (v.active && s.pads[v.pad].choke == pd.choke) { v.releasing = true; v.relInc = 1.0f / (0.004f * (float) outRate); }
        Voice* v = nullptr;
        for (auto& x : voices) if (! x.active) { v = &x; break; }
        if (v == nullptr) { v = &voices[(size_t) (nextSteal++ % voices.size())]; }
        auto lane = [&] (int l) { return stepIdx >= 0 ? (int) pd.lane[l][stepIdx] : beat::laneDef[l]; };
        const int N = pd.sample->getNumSamples();
        v->a = juce::jlimit (0.0, (double) N - 2, (double) pd.start * N);
        v->b = juce::jlimit (v->a + 1, (double) N - 1, (double) pd.end * N);
        v->reverse = pd.reverse; v->pos = v->reverse ? v->b : v->a;
        const float lanePitch = (lane (beat::lanePitch) - 64) / 64.0f * 12.0f;
        v->inc = pd.sampleRate / outRate * std::pow (2.0, (pd.tune + lanePitch) / 12.0);
        const float g = pd.gain * std::pow (juce::jlimit (1, 127, vel) / 127.0f, 1.2f);
        const float pan = juce::jlimit (-1.0f, 1.0f, pd.pan + (lane (beat::lanePan) - 64) / 64.0f);
        v->gl = g * std::cos ((pan + 1) * juce::MathConstants<float>::pi * 0.25f) * 1.4142f;
        v->gr = g * std::sin ((pan + 1) * juce::MathConstants<float>::pi * 0.25f) * 1.4142f;
        v->atkInc = pd.attackMs > 0.1f ? 1.0f / (pd.attackMs * 0.001f * (float) outRate) : 1.0f;
        v->env = pd.attackMs > 0.1f ? 0.0f : 1.0f;
        v->decayLen = (int) (pd.decayMs * 0.001 * outRate * v->inc);
        v->ahdA = pd.attackMs * 0.001 * outRate; v->ahdH = pd.holdMs * 0.001 * outRate; v->ahdD = pd.envDecMs * 0.001 * outRate; v->sus = pd.sustain;
        // pitch envelope (Battery): amount -> break darajasigacha decay1, keyin decay2 bilan 0 ga
        v->peAmt = pd.peAmt; v->peLevel = 1.0f; v->peStage2 = false; v->peBrk = juce::jlimit (0.0f, 1.0f, pd.peBrk);
        v->peK1 = std::exp (-1.0f / juce::jmax (1.0f, pd.peD1 * 0.001f * (float) outRate));
        v->peK2 = std::exp (-1.0f / juce::jmax (1.0f, pd.peD2 * 0.001f * (float) outRate));
        // filtr: pad sozlamasi + step lane'lari (cutoff, resonance)
        const int lc = lane (beat::laneCut), lr = lane (beat::laneRes);
        v->fType = pd.fType == 0 && lc < 127 ? 1 : pd.fType;
        double fc = (pd.fType == 0 ? 20000.0 : pd.fCut) * std::pow (2.0, (lc - 127) / 127.0 * 8.0);
        const double res = juce::jlimit (0.0, 1.0, (double) pd.fRes + lr / 127.0);
        const double q = 0.5 + res * res * 11.5;
        v->fStages = pd.fSlope == 2 ? 2 : 1; v->onePole = pd.fSlope == 0;
        for (auto& f : v->svf) { f.set (fc, pd.fSlope == 0 ? 0.707 : q, outRate); f.reset(); }
        v->op.set (fc, outRate); v->op.z[0] = v->op.z[1] = 0;
        v->fGain = juce::Decibels::decibelsToGain (pd.fGain);
        // gate (step uzunligiga nisbatan emas — sample bo'lagi uzunligiga nisbatan), fx send, shina
        const int gl = lane (beat::laneGate);
        v->gateLen = gl >= 127 ? 1e18 : juce::jmax (0.01, gl / 127.0) * (v->b - v->a) / juce::jmax (1e-9, v->inc);
        v->played = 0;
        v->fxSend = lane (beat::laneFx) / 127.0f;
        v->bus = pd.bus;
        v->releasing = false; v->relInc = 0.0f; v->pad = p; v->delay = offset; v->buf = pd.sample; v->active = true;
        v->startedAt = juce::Time::getMillisecondCounter();
        padHit[p] = juce::Time::getMillisecondCounter();
        juce::ignoreUnused (stepSec);
    }
    void render (const beat::Snapshot& sn, juce::AudioBuffer<float>& out, int s0, int n)
    {
        if (busBuf.getNumSamples() < n) return;
        busBuf.clear (0, n);
        float lastPos[beat::numPads]; for (auto& x : lastPos) x = -1;
        float padPk[beat::numPads] {};
        juce::uint32 newest[beat::numPads] {};
        for (auto& v : voices)
        {
            if (! v.active) continue;
            const auto& b = *v.buf;
            const float* c0 = b.getReadPointer (0); const float* c1 = b.getReadPointer (juce::jmin (1, b.getNumChannels() - 1));
            const int bi = juce::jlimit (0, beat::numBuses, v.bus);
            auto* L = busBuf.getWritePointer (bi * 2), *R = busBuf.getWritePointer (bi * 2 + 1);
            auto* FL = busBuf.getWritePointer (10), *FR = busBuf.getWritePointer (11);
            float vpk = 0;
            for (int i = juce::jmax (0, v.delay); i < n; ++i)
            {
                const int ip = (int) v.pos; const float fr = (float) (v.pos - ip);
                if (ip < 0 || ip + 1 >= b.getNumSamples()) { v.active = false; break; }
                float xl = c0[ip] + (c0[ip + 1] - c0[ip]) * fr, xr = c1[ip] + (c1[ip + 1] - c1[ip]) * fr;
                if (v.fType > 0)
                {
                    if (v.onePole && v.fType != 3)
                    {   // 1P: 6 dB/okt
                        const float ll = v.op.lp (xl, 0), lr = v.op.lp (xr, 1);
                        xl = v.fType == 1 ? ll : xl - ll; xr = v.fType == 1 ? lr : xr - lr;
                    }
                    else for (int st = 0; st < v.fStages; ++st) { xl = v.svf[st].process (xl, 0, v.fType); xr = v.svf[st].process (xr, 1, v.fType); }
                    xl *= v.fGain; xr *= v.fGain;
                }
                const double remain = v.reverse ? v.pos - v.a : v.b - v.pos;
                float fade = 1.0f;
                if (v.decayLen > 0 && remain < v.decayLen) fade = (float) (remain / v.decayLen);
                if (v.played > v.gateLen && ! v.releasing) { v.releasing = true; v.relInc = 1.0f / (0.006f * (float) outRate); }
                if (v.env < 1.0f && ! v.releasing) v.env = juce::jmin (1.0f, v.env + v.atkInc);
                if (v.releasing) { v.env -= v.relInc > 0 ? v.relInc : 0.01f; if (v.env <= 0) { v.active = false; break; } }
                float ahd = 1.0f;
                if (v.sus < 0.999f)
                {
                    const double t = v.played - v.ahdA - v.ahdH;
                    if (t > 0) ahd = v.ahdD > 1 && t < v.ahdD ? 1.0f - (1.0f - v.sus) * (float) (t / v.ahdD) : v.sus;
                }
                const float g = v.env * fade * ahd;
                const float ol = xl * v.gl * g, orr = xr * v.gr * g;
                L[i] += ol; R[i] += orr;
                vpk = juce::jmax (vpk, std::abs (ol), std::abs (orr));
                if (v.fxSend > 0) { FL[i] += ol * v.fxSend; FR[i] += orr * v.fxSend; }
                // pitch envelope
                double inc = v.inc;
                if (std::abs (v.peAmt) > 0.01f)
                {
                    if (! v.peStage2) { v.peLevel *= v.peK1; if (v.peLevel <= v.peBrk) v.peStage2 = true; } else v.peLevel *= v.peK2;
                    inc *= std::pow (2.0, v.peAmt * v.peLevel / 12.0);
                }
                v.pos += v.reverse ? -inc : inc; v.played += 1;
                if (v.reverse ? v.pos <= v.a : v.pos >= v.b) { v.active = false; break; }
            }
            padPk[v.pad] = juce::jmax (padPk[v.pad], vpk);
            v.delay = juce::jmax (0, v.delay - n);
            if (v.active && v.delay == 0 && v.startedAt >= newest[v.pad])
            { newest[v.pad] = v.startedAt; lastPos[v.pad] = (float) (v.pos / juce::jmax (1, b.getNumSamples())); }
        }
        for (int p = 0; p < beat::numPads; ++p) { padPos[p] = lastPos[p]; padPeak[p] = juce::jmax (padPk[p], padPeak[p].load() * 0.92f); }
        // FX (reverb) qaytishi
        reverb.processStereo (busBuf.getWritePointer (10), busBuf.getWritePointer (11), n);
        // shinalar -> master
        auto* L = out.getWritePointer (0, s0);
        auto* R = out.getNumChannels() > 1 ? out.getWritePointer (1, s0) : nullptr;
        for (int bi = 0; bi <= beat::numBuses + 1; ++bi)
        {
            const float gb = bi == 0 ? 1.0f : bi <= beat::numBuses ? sn.busGain[bi - 1] : sn.fxReturn;
            const int ch = bi <= beat::numBuses ? bi * 2 : 10;
            const float* a = busBuf.getReadPointer (ch), *b2 = busBuf.getReadPointer (ch + 1);
            float pk = 0;
            for (int i = 0; i < n; ++i)
            {
                const float l = a[i] * gb * sn.master, r = b2[i] * gb * sn.master;
                L[i] += l; if (R) R[i] += r;
                pk = juce::jmax (pk, std::abs (l), std::abs (r));
            }
            busPeak[bi] = juce::jmax (pk, busPeak[bi].load() * 0.92f);
        }
    }
public:
    std::atomic<float> padPeak[beat::numPads] {};         // har pad chiqishi (mikser metrlari)
    std::atomic<float> padPos[beat::numPads] {};          // pad'ning hozir chalinayotgan joyi (0..1), -1 = jim
    std::atomic<float> busPeak[beat::numBuses + 2] {};    // 0 = master (to'g'ridan), 1..4 = Bus, 5 = FX
private:
    juce::AudioBuffer<float> busBuf { 12, 8192 };
    juce::Reverb reverb;
    std::array<Voice, 48> voices; unsigned nextSteal = 0;
    std::shared_ptr<const beat::Snapshot> snap; juce::SpinLock snapLock;
    std::atomic<double> bpmCache { 120.0 };
    double outRate = 48000; bool dirty = false;
    juce::AbstractFifo uiFifo { 64 }; std::array<int, 64> uiQueue {};
};

// =============================================================================  AZ BEAT muharriri
class BeatEditor : public te::Plugin::EditorComponent, private juce::Timer, public juce::FileDragAndDropTarget, public juce::DragAndDropTarget
{
public:
    explicit BeatEditor (AzBeatPlugin& p) : plugin (p)
    {
        for (auto* b : { &loadKitB, &saveKitB, &defaultB, &sliceB, &toMidiB, &seqB, &clearB, &humanB, &loadSmpB, &revB, &syncB, &muteB, &soloB, &normB, &prevB, &nextB })
        { addAndMakeVisible (b); b->getProperties().set ("radius", 5.0f); }
        loadKitB.setButtonText ("Kit..."); saveKitB.setButtonText (U ("Saqlash")); defaultB.setButtonText (U ("AZ kit"));
        sliceB.setButtonText (U ("Slice loop...")); toMidiB.setButtonText (U ("Pattern \xe2\x86\x92 MIDI")); seqB.setButtonText ("SEQ");
        clearB.setButtonText (U ("Tozalash")); humanB.setButtonText ("Humanize"); loadSmpB.setButtonText (U ("Sample..."));
        revB.setButtonText ("REVERSE"); syncB.setButtonText ("SYNC"); muteB.setButtonText ("M"); soloB.setButtonText ("S");
        normB.setButtonText ("NORMALIZE"); normB.setTooltip (U ("Past signalli sample'ni -1 dBFS cho'qqiga ko'tarish"));
        prevB.setButtonText (juce::String::fromUTF8 ("\xe2\x97\x80")); nextB.setButtonText (juce::String::fromUTF8 ("\xe2\x96\xb6"));
        prevB.setTooltip (U ("Papkadagi oldingi sample (\xe2\x86\x90). Shift: oldingi MOS sample"));
        nextB.setTooltip (U ("Papkadagi keyingi sample (\xe2\x86\x92). Shift: keyingi MOS sample (tonallik/BPM)"));
        prevB.onClick = [this] { browse (-1, juce::ModifierKeys::currentModifiers.isShiftDown()); };
        nextB.onClick = [this] { browse (+1, juce::ModifierKeys::currentModifiers.isShiftDown()); };
        normB.onClick = [this] { plugin.pad (sel).setProperty ("norm", normB.getToggleState(), plugin.um()); plugin.previewPad (sel); };
        setWantsKeyboardFocus (true);
        sliceB.setTooltip (U ("Loopni transientlar bo'yicha 20 padga bo'lish va patternni avtomatik yozish (Nerve)"));
        toMidiB.setTooltip (U ("Patternni trekka MIDI klip qilib qo'yish (keyin tahrirlash uchun)"));
        syncB.setTooltip (U ("Loopni loyiha tempiga moslash"));
        for (auto* b : { &seqB, &revB, &syncB, &muteB, &soloB, &normB }) b->setClickingTogglesState (true);
        normB.setColour (juce::TextButton::buttonOnColourId, green.withAlpha (0.4f));
        seqB.setColour (juce::TextButton::buttonOnColourId, green.withAlpha (0.4f));
        addAndMakeVisible (lenBox); for (int l : { 16, 32, 48, 64 }) lenBox.addItem (juce::String (l) + " step", l);
        addAndMakeVisible (rateBox); for (int i = 0; i < 4; ++i) rateBox.addItem (beat::rateNames[i], i + 1);
        addAndMakeVisible (chokeBox); chokeBox.addItem ("Choke: -", 1); for (int i = 1; i <= 4; ++i) chokeBox.addItem ("Choke " + juce::String (i), i + 1);
        auto knob = [this] (juce::Slider& s, double lo, double hi, double step, const juce::String& suf, double mid = 0)
        {
            addAndMakeVisible (s); s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 62, 15); s.setRange (lo, hi, step); s.setTextValueSuffix (suf);
            s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
            if (mid != 0) s.setSkewFactorFromMidPoint (mid);
        };
        knob (gainK, -36, 12, 0.1, " dB"); knob (panK, -1, 1, 0.01, ""); knob (tuneK, -24, 24, 1, " st"); knob (fineK, -100, 100, 1, " ct");
        knob (atkK, 0, 200, 0.5, " ms", 20); knob (decK, 0, 2000, 1, " ms", 200); knob (dlyK, -250, 250, 0.1, " ms");
        dlyK.getProperties().set ("bipolar", true); dlyK.setDoubleClickReturnValue (true, 0);
        dlyK.setTooltip (U ("Delay: sample'ni kechiktirish (+) yoki ertaroq chalish (\xe2\x88\x92), millisekundda")); knob (swingK, 0, 75, 1, "%");
        panK.getProperties().set ("bipolar", true); tuneK.getProperties().set ("bipolar", true); fineK.getProperties().set ("bipolar", true);
        gainK.setDoubleClickReturnValue (true, 0); panK.setDoubleClickReturnValue (true, 0); tuneK.setDoubleClickReturnValue (true, 0); fineK.setDoubleClickReturnValue (true, 0);

        auto bindPad = [this] (juce::Slider& s, const char* id) { s.onValueChange = [this, &s, id] { plugin.pad (sel).setProperty (id, s.getValue(), plugin.um()); repaint(); }; };
        bindPad (gainK, "gain"); bindPad (panK, "pan");
        gainK.onValueChange = [this] { plugin.pad (sel).setProperty ("gain", gainK.getValue(), plugin.um()); padF[sel].setValue (gainK.getValue(), juce::dontSendNotification); repaint(); };
        atkK.textFromValueFunction = decK.textFromValueFunction = [] (double v) { return v >= 1000 ? juce::String (v / 1000.0, 2) + " s" : juce::String (v, v < 10 ? 1 : 0) + " ms"; }; bindPad (tuneK, "tune"); bindPad (fineK, "fine"); bindPad (atkK, "attack"); bindPad (decK, "decay"); bindPad (dlyK, "delay");
        swingK.onValueChange = [this] { plugin.state.setProperty ("swing", swingK.getValue() / 100.0, plugin.um()); };
        revB.onClick  = [this] { plugin.pad (sel).setProperty ("reverse", revB.getToggleState(), plugin.um()); repaint(); };
        syncB.onClick = [this] { plugin.pad (sel).setProperty ("sync", syncB.getToggleState(), plugin.um()); };
        muteB.onClick = [this] { plugin.pad (sel).setProperty ("mute", muteB.getToggleState(), plugin.um()); repaint(); };
        soloB.onClick = [this] { plugin.pad (sel).setProperty ("solo", soloB.getToggleState(), plugin.um()); repaint(); };
        chokeBox.onChange = [this] { plugin.pad (sel).setProperty ("choke", chokeBox.getSelectedId() - 1, plugin.um()); };
        seqB.onClick = [this] { plugin.state.setProperty ("seqOn", seqB.getToggleState(), plugin.um()); };
        lenBox.onChange = [this] { plugin.state.setProperty ("length", lenBox.getSelectedId(), plugin.um()); repaint(); };
        rateBox.onChange = [this] { plugin.state.setProperty ("rate", rateBox.getSelectedId() - 1, plugin.um()); };
        clearB.onClick = [this]
        {
            juce::PopupMenu m; m.addItem (1, U ("Tanlangan pad qatorini tozalash")); m.addItem (2, U ("Butun patternni tozalash"));
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&clearB), [this] (int r)
            {
                for (int p = 0; p < beat::numPads; ++p) if (r == 2 || (r == 1 && p == sel)) plugin.pad (p).setProperty ("steps", juce::String::repeatedString ("0,", beat::maxSteps), plugin.um());
                repaint();
            });
        };
        humanB.onClick = [this]
        {
            juce::Random r;
            for (int p = 0; p < beat::numPads; ++p) for (int s = 0; s < plugin.length(); ++s)
                if (const int v = plugin.stepVel (p, s); v > 0) plugin.setStep (p, s, juce::jlimit (30, 127, v + r.nextInt ({ -14, 15 })), plugin.um());
            repaint();
        };
        loadSmpB.onClick = [this] { chooseFile (U ("Sample tanlang"), "*.wav;*.aif;*.aiff;*.flac;*.mp3", false, [this] (juce::File f) { plugin.loadSample (sel, f); refresh(); }); };
        loadKitB.onClick = [this] { chooseFile (U ("AZ BEAT kit"), "*.azkit", false, [this] (juce::File f) { plugin.loadKit (f); refresh(); }); };
        saveKitB.onClick = [this] { chooseFile (U ("Kitni saqlash"), "*.azkit", true, [this] (juce::File f) { plugin.saveKit (f.withFileExtension ("azkit")); }); };
        defaultB.onClick = [this] { plugin.loadDefaultKit (false); refresh(); };
        sliceB.onClick = [this] { chooseFile (U ("Loop tanlang (Slice)"), "*.wav;*.aif;*.aiff;*.flac;*.mp3", false, [this] (juce::File f) { doSlice (f); }); };
        toMidiB.onClick = [this] { patternToMidi(); };

        // ---- Nerve lane tablari: velocity + 7 lane
        for (int i = 0; i < 8; ++i)
        {
            auto* b = laneTabs.add (new juce::TextButton (i == 0 ? juce::String ("velocity") : juce::String (beat::laneNames[i - 1])));
            addAndMakeVisible (b); b->setClickingTogglesState (true); b->setRadioGroupId (771);
            b->getProperties().set ("radius", 4.0f);
            b->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff4ad6ff).withAlpha (0.35f));
            b->onClick = [this, i] { if (laneTabs[i]->getToggleState()) { lane = i - 1; repaint(); } };
        }
        laneTabs[0]->setToggleState (true, juce::dontSendNotification);
        laneTabs[0]->setTooltip (U ("Step velocity (Nerve)"));

        // ---- Buses (Battery): Bus 1-4, FX qaytishi, Master — vertikal faderlar, g'ildirak bilan ham
        auto fader = [this] (juce::Slider& f, const juce::String& prop, double def)
        {
            addAndMakeVisible (f); f.setSliderStyle (juce::Slider::LinearVertical);
            f.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0); f.setRange (-60.0, 6.0, 0.1); f.setSkewFactorFromMidPoint (-12.0);
            f.setTextValueSuffix (" dB"); f.setDoubleClickReturnValue (true, def); f.setScrollWheelEnabled (true);
            f.setMouseDragSensitivity (180);
            f.onValueChange = [this, &f, prop] { plugin.state.setProperty (prop, f.getValue(), plugin.um()); };
        };
        for (int b = 0; b < beat::numBuses; ++b) fader (busF[b], "bus" + juce::String (b + 1), 0.0);
        fader (fxF, "fxReturn", -6.0);
        fader (masterK, "master", 0.0);
        masterK.onValueChange = [this] { plugin.state.setProperty ("master", masterK.getValue(), plugin.um()); };
        // ---- Pad mikseri: har sample'ning o'z faderi (pad gain), metri, M/S, chiqish shinasi
        for (int p = 0; p < beat::numPads; ++p)
        {
            auto& f = padF[p];
            addAndMakeVisible (f); f.setSliderStyle (juce::Slider::LinearVertical); f.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            f.setRange (-60.0, 12.0, 0.1); f.setSkewFactorFromMidPoint (-12.0); f.setDoubleClickReturnValue (true, 0.0); f.setScrollWheelEnabled (true);
            f.setMouseDragSensitivity (180); f.setPopupDisplayEnabled (true, true, this); f.setTextValueSuffix (" dB");
            f.setColour (juce::Slider::trackColourId, beatColour (p));
            f.onValueChange = [this, p]
            {
                plugin.pad (p).setProperty ("gain", padF[p].getValue(), plugin.um());
                if (p == sel) gainK.setValue (padF[p].getValue(), juce::dontSendNotification);
                repaint (mixPanel().toNearestInt());
            };
            f.onDragStart = [this, p] { if (sel != p) { sel = p; refresh(); } };
        }
        addChildComponent (nameEd);
        nameEd.setFont (font (12, 600)); nameEd.setJustification (juce::Justification::centredLeft); nameEd.setIndents (6, 2);
        nameEd.setColour (juce::TextEditor::backgroundColourId, bg1); nameEd.setColour (juce::TextEditor::outlineColourId, red);
        nameEd.setColour (juce::TextEditor::focusedOutlineColourId, red);
        nameEd.onReturnKey = [this] { commitRename(); };
        nameEd.onEscapeKey = [this] { nameEd.setVisible (false); };
        nameEd.onFocusLost = [this] { commitRename(); };
        addAndMakeVisible (busBox);
        busBox.addItem ("Out: Master", 1); for (int b = 1; b <= beat::numBuses; ++b) busBox.addItem ("Out: Bus " + juce::String (b), b + 1);
        busBox.setTooltip (U ("Tanlangan pad qaysi shinaga chiqadi (Battery Buses)"));
        busBox.onChange = [this] { plugin.pad (sel).setProperty ("bus", busBox.getSelectedId() - 1, plugin.um()); repaint(); };

        // ---- Pitch Envelope (Battery)
        knob (peAmtK, -48, 48, 0.1, " st"); peAmtK.getProperties().set ("bipolar", true); peAmtK.setDoubleClickReturnValue (true, 0);
        knob (peD1K, 1, 2000, 1, " ms", 80); knob (peBrkK, 0, 1, 0.01, ""); knob (peD2K, 1, 5000, 1, " ms", 400);
        bindPad (peAmtK, "pe_amt"); bindPad (peD1K, "pe_d1"); bindPad (peBrkK, "pe_brk"); bindPad (peD2K, "pe_d2");
        // ---- Amp envelope (Battery): Attack, Hold, Decay, Sustain, Release
        knob (holdK, 0, 5000, 1, " ms", 300); knob (envDecK, 0, 5000, 1, " ms", 300); knob (susK, 0, 1, 0.01, "");
        susK.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100)) + "%"; };
        susK.setDoubleClickReturnValue (true, 1.0); holdK.setDoubleClickReturnValue (true, 0); envDecK.setDoubleClickReturnValue (true, 0);
        bindPad (holdK, "hold"); bindPad (envDecK, "env_dec"); bindPad (susK, "sustain");
        peBrkK.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100)) + "%"; };

        // ---- Filter / EQ (Battery)
        const char* ft[] = { "OFF", "LOWPASS", "HIPASS", "BANDPASS" };
        for (int i = 0; i < 4; ++i)
        {
            auto* b = fTypeB.add (new juce::TextButton (ft[i])); addAndMakeVisible (b);
            b->setClickingTogglesState (true); b->setRadioGroupId (772); b->getProperties().set ("radius", 4.0f);
            b->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffffc04a).withAlpha (0.35f));
            b->onClick = [this, i] { if (fTypeB[i]->getToggleState()) { plugin.pad (sel).setProperty ("f_type", i, plugin.um()); repaint(); } };
        }
        const char* fs[] = { "1P", "2P", "4P" };
        for (int i = 0; i < 3; ++i)
        {
            auto* b = fSlopeB.add (new juce::TextButton (fs[i])); addAndMakeVisible (b);
            b->setClickingTogglesState (true); b->setRadioGroupId (773); b->getProperties().set ("radius", 4.0f);
            b->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffffc04a).withAlpha (0.35f));
            b->setTooltip (i == 0 ? "6 dB/okt" : i == 1 ? "12 dB/okt" : "24 dB/okt");
            b->onClick = [this, i] { if (fSlopeB[i]->getToggleState()) { plugin.pad (sel).setProperty ("f_slope", i, plugin.um()); repaint(); } };
        }
        knob (fCutK, 20, 20000, 1, "", 1000); knob (fResK, 0, 1, 0.01, ""); knob (fGainK, -24, 24, 0.1, " dB");
        fGainK.getProperties().set ("bipolar", true); fGainK.setDoubleClickReturnValue (true, 0); fCutK.setDoubleClickReturnValue (true, 20000);
        fResK.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100)) + "%"; };
        fCutK.textFromValueFunction = [] (double v) { return v >= 1000 ? juce::String (v / 1000.0, 1) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz"; };
        bindPad (fCutK, "f_cut"); bindPad (fResK, "f_res"); bindPad (fGainK, "f_gain");
        for (auto* k : { &gainK, &panK, &tuneK, &fineK, &atkK, &decK, &dlyK, &swingK, &peAmtK, &peD1K, &peBrkK, &peD2K, &fCutK, &fResK, &fGainK, &holdK, &envDecK, &susK })
            k->setScrollWheelEnabled (true);
        refresh();
        holdK.textFromValueFunction = envDecK.textFromValueFunction = atkK.textFromValueFunction;
        for (auto* k : { &atkK, &decK, &holdK, &envDecK }) k->setTextValueSuffix ({});
        setSize (defaultW, defaultH);
        startTimerHz (30);
    }
    ~BeatEditor() override { stopTimer(); }
    bool allowWindowResizing() override { return false; }
    juce::ComponentBoundsConstrainer* getBoundsConstrainer() override { return nullptr; }

    void refresh()
    {
        auto v = plugin.pad (sel);
        for (auto [s, id] : std::initializer_list<std::pair<juce::Slider*, const char*>> { { &gainK, "gain" }, { &panK, "pan" }, { &tuneK, "tune" }, { &fineK, "fine" }, { &atkK, "attack" }, { &decK, "decay" }, { &dlyK, "delay" } })
            s->setValue ((double) v[id], juce::dontSendNotification);
        normB.setToggleState (v["norm"], juce::dontSendNotification);
        updateMatch();
        revB.setToggleState (v["reverse"], juce::dontSendNotification); syncB.setToggleState (v["sync"], juce::dontSendNotification);
        muteB.setToggleState (v["mute"], juce::dontSendNotification); soloB.setToggleState (v["solo"], juce::dontSendNotification);
        chokeBox.setSelectedId ((int) v["choke"] + 1, juce::dontSendNotification);
        seqB.setToggleState (plugin.state["seqOn"], juce::dontSendNotification);
        lenBox.setSelectedId (plugin.length(), juce::dontSendNotification);
        rateBox.setSelectedId ((int) plugin.state["rate"] + 1, juce::dontSendNotification);
        swingK.setValue ((double) plugin.state["swing"] * 100.0, juce::dontSendNotification);
        masterK.setValue ((double) plugin.state["master"], juce::dontSendNotification);
        for (int b = 0; b < beat::numBuses; ++b) busF[b].setValue ((double) plugin.state.getProperty ("bus" + juce::String (b + 1), 0.0), juce::dontSendNotification);
        fxF.setValue ((double) plugin.state.getProperty ("fxReturn", -6.0), juce::dontSendNotification);
        for (int p = 0; p < beat::numPads; ++p) padF[p].setValue ((double) plugin.pad (p)["gain"], juce::dontSendNotification);
        holdK.setValue ((double) v.getProperty ("hold", 0.0), juce::dontSendNotification);
        envDecK.setValue ((double) v.getProperty ("env_dec", 0.0), juce::dontSendNotification);
        susK.setValue ((double) v.getProperty ("sustain", 1.0), juce::dontSendNotification);
        busBox.setSelectedId ((int) v.getProperty ("bus", 0) + 1, juce::dontSendNotification);
        for (auto [s, id, d] : std::initializer_list<std::tuple<juce::Slider*, const char*, double>> { { &peAmtK, "pe_amt", 0.0 }, { &peD1K, "pe_d1", 60.0 }, { &peBrkK, "pe_brk", 0.3 }, { &peD2K, "pe_d2", 300.0 },
                                                                                                      { &fCutK, "f_cut", 20000.0 }, { &fResK, "f_res", 0.1 }, { &fGainK, "f_gain", 0.0 } })
            s->setValue ((double) v.getProperty (id, d), juce::dontSendNotification);
        fTypeB[juce::jlimit (0, 3, (int) v.getProperty ("f_type", 0))]->setToggleState (true, juce::dontSendNotification);
        fSlopeB[juce::jlimit (0, 2, (int) v.getProperty ("f_slope", 1))]->setToggleState (true, juce::dontSendNotification);
        repaint();
    }

    // ---------------------------------------------------------------- joylashuv
    static constexpr float rowH = 17.0f;
    juce::Rectangle<float> padRect (int i) const
    {
        const int row = 4 - i / 4, col = i % 4;              // Battery: 1-pad pastki chapda (MPC kabi), 20 pad = 4 x 5
        const float sz = 70, g = 7;
        return { 16 + col * (sz + g), 64 + row * (sz + g), sz, sz };
    }
    juce::Rectangle<float> gridArea() const { return { 340, 88, (float) getWidth() - 356, beat::numPads * rowH }; }
    juce::Rectangle<float> laneArea() const { return gridArea().withTrimmedLeft (110); }
    juce::Rectangle<float> cell (int p, int s) const
    {
        auto g = gridArea().withTrimmedLeft (110);
        const float w = g.getWidth() / plugin.length();
        return { g.getX() + s * w, g.getY() + p * rowH, w, rowH };
    }
public:
    static constexpr int defaultW = 1240, defaultH = 1136;
private:
    juce::Rectangle<float> waveArea() const { return { 16, 478, 580, 158 }; }
    juce::Rectangle<float> pePanel() const  { return { 16, 650, 388, 224 }; }
    juce::Rectangle<float> fltPanel() const { return { 416, 650, 388, 224 }; }
    juce::Rectangle<float> ampPanel() const { return { 816, 650, (float) getWidth() - 832, 224 }; }
    juce::Rectangle<float> mixPanel() const { return { 16, 884, (float) getWidth() - 32, 226 }; }
    // mikser: 0..19 — padlar, 20..23 — Bus 1-4, 24 — FX, 25 — Master
    static constexpr int numStrips = beat::numPads + 6;
    juce::Rectangle<float> stripRect (int k) const
    {
        auto mp = mixPanel(); const float x0 = mp.getX() + 8, w = mp.getWidth() - 16, bw = 48;
        const float pw = (w - 6 * bw - 10) / beat::numPads;
        if (k < beat::numPads) return { x0 + k * pw, mp.getY() + 24, pw, mp.getHeight() - 28 };
        return { x0 + beat::numPads * pw + 10 + (k - beat::numPads) * bw, mp.getY() + 24, bw, mp.getHeight() - 28 };
    }
    juce::Rectangle<float> stripName (int k) const  { auto r = stripRect (k); return { r.getX() + 1, r.getY(), r.getWidth() - 2, 17 }; }
    juce::Rectangle<float> stripFader (int k) const { auto r = stripRect (k); return { r.getX() + 2, r.getY() + 20, juce::jmin (26.0f, r.getWidth() - 14), 124 }; }
    juce::Rectangle<float> stripMeter (int k) const { auto f = stripFader (k); return { f.getRight() + 2, f.getY() + 6, 5, f.getHeight() - 12 }; }
    juce::Rectangle<float> stripDb (int k) const    { auto r = stripRect (k); return { r.getX(), r.getY() + 146, r.getWidth(), 12 }; }
    juce::Rectangle<float> stripM (int k) const     { auto r = stripRect (k); return { r.getX() + 2, r.getY() + 161, (r.getWidth() - 6) * 0.5f, 15 }; }
    juce::Rectangle<float> stripS (int k) const     { auto m = stripM (k); return m.translated (m.getWidth() + 2, 0); }
    juce::Rectangle<float> stripOut (int k) const   { auto r = stripRect (k); return { r.getX() + 2, r.getY() + 179, r.getWidth() - 4, 15 }; }
    juce::String busName (int b) const   // b: 1..4
    { return plugin.state.getProperty ("busName" + juce::String (b), "BUS " + juce::String (b)).toString(); }
    juce::String padName (int p) const
    { const auto n = plugin.pad (p)["name"].toString(); return n.isNotEmpty() ? n : "Pad " + juce::String (p + 1); }

    void resized() override
    {
        auto top = juce::Rectangle<int> (200, 14, getWidth() - 214, 30);
        loadKitB.setBounds (top.removeFromLeft (64)); top.removeFromLeft (4); saveKitB.setBounds (top.removeFromLeft (70)); top.removeFromLeft (4);
        defaultB.setBounds (top.removeFromLeft (66)); top.removeFromLeft (16);
        sliceB.setBounds (top.removeFromLeft (110)); top.removeFromLeft (4); toMidiB.setBounds (top.removeFromLeft (120)); top.removeFromLeft (16);
        seqB.setBounds (top.removeFromLeft (52)); top.removeFromLeft (4); lenBox.setBounds (top.removeFromLeft (88)); top.removeFromLeft (4);
        rateBox.setBounds (top.removeFromLeft (70)); top.removeFromLeft (4); clearB.setBounds (top.removeFromLeft (76)); top.removeFromLeft (4);
        humanB.setBounds (top.removeFromLeft (86));
        // Nerve lane tablari
        {
            int x = 340; const int w = (int) ((getWidth() - 356 - 7 * 4) / 8);
            for (auto* b : laneTabs) { b->setBounds (x, 60, w, 22); x += w + 4; }
        }
        // pad paneli
        const int y = 470;
        int x = 620;
        for (auto* k : { &gainK, &panK, &tuneK, &fineK, &dlyK }) { k->setBounds (x, y + 6, 58, 78); x += 64; }
        swingK.setBounds (getWidth() - 82, y + 6, 64, 78);
        x = 620;
        loadSmpB.setBounds (x, 568, 90, 26); revB.setBounds (x + 96, 568, 80, 26); syncB.setBounds (x + 182, 568, 62, 26);
        chokeBox.setBounds (x + 250, 568, 96, 26); muteB.setBounds (x + 352, 568, 30, 26); soloB.setBounds (x + 386, 568, 30, 26);
        normB.setBounds (x, 600, 120, 24); busBox.setBounds (x + 126, 600, 120, 24);
        prevB.setBounds (16, 456, 26, 18); nextB.setBounds (46, 456, 26, 18);
        // Mikser: pad faderlari + Bus 1-4, FX, Master
        for (int p = 0; p < beat::numPads; ++p) padF[p].setBounds (stripFader (p).toNearestInt());
        juce::Slider* fs[] = { &busF[0], &busF[1], &busF[2], &busF[3], &fxF, &masterK };
        for (int i = 0; i < 6; ++i) fs[i]->setBounds (stripFader (beat::numPads + i).toNearestInt());
        // Amp envelope
        {
            auto ap = ampPanel().toNearestInt(); const int kw = (ap.getWidth() - 16) / 5;
            int ax = ap.getX() + 8;
            for (auto* k : { &atkK, &holdK, &envDecK, &susK, &decK }) { k->setBounds (ax + (kw - 66) / 2, ap.getY() + 136, 66, 80); ax += kw; }
        }
        // Pitch envelope
        auto pe = pePanel().toNearestInt();
        x = pe.getX() + 8;
        for (auto* k : { &peAmtK, &peD1K, &peBrkK, &peD2K }) { k->setBounds (x, pe.getY() + 136, 66, 80); x += 70; }
        // Filter / EQ
        auto fp = fltPanel().toNearestInt();
        {
            int fx = fp.getX() + 8; const int bw = (fp.getWidth() - 16 - 3 * 3) / 4;
            for (auto* b : fTypeB) { b->setBounds (fx, fp.getY() + 24, bw, 20); fx += bw + 3; }
            fx = fp.getX() + 8;
            for (auto* b : fSlopeB) { b->setBounds (fx, fp.getY() + 48, 36, 20); fx += 39; }
            fx = fp.getX() + 8; const int kw = (fp.getWidth() - 16) / 3;
            for (auto* k : { &fCutK, &fResK, &fGainK }) { k->setBounds (fx + (kw - 66) / 2, fp.getY() + 136, 66, 80); fx += kw; }
        }
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        g.fillAll (bg0);
        // sarlavha
        g.setColour (red); g.fillRoundedRectangle (16, 14, 30, 30, 8);
        label (g, "AZ", { 16, 14, 30, 30 }, 12, Colours::white, Justification::centred, 800);
        label (g, "AZ BEAT", { 54, 12, 140, 18 }, 15, text, Justification::centredLeft, 800);
        label (g, plugin.state["kit"].toString().isEmpty() ? String ("Kit") : plugin.state["kit"].toString(), { 54, 30, 140, 14 }, 9.5f, dim, Justification::centredLeft, 500);
        // padlar (Battery)
        const auto now = Time::getMillisecondCounter();
        for (int i = 0; i < beat::numPads; ++i)
        {
            auto r = padRect (i); auto v = plugin.pad (i);
            const bool has = v["file"].toString().isNotEmpty();
            const float hit = jlimit (0.0f, 1.0f, 1.0f - (float) (now - plugin.padHit[i].load()) / 220.0f);
            const auto col = beatColour (i);
            g.setColour (col.withAlpha (has ? 0.16f + hit * 0.5f : 0.04f)); g.fillRoundedRectangle (r, 10);
            g.setColour (i == sel ? Colours::white.withAlpha (0.85f) : col.withAlpha (has ? 0.55f : 0.15f)); g.drawRoundedRectangle (r.reduced (0.5f), 10, i == sel ? 2.0f : 1.0f);
            // glyans
            g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.10f), r.getX(), r.getY(), Colours::transparentWhite, r.getX(), r.getCentreY(), false));
            g.fillRoundedRectangle (r.reduced (2).withHeight (r.getHeight() * 0.45f), 8);
            label (g, String (i + 1), r.reduced (8, 6).withHeight (12), 9, dim, Justification::topLeft, 700);
            label (g, MidiMessage::getMidiNoteName (beat::firstNote + i, true, true, 3), r.reduced (8, 6).withHeight (12), 8.5f, dim2, Justification::topRight, 500);
            label (g, has ? v["name"].toString() : U ("bo'sh"), r.reduced (6, 0).withTrimmedTop (r.getHeight() - 26).withHeight (18), 9.5f, has ? text : dim2, Justification::centredLeft, 600);
            if ((bool) v["mute"]) label (g, "M", r.reduced (8, 22).withHeight (12), 9, yellow, Justification::topLeft, 800);
            if ((bool) v["solo"]) label (g, "S", r.reduced (20, 22).withHeight (12), 9, green, Justification::topLeft, 800);
            if ((int) v["choke"] > 0) label (g, "CH" + String ((int) v["choke"]), r.reduced (8, 6).withTrimmedTop (r.getHeight() - 40).withHeight (12), 8.5f, blue, Justification::centredRight, 700);
        }
        caps (g, U ("PADLAR \xc2\xb7 SAMPLE'NI SUDRAB TASHLANG"), { 16, 48, 380, 12 }, 8.5f);
        // step grid (Nerve)
        const int L = plugin.length(); const int ps = plugin.playStep.load();
        auto ga = gridArea();
        const String dot = String::fromUTF8 ("  \xc2\xb7  ");
        caps (g, "SEQUENCER" + dot + String (L) + " step" + dot + String (beat::rateNames[jlimit (0, 3, (int) plugin.state["rate"])]), { ga.getX(), 44, 400, 12 }, 8.5f);
        for (int p = 0; p < beat::numPads; ++p)
        {
            auto v = plugin.pad (p);
            auto row = Rectangle<float> (ga.getX(), ga.getY() + p * rowH, 106, rowH - 2);
            g.setColour (p == sel ? beatColour (p).withAlpha (0.25f) : Colours::white.withAlpha (0.03f)); g.fillRoundedRectangle (row, 4);
            label (g, String (p + 1) + "  " + (v["name"].toString().isEmpty() ? String::fromUTF8 ("\xe2\x80\x94") : v["name"].toString()), row.reduced (6, 0), 8.5f, (bool) v["mute"] ? dim2 : text, Justification::centredLeft, 500);
            if (const int bus = (int) v.getProperty ("bus", 0); bus > 0) label (g, "B" + String (bus), row.reduced (4, 0), 7.5f, Colour (0xff4ad6ff), Justification::centredRight, 700);
            if (lane >= 0) continue;
            auto st = AzBeatPlugin::intList (v["steps"], 0), ra = AzBeatPlugin::intList (v["ratchet"], 1);
            for (int s = 0; s < L; ++s)
            {
                auto c = cell (p, s).reduced (1.0f, 1.5f);
                const bool beatStart = s % 4 == 0;
                g.setColour (Colours::white.withAlpha (s == ps ? 0.13f : beatStart ? 0.06f : 0.035f)); g.fillRoundedRectangle (c, 2.5f);
                if (const int vel = st[s]; vel > 0)
                {
                    const float h = c.getHeight() * (0.35f + 0.65f * vel / 127.0f);
                    g.setColour (beatColour (p).withAlpha (0.55f + 0.45f * vel / 127.0f)); g.fillRoundedRectangle (c.withTop (c.getBottom() - h), 2.5f);
                    if (ra[s] > 1) label (g, "x" + String (ra[s]), c, 7.5f, Colours::white, Justification::centred, 700);
                }
            }
        }
        if (lane >= 0) paintLane (g);
        if (ps >= 0 && ps < L) { auto c = cell (0, ps); g.setColour (Colours::white.withAlpha (0.5f)); g.fillRect (c.getX(), ga.getY() - 4, c.getWidth(), 3.0f); }
        label (g, lane < 0 ? U ("Bosish: qo'yish/olish \xc2\xb7 vertikal sudrash yoki g'ildirak: velocity \xc2\xb7 Shift+bosish: ratchet \xc2\xb7 o'ng tugma: qatorni to'ldirish")
                           : U ("Lane: Pad ") + String (sel + 1) + U (" \xc2\xb7 chizib sudrang \xc2\xb7 g'ildirak: qiymat \xc2\xb1 \xc2\xb7 ikki marta bosish: standart qiymat \xc2\xb7 Alt+bosish: butun lane'ni tiklash"),
               { ga.getX(), ga.getBottom() + 4, ga.getWidth(), 14 }, 9, dim2, Justification::centredLeft, 400);
        // tanlangan pad: to'lqin + start/end + playhead
        auto wa = waveArea();
        caps (g, "PAD " + String (sel + 1) + String::fromUTF8 ("  \xc2\xb7  ") + plugin.pad (sel)["name"].toString(), { wa.getX() + 64, wa.getY() - 18, 240, 12 }, 8.5f, beatColour (sel));
        if (matchText.isNotEmpty())
        {
            const Colour mc = matchScore == 2 ? green : matchScore == 1 ? yellow : red;
            auto br = Rectangle<float> (wa.getRight() - 300, wa.getY() - 21, 300, 16);
            g.setColour (mc.withAlpha (0.14f)); g.fillRoundedRectangle (br, 4);
            label (g, matchText, br.reduced (6, 0), 9, mc, Justification::centredRight, 600);
        }
        g.setColour (Colour (0xff09080b)); g.fillRoundedRectangle (wa, 8);
        double sr = 48000; auto buf = plugin.sampleOf (sel, sr);
        if (buf && buf->getNumSamples() > 1)
        {
            const int W = (int) wa.getWidth(); const int N = buf->getNumSamples();
            for (int x = 0; x < W; ++x)
            {
                const int a = (int) ((double) N * x / W), b = jmax (a + 1, (int) ((double) N * (x + 1) / W));
                const float m = buf->getMagnitude (0, a, jmin (b, N) - a);
                g.setColour (beatColour (sel).withAlpha (0.65f));
                g.fillRect (wa.getX() + x, wa.getCentreY() - m * wa.getHeight() * 0.45f, 1.0f, jmax (1.0f, m * wa.getHeight() * 0.9f));
            }
            const float sx = wa.getX() + wa.getWidth() * (float) (double) plugin.pad (sel)["start"], ex = wa.getX() + wa.getWidth() * (float) (double) plugin.pad (sel)["end"];
            g.setColour (Colours::black.withAlpha (0.55f)); g.fillRect (wa.withRight (sx)); g.fillRect (wa.withLeft (ex));
            g.setColour (green); g.fillRect (sx - 1, wa.getY(), 2.0f, wa.getHeight());
            g.setColour (red); g.fillRect (ex - 1, wa.getY(), 2.0f, wa.getHeight());
            label (g, "S", { sx + 3, wa.getY() + 2, 12, 12 }, 9, green, Justification::centredLeft, 800);
            label (g, "E", { ex - 13, wa.getY() + 2, 12, 12 }, 9, red, Justification::centredLeft, 800);
            paintWaveEnv (g, wa, sx, ex, N, sr);
            // ovoz qayerda ketayotgani (playhead)
            if (const float pp = plugin.padPos[sel].load(); pp >= 0.0f)
            {
                const float px = wa.getX() + wa.getWidth() * jlimit (0.0f, 1.0f, pp);
                g.setColour (Colours::white.withAlpha (0.10f)); g.fillRect (jmin (sx, px), wa.getY(), std::abs (px - sx), wa.getHeight());
                g.setColour (Colours::white); g.fillRect (px - 1.0f, wa.getY(), 2.0f, wa.getHeight());
                Path tri; tri.addTriangle (px - 5, wa.getY(), px + 5, wa.getY(), px, wa.getY() + 7); g.fillPath (tri);
            }
        }
        else label (g, U ("Sample yo'q \xe2\x80\x94 media brauzerdan sudrab tashlang yoki \"Sample...\""), wa, 10.5f, dim, Justification::centred, 500);
        const char* kn[] = { "GAIN", "PAN", "TUNE", "FINE", "DELAY" };
        int x = 620; for (auto* n : kn) { caps (g, n, { (float) x, 458, 58, 10 }, 8, n[1] == 'E' && n[2] == 'L' ? Colour (0xff4ad6ff) : dim, Justification::centred); x += 64; }
        caps (g, "SWING", { (float) getWidth() - 82, 458, 64, 10 }, 8, dim, Justification::centred);
        g.setColour (Colours::white.withAlpha (0.06f)); g.drawVerticalLine (getWidth() - 92, 456, 560);
        paintMixer (g); paintPitchEnv (g); paintFilter (g); paintAmpEnv (g);
        if (dropHint >= 0) { g.setColour (Colours::white.withAlpha (0.8f)); g.drawRoundedRectangle (padRect (dropHint), 10, 2.5f); }
        if (info.isNotEmpty()) label (g, info, { 16, (float) getHeight() - 24, (float) getWidth() - 32, 18 }, 10, silver, Justification::centredLeft, 500);
        label (g, U ("MIDI: C1 (36) = Pad 1 \xe2\x80\xa6 G2 (55) = Pad 20"), { 16, (float) getHeight() - 24, (float) getWidth() - 32, 18 }, 9, dim2, Justification::centredRight, 400);
    }

    // ---------------------------------------------------------------- Nerve lane (katta ustunlar)
    static bool laneBipolar (int l) { return l == beat::lanePitch || l == beat::lanePan; }
    void paintLane (juce::Graphics& g)
    {
        using namespace juce;
        auto la = laneArea(); const int L = plugin.length(); const int ps = plugin.playStep.load();
        g.setColour (Colour (0xff09080b)); g.fillRoundedRectangle (la, 6);
        const Colour col = beatColour (sel);
        const float w = la.getWidth() / L;
        const float midY = la.getBottom() - la.getHeight() * 64.0f / 127.0f;
        for (int s = 0; s < L; ++s)
        {
            auto c = Rectangle<float> (la.getX() + s * w, la.getY(), w, la.getHeight()).reduced (1.5f, 3.0f);
            g.setColour (Colours::white.withAlpha (s == ps ? 0.10f : s % 4 == 0 ? 0.05f : 0.025f)); g.fillRoundedRectangle (c, 2);
            const int val = plugin.laneVal (sel, lane, s);
            const bool on = plugin.stepVel (sel, s) > 0;
            const float vy = c.getBottom() - c.getHeight() * val / 127.0f;
            g.setColour (col.withAlpha (on ? 0.85f : 0.28f));
            if (laneBipolar (lane)) g.fillRect (Rectangle<float> (c.getX(), jmin (vy, midY), c.getWidth(), std::abs (vy - midY) + 1.5f));
            else g.fillRoundedRectangle (c.withTop (vy), 2);
            g.setColour (Colours::white.withAlpha (on ? 0.9f : 0.35f)); g.fillRect (c.getX(), vy - 1, c.getWidth(), 2.0f);
        }
        if (laneBipolar (lane)) { g.setColour (Colours::white.withAlpha (0.25f)); g.drawHorizontalLine ((int) midY, la.getX(), la.getRight()); }
        label (g, String (beat::laneNames[lane]).toUpperCase() + String::fromUTF8 ("  \xc2\xb7  ") + laneValueText (lane, hoverStep >= 0 ? plugin.laneVal (sel, lane, hoverStep) : beat::laneDef[lane]),
               la.reduced (8, 4).withHeight (14), 9, Colour (0xff4ad6ff), Justification::topRight, 700);
    }
    static juce::String laneValueText (int l, int v)
    {
        switch (l)
        {
            case beat::laneCut:   return juce::String (juce::roundToInt (v / 1.27)) + "%";
            case beat::laneRes:   return juce::String (juce::roundToInt (v / 1.27)) + "%";
            case beat::lanePitch: return juce::String ((v - 64) / 64.0 * 12.0, 1) + " st";
            case beat::lanePan:   return v == 64 ? juce::String ("C") : (v < 64 ? "L" : "R") + juce::String (std::abs (v - 64) * 100 / 64);
            case beat::laneGate:  return juce::String (juce::roundToInt (v / 1.27)) + "%";
            case beat::laneLate:  return juce::String (juce::roundToInt (v / 1.27 * 0.5)) + "% step";
            default:              return juce::String (juce::roundToInt (v / 1.27)) + "%";
        }
    }
    int laneStepAt (juce::Point<float> pt) const
    {
        auto la = laneArea();
        if (pt.x < la.getX() || pt.x >= la.getRight()) return -1;
        return juce::jlimit (0, plugin.length() - 1, (int) ((pt.x - la.getX()) / (la.getWidth() / plugin.length())));
    }
    int laneValueAt (float y) const { auto la = laneArea().reduced (0, 3); return juce::jlimit (0, 127, juce::roundToInt ((la.getBottom() - y) / la.getHeight() * 127.0f)); }

    // ---------------------------------------------------------------- Battery panellari
    void panelBg (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title)
    {
        g.setColour (juce::Colours::white.withAlpha (0.025f)); g.fillRoundedRectangle (r, 8);
        g.setColour (juce::Colours::white.withAlpha (0.07f)); g.drawRoundedRectangle (r.reduced (0.5f), 8, 1.0f);
        caps (g, title, r.reduced (10, 8).withHeight (12), 8.5f);
    }
    // ---------------------------------------------------------------- Mikser (har sample + shinalar)
    void paintMixer (juce::Graphics& g)
    {
        using namespace juce;
        auto mp = mixPanel(); panelBg (g, mp, U ("MIKSER \xc2\xb7 PADLAR VA SHINALAR"));
        label (g, U ("fader / g'ildirak \xc2\xb7 ikki marta bosish: 0 dB \xc2\xb7 nomni ikki marta bosib o'zgartiring \xc2\xb7 OUT: shina"), mp.reduced (10, 8).withHeight (12), 8.5f, dim2, Justification::centredRight, 400);
        float mpk = 0; for (auto& b : plugin.busPeak) mpk = jmax (mpk, b.load());
        for (int k = 0; k < numStrips; ++k)
        {
            auto r = stripRect (k);
            const bool isPad = k < beat::numPads;
            const int p = k;
            if (isPad && p == sel) { g.setColour (beatColour (p).withAlpha (0.12f)); g.fillRoundedRectangle (r, 4); }
            if (k == beat::numPads) { g.setColour (Colours::white.withAlpha (0.10f)); g.drawVerticalLine ((int) r.getX() - 6, r.getY(), r.getBottom()); }
            // nom
            auto nr = stripName (k);
            const Colour col = isPad ? beatColour (p) : k == numStrips - 1 ? red : k == numStrips - 2 ? Colour (0xffb98aff) : Colour (0xff4ad6ff);
            g.setColour (col.withAlpha (isPad ? 0.85f : 0.6f)); g.fillRect (nr.getX(), nr.getY(), nr.getWidth(), 2.0f);
            const String nm = isPad ? padName (p) : k < beat::numPads + 4 ? busName (k - beat::numPads + 1) : k == numStrips - 2 ? String ("FX") : String ("MASTER");
            label (g, isPad ? String (p + 1) : String(), nr.withTrimmedTop (3), 7, dim2, Justification::topLeft, 700);
            label (g, nm, nr.withTrimmedTop (3).withTrimmedLeft (isPad ? 9.0f : 0.0f), isPad ? 8.0f : 8.5f, isPad && (bool) plugin.pad (p)["mute"] ? dim2 : text, isPad ? Justification::centredLeft : Justification::centred, 600);
            // metr
            const float pk = isPad ? plugin.padPeak[p].load() : k < beat::numPads + 4 ? plugin.busPeak[k - beat::numPads + 1].load() : k == numStrips - 2 ? plugin.busPeak[5].load() : mpk;
            auto m = stripMeter (k);
            g.setColour (Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (m, 2);
            const float db = Decibels::gainToDecibels (pk, -60.0f);
            const float h = m.getHeight() * jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f);
            g.setGradientFill (ColourGradient (green, 0, m.getBottom(), red, 0, m.getY(), false));
            g.fillRoundedRectangle (m.withTop (m.getBottom() - h), 2);
            // dB qiymati
            const double val = isPad ? padF[p].getValue() : k < beat::numPads + 4 ? busF[k - beat::numPads].getValue() : k == numStrips - 2 ? fxF.getValue() : masterK.getValue();
            label (g, val <= -59.9 ? String::fromUTF8 ("-\xe2\x88\x9e") : String (val, 1), stripDb (k), 8, silver, Justification::centred, 600);
            if (isPad)
            {
                auto v = plugin.pad (p);
                auto mm = stripM (k), ss = stripS (k);
                const bool mu = v["mute"], so = v["solo"];
                g.setColour (mu ? yellow.withAlpha (0.8f) : Colours::white.withAlpha (0.06f)); g.fillRoundedRectangle (mm, 3);
                g.setColour (so ? green.withAlpha (0.8f) : Colours::white.withAlpha (0.06f)); g.fillRoundedRectangle (ss, 3);
                label (g, "M", mm, 8, mu ? Colours::black : dim, Justification::centred, 800);
                label (g, "S", ss, 8, so ? Colours::black : dim, Justification::centred, 800);
                const int bus = (int) v.getProperty ("bus", 0);
                auto o = stripOut (k);
                g.setColour (bus > 0 ? Colour (0xff4ad6ff).withAlpha (0.22f) : Colours::white.withAlpha (0.05f)); g.fillRoundedRectangle (o, 3);
                label (g, bus > 0 ? "B" + String (bus) : String ("MST"), o, 7.5f, bus > 0 ? Colour (0xff4ad6ff) : dim, Justification::centred, 700);
            }
            else if (k < beat::numPads + 4)
            {
                int cnt = 0; for (int q = 0; q < beat::numPads; ++q) if ((int) plugin.pad (q).getProperty ("bus", 0) == k - beat::numPads + 1) ++cnt;
                label (g, String (cnt) + " pad", stripM (k).withWidth (r.getWidth() - 4), 8, dim2, Justification::centred, 500);
            }
        }
    }
    // ---------------------------------------------------------------- Amp envelope paneli
    void paintAmpEnv (juce::Graphics& g)
    {
        using namespace juce;
        auto ap = ampPanel(); panelBg (g, ap, "AMP ENVELOPE");
        auto v = plugin.pad (sel);
        const float a = (float) (double) v["attack"], h = (float) (double) v.getProperty ("hold", 0.0), d = (float) (double) v.getProperty ("env_dec", 0.0),
                    su = (float) (double) v.getProperty ("sustain", 1.0), rl = (float) (double) v["decay"];
        auto r = Rectangle<float> (ap.getX() + 10, ap.getY() + 26, ap.getWidth() - 20, 92);
        g.setColour (Colour (0xff09080b)); g.fillRoundedRectangle (r, 6);
        const float total = jmax (1.0f, a + h + d + rl) * 1.25f + 1;
        const float body = total * 0.2f;   // sustain bo'lagi
        auto X = [&] (float t) { return r.getX() + 6 + (r.getWidth() - 12) * t / (total + body); };
        const float top = r.getY() + 8, bot = r.getBottom() - 6, ys = bot - (bot - top) * su;
        Path p; p.startNewSubPath (X (0), bot); p.lineTo (X (a), top); p.lineTo (X (a + h), top); p.lineTo (X (a + h + d), ys);
        p.lineTo (X (a + h + d + body), ys); p.lineTo (X (a + h + d + body + rl), bot);
        Path f (p); f.lineTo (X (0), bot); f.closeSubPath();
        g.setColour (Colour (0xff4aa8ff).withAlpha (0.22f)); g.fillPath (f);
        g.setColour (Colour (0xff6cc0ff)); g.strokePath (p, PathStrokeType (1.8f));
        label (g, "A " + String (roundToInt (a)) + "  H " + String (roundToInt (h)) + "  D " + String (roundToInt (d)) + "  S " + String (roundToInt (su * 100)) + "%  R " + String (roundToInt (rl)),
               r.reduced (6, 3), 8.5f, Colour (0xff6cc0ff), Justification::topRight, 700);
        const char* kn[] = { "ATTACK", "HOLD", "DECAY", "SUSTAIN", "RELEASE" };
        const float kw = (ap.getWidth() - 16) / 5.0f;
        for (int i = 0; i < 5; ++i) caps (g, kn[i], { ap.getX() + 8 + i * kw, ap.getY() + 124, kw, 10 }, 8, dim, Justification::centred);
    }
    // ---------------------------------------------------------------- to'lqin ustidagi envelope (Battery kabi)
    struct WaveGeo { juce::Rectangle<float> wa; float sx = 0, ex = 0, top = 0, bot = 0; double regionMs = 1; };
    WaveGeo waveGeo() const
    {
        WaveGeo w; w.wa = waveArea();
        double sr = 48000; auto buf = plugin.sampleOf (sel, sr);
        auto v = plugin.pad (sel);
        w.sx = w.wa.getX() + w.wa.getWidth() * (float) (double) v["start"]; w.ex = w.wa.getX() + w.wa.getWidth() * (float) (double) v["end"];
        w.top = w.wa.getY() + 14; w.bot = w.wa.getBottom() - 20;
        const int N = buf ? buf->getNumSamples() : 0;
        w.regionMs = juce::jmax (1.0, (double) N / juce::jmax (1.0, sr) * ((double) v["end"] - (double) v["start"]) * 1000.0);
        return w;
    }
    float xAtMs (const WaveGeo& w, double ms) const { return w.sx + (float) (juce::jlimit (0.0, w.regionMs, ms) / w.regionMs) * (w.ex - w.sx); }
    double msAtX (const WaveGeo& w, float x) const { return juce::jlimit (0.0, w.regionMs, (double) (x - w.sx) / juce::jmax (1.0f, w.ex - w.sx) * w.regionMs); }
    std::array<juce::Point<float>, 6> ampNodes (const WaveGeo& w) const
    {
        auto v = plugin.pad (sel);
        const double a = v["attack"], h = v.getProperty ("hold", 0.0), d = v.getProperty ("env_dec", 0.0), rl = v["decay"];
        const float su = (float) (double) v.getProperty ("sustain", 1.0), ys = w.bot - (w.bot - w.top) * su;
        const float x3 = xAtMs (w, a + h + d), x4 = juce::jmax (x3, xAtMs (w, w.regionMs - rl));
        return { juce::Point<float> (w.sx, w.bot), { xAtMs (w, a), w.top }, { xAtMs (w, a + h), w.top }, { x3, ys }, { x4, ys }, { w.ex, w.bot } };
    }
    std::array<juce::Point<float>, 3> pitchNodes (const WaveGeo& w) const
    {
        auto v = plugin.pad (sel);
        const double amt = v.getProperty ("pe_amt", 0.0), d1 = v.getProperty ("pe_d1", 60.0), d2 = v.getProperty ("pe_d2", 300.0);
        const double brk = juce::jlimit (0.01, 0.999, (double) v.getProperty ("pe_brk", 0.3));
        const float mid = (w.top + w.bot) * 0.5f, half = (w.bot - w.top) * 0.5f;
        auto Y = [&] (double st) { return mid - (float) (st / 48.0) * half; };
        const double t1 = -d1 * std::log (brk), t2 = t1 + 3.0 * d2;
        return { juce::Point<float> (w.sx, Y (amt)), { xAtMs (w, t1), Y (amt * brk) }, { xAtMs (w, t2), Y (amt * 0.05) } };
    }
    juce::Rectangle<float> envTabRect (int i) const { auto wa = waveArea(); return { wa.getRight() - 156 + i * 76.0f, wa.getBottom() - 18, 72, 14 }; }
    void paintWaveEnv (juce::Graphics& g, juce::Rectangle<float> wa, float sx, float ex, int N, double sr)
    {
        using namespace juce;
        const auto w = waveGeo();
        if (envTab == 0)
        {
            const auto n = ampNodes (w);
            Path p; p.startNewSubPath (n[0]); for (int i = 1; i < 6; ++i) p.lineTo (n[(size_t) i]);
            Path f (p); f.closeSubPath();
            g.setGradientFill (ColourGradient (Colour (0xff4aa8ff).withAlpha (0.45f), 0, w.top, Colour (0xff4aa8ff).withAlpha (0.08f), 0, w.bot, false));
            g.fillPath (f);
            g.setColour (Colour (0xff8fd0ff)); g.strokePath (p, PathStrokeType (1.6f));
            for (int i = 1; i <= 4; ++i)
            {
                const auto pt = n[(size_t) i]; const bool hv = hoverNode == i;
                g.setColour (Colours::white); g.fillEllipse (pt.x - (hv ? 5.0f : 3.5f), pt.y - (hv ? 5.0f : 3.5f), hv ? 10.0f : 7.0f, hv ? 10.0f : 7.0f);
            }
        }
        else
        {
            const auto n = pitchNodes (w);
            const float mid = (w.top + w.bot) * 0.5f;
            g.setColour (Colours::white.withAlpha (0.18f)); g.drawHorizontalLine ((int) mid, sx, ex);
            auto v = plugin.pad (sel);
            const double amt = v.getProperty ("pe_amt", 0.0), d1 = v.getProperty ("pe_d1", 60.0), d2 = v.getProperty ("pe_d2", 300.0);
            const double brk = jlimit (0.01, 0.999, (double) v.getProperty ("pe_brk", 0.3));
            Path p; bool first = true;
            for (float x = sx; x <= ex; x += 1.5f)
            {
                const double t = msAtX (w, x), t1 = -d1 * std::log (brk);
                const double lvl = t < t1 ? std::exp (-t / jmax (1.0, d1)) : brk * std::exp (-(t - t1) / jmax (1.0, d2));
                const float y = mid - (float) (amt * lvl / 48.0) * (w.bot - w.top) * 0.5f;
                if (first) { p.startNewSubPath (x, y); first = false; } else p.lineTo (x, y);
            }
            g.setColour (Colour (0xffb98aff)); g.strokePath (p, PathStrokeType (1.8f));
            for (int i = 0; i < 3; ++i)
            {
                const auto pt = n[(size_t) i]; const bool hv = hoverNode == i;
                g.setColour (Colours::white); g.fillEllipse (pt.x - (hv ? 5.0f : 3.5f), pt.y - (hv ? 5.0f : 3.5f), hv ? 10.0f : 7.0f, hv ? 10.0f : 7.0f);
            }
            label (g, String (amt >= 0 ? "+" : "") + String (amt, 1) + " st", { sx + 8, w.top - 12, 80, 12 }, 8.5f, Colour (0xffb98aff), Justification::centredLeft, 700);
        }
        // pastki ma'lumot qatori (Battery kabi)
        auto v = plugin.pad (sel);
        const String fileName = File (v["file"].toString()).getFileName();
        const double regionSec = w.regionMs / 1000.0;
        g.setColour (Colours::black.withAlpha (0.55f)); g.fillRect (wa.withTop (wa.getBottom() - 20).reduced (1, 0));
        label (g, "pad " + String (sel + 1) + ": " + (fileName.isNotEmpty() ? fileName : padName (sel)) + String::fromUTF8 ("   \xc2\xb7   oneshot length (seconds): ") + String (regionSec, 4),
               { wa.getX() + 8, wa.getBottom() - 19, wa.getWidth() - 330, 18 }, 8.5f, silver, Justification::centredLeft, 500);
        label (g, "START: " + String ((int) ((double) v["start"] * N)) + "   " + String (N / jmax (1.0, sr), 2) + " s",
               { wa.getRight() - 330, wa.getBottom() - 19, 168, 18 }, 8.5f, silver, Justification::centredRight, 600);
        const char* tabs[] = { "AMP ENV", "PITCH ENV" };
        for (int i = 0; i < 2; ++i)
        {
            auto t = envTabRect (i); const bool on = envTab == i;
            g.setColour (on ? (i == 0 ? Colour (0xff4aa8ff) : Colour (0xffb98aff)).withAlpha (0.35f) : Colours::white.withAlpha (0.06f)); g.fillRoundedRectangle (t, 3);
            g.setColour (Colours::white.withAlpha (on ? 0.5f : 0.15f)); g.drawRoundedRectangle (t, 3, 1);
            label (g, tabs[i], t, 8, on ? Colours::white : dim, Justification::centred, 700);
        }
    }
    int envNodeAt (juce::Point<float> pt) const
    {
        const auto w = waveGeo();
        if (envTab == 0) { const auto n = ampNodes (w); for (int i = 4; i >= 1; --i) if (n[(size_t) i].getDistanceFrom (pt) < 8) return i; }
        else { const auto n = pitchNodes (w); for (int i = 2; i >= 0; --i) if (n[(size_t) i].getDistanceFrom (pt) < 8) return i; }
        return -1;
    }
    void dragEnvNode (int node, juce::Point<float> pt)
    {
        const auto w = waveGeo(); auto v = plugin.pad (sel); auto* um = plugin.um();
        const double t = msAtX (w, pt.x);
        if (envTab == 0)
        {
            const double a = v["attack"], h = v.getProperty ("hold", 0.0);
            if (node == 1) v.setProperty ("attack", juce::jlimit (0.0, 200.0, t), um);
            if (node == 2) v.setProperty ("hold", juce::jlimit (0.0, 5000.0, t - a), um);
            if (node == 3)
            {
                v.setProperty ("env_dec", juce::jlimit (0.0, 5000.0, t - a - h), um);
                v.setProperty ("sustain", juce::jlimit (0.0, 1.0, (double) ((w.bot - pt.y) / (w.bot - w.top))), um);
            }
            if (node == 4) v.setProperty ("decay", juce::jlimit (0.0, 2000.0, w.regionMs - t), um);
            info = "A " + juce::String ((double) v["attack"], 1) + " ms  H " + juce::String ((double) v["hold"], 0) + " ms  D " + juce::String ((double) v["env_dec"], 0)
                 + " ms  S " + juce::String (juce::roundToInt ((double) v["sustain"] * 100)) + "%  R " + juce::String ((double) v["decay"], 0) + " ms";
        }
        else
        {
            const float mid = (w.top + w.bot) * 0.5f, half = (w.bot - w.top) * 0.5f;
            const double st = juce::jlimit (-48.0, 48.0, (double) ((mid - pt.y) / half * 48.0f));
            const double amt = v.getProperty ("pe_amt", 0.0);
            const double brk = juce::jlimit (0.01, 0.999, (double) v.getProperty ("pe_brk", 0.3)), d1 = v.getProperty ("pe_d1", 60.0);
            if (node == 0) v.setProperty ("pe_amt", std::round (st * 10.0) / 10.0, um);
            if (node == 1)
            {
                double nb = brk;
                if (std::abs (amt) > 0.05) nb = juce::jlimit (0.02, 0.98, st / amt);
                v.setProperty ("pe_brk", nb, um);
                v.setProperty ("pe_d1", juce::jlimit (1.0, 2000.0, juce::jmax (1.0, t) / -std::log (nb)), um);
            }
            if (node == 2) v.setProperty ("pe_d2", juce::jlimit (1.0, 5000.0, (t - (-d1 * std::log (brk))) / 3.0), um);
            info = "Pitch env: " + juce::String ((double) v["pe_amt"], 1) + " st  D1 " + juce::String ((double) v["pe_d1"], 0) + " ms  Break "
                 + juce::String (juce::roundToInt ((double) v["pe_brk"] * 100)) + "%  D2 " + juce::String ((double) v["pe_d2"], 0) + " ms";
        }
        refresh();
    }
    // ---------------------------------------------------------------- nom o'zgartirish (pad / shina)
    void startRename (int kind, int index, juce::Rectangle<float> r)
    {
        renameKind = kind; renameIdx = index;
        nameEd.setText (kind == 0 ? plugin.pad (index)["name"].toString() : busName (index), juce::dontSendNotification);
        nameEd.setBounds (r.toNearestInt().withWidth (juce::jmax (140, juce::roundToInt (r.getWidth()))).withHeight (24).constrainedWithin (getLocalBounds()));
        nameEd.setVisible (true); nameEd.toFront (true); nameEd.grabKeyboardFocus(); nameEd.selectAll();
    }
public:
    void renamePad (int p, const juce::String& n) { plugin.pad (p).setProperty ("name", n.trim().substring (0, 40), plugin.um()); repaint(); }
    void renameBus (int b, const juce::String& n) { plugin.state.setProperty ("busName" + juce::String (b), n.trim().isEmpty() ? "BUS " + juce::String (b) : n.trim().substring (0, 16), plugin.um()); repaint(); }
    juce::String getBusName (int b) const { return busName (b); }
    void setPadFaderForTest (int p, double db) { padF[p].setValue (db, juce::sendNotificationSync); }
    void commitRename()
    {
        if (! nameEd.isVisible()) return;
        nameEd.setVisible (false);
        if (renameKind == 0) renamePad (renameIdx, nameEd.getText()); else renameBus (renameIdx, nameEd.getText());
        grabKeyboardFocus();
    }

    void paintPitchEnv (juce::Graphics& g)
    {
        using namespace juce;
        auto pe = pePanel(); panelBg (g, pe, "PITCH ENVELOPE");
        auto v = plugin.pad (sel);
        const float amt = (float) (double) v.getProperty ("pe_amt", 0.0), d1 = (float) (double) v.getProperty ("pe_d1", 60.0),
                    brk = (float) (double) v.getProperty ("pe_brk", 0.3), d2 = (float) (double) v.getProperty ("pe_d2", 300.0);
        auto r = Rectangle<float> (pe.getX() + 10, pe.getY() + 26, pe.getWidth() - 20, 92);
        g.setColour (Colour (0xff09080b)); g.fillRoundedRectangle (r, 6);
        const float midY = r.getCentreY();
        g.setColour (Colours::white.withAlpha (0.12f)); g.drawHorizontalLine ((int) midY, r.getX(), r.getRight());
        // egri chiziq: decay1 -> break -> decay2
        const float total = jmax (50.0f, d1 * 2.5f + d2 * 2.0f);
        Path path; const float k1 = std::exp (-1.0f / jmax (0.001f, d1)), k2 = std::exp (-1.0f / jmax (0.001f, d2));
        float lvl = 1.0f; bool st2 = false; float brkX = -1;
        for (int i = 0; i <= (int) r.getWidth(); ++i)
        {
            const float tms = total * i / r.getWidth();
            if (i > 0) { const float dt = total / r.getWidth(); for (int q = 0; q < 4; ++q) { if (! st2) { lvl *= std::pow (k1, dt / 4); if (lvl <= brk) { st2 = true; brkX = r.getX() + i; } } else lvl *= std::pow (k2, dt / 4); } }
            const float y = midY - jlimit (-1.0f, 1.0f, amt / 48.0f) * lvl * (r.getHeight() * 0.45f);
            if (i == 0) path.startNewSubPath (r.getX(), y); else path.lineTo (r.getX() + i, y);
            ignoreUnused (tms);
        }
        const Colour c = std::abs (amt) > 0.01f ? Colour (0xffb98aff) : dim2;
        g.setColour (c); g.strokePath (path, PathStrokeType (1.8f));
        if (brkX > 0) { g.setColour (c.withAlpha (0.5f)); g.drawVerticalLine ((int) brkX, r.getY() + 4, r.getBottom() - 4); label (g, "break", { brkX + 3, r.getY() + 2, 40, 10 }, 7.5f, c, Justification::centredLeft, 600); }
        label (g, String (amt >= 0 ? "+" : "") + String (amt, 1) + " st", r.reduced (6, 3), 8.5f, c, Justification::topRight, 700);
        const char* kn[] = { "AMOUNT", "DECAY 1", "BREAK", "DECAY 2" };
        for (int i = 0; i < 4; ++i) caps (g, kn[i], { pe.getX() + 8 + i * 70.0f, pe.getY() + 124, 66, 10 }, 8, dim, Justification::centred);
    }
    // filtrning taxminiy amplituda xarakteristikasi (dB)
    static float filterResponseDb (int type, int slope, float fc, float res, float gainDb, float f)
    {
        if (type == 0) return 0.0f;
        const double w = f / juce::jmax (10.0f, fc);
        const double q = slope == 0 ? 0.707 : 0.5 + res * res * 11.5;
        std::complex<double> s (0.0, w);
        std::complex<double> h;
        if (slope == 0) { const auto lp = 1.0 / (s + 1.0); h = type == 1 ? lp : type == 2 ? s / (s + 1.0) : s / (s * s + s / 0.707 + 1.0); }
        else
        {
            const auto den = s * s + s / q + 1.0;
            h = type == 1 ? 1.0 / den : type == 2 ? s * s / den : (s / q) / den;
            if (slope == 2) h = h * h;
        }
        return (float) (20.0 * std::log10 (juce::jmax (1e-6, std::abs (h)))) + gainDb;
    }
    void paintFilter (juce::Graphics& g)
    {
        using namespace juce;
        auto fp = fltPanel(); panelBg (g, fp, "FILTER / EQ");
        auto v = plugin.pad (sel);
        const int type = (int) v.getProperty ("f_type", 0), slope = (int) v.getProperty ("f_slope", 1);
        const float fc = (float) (double) v.getProperty ("f_cut", 20000.0), res = (float) (double) v.getProperty ("f_res", 0.1), gdb = (float) (double) v.getProperty ("f_gain", 0.0);
        auto r = Rectangle<float> (fp.getX() + 10, fp.getY() + 72, fp.getWidth() - 20, 50);
        g.setColour (Colour (0xff09080b)); g.fillRoundedRectangle (r, 6);
        for (float f : { 100.0f, 1000.0f, 10000.0f })
        {
            const float x = r.getX() + r.getWidth() * (std::log10 (f) - std::log10 (20.0f)) / 3.0f;
            g.setColour (Colours::white.withAlpha (0.06f)); g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
        }
        const float zeroY = r.getY() + r.getHeight() * 0.35f;
        g.setColour (Colours::white.withAlpha (0.12f)); g.drawHorizontalLine ((int) zeroY, r.getX(), r.getRight());
        Path path;
        for (int i = 0; i <= (int) r.getWidth(); ++i)
        {
            const float f = 20.0f * std::pow (1000.0f, (float) i / r.getWidth());
            const float db = jlimit (-48.0f, 24.0f, filterResponseDb (type, slope, fc, res, gdb, f));
            const float y = jlimit (r.getY() + 1, r.getBottom() - 1, db > 0 ? zeroY - db / 24.0f * (zeroY - r.getY()) : zeroY - db / 48.0f * (r.getBottom() - zeroY));
            if (i == 0) path.startNewSubPath (r.getX(), y); else path.lineTo (r.getX() + i, y);
        }
        const Colour c = type > 0 ? Colour (0xffffc04a) : dim2;
        g.setColour (c); g.strokePath (path, PathStrokeType (1.8f));
        const char* kn[] = { "CUTOFF", "RESO", "GAIN" };
        const float kw = (fp.getWidth() - 16) / 3.0f;
        for (int i = 0; i < 3; ++i) caps (g, kn[i], { fp.getX() + 8 + i * kw, fp.getY() + 124, kw, 10 }, 8, dim, Justification::centred);
        label (g, slope == 0 ? "6 dB/okt" : slope == 1 ? "12 dB/okt" : "24 dB/okt", { fp.getRight() - 90, fp.getY() + 50, 80, 16 }, 8.5f, dim, Justification::centredRight, 600);
    }

    static juce::Colour beatColour (int i)
    {
        static const juce::Colour cs[] = { juce::Colour (0xffff3347), juce::Colour (0xffff9a4a), juce::Colour (0xffffc04a), juce::Colour (0xff3ddc97),
                                           juce::Colour (0xff4ad6ff), juce::Colour (0xff8ab4ff), juce::Colour (0xffb98aff), juce::Colour (0xffff8ad8) };
        return cs[(i / 2) % 8];
    }

    // ---------------------------------------------------------------- sichqoncha
    int padAt (juce::Point<float> p) const { for (int i = 0; i < beat::numPads; ++i) if (padRect (i).contains (p)) return i; return -1; }
    bool cellAt (juce::Point<float> pt, int& p, int& s) const
    {
        auto g = gridArea().withTrimmedLeft (110);
        if (! g.contains (pt)) return false;
        p = (int) ((pt.y - g.getY()) / rowH); s = (int) ((pt.x - g.getX()) / (g.getWidth() / plugin.length()));
        return p >= 0 && p < beat::numPads && s >= 0 && s < plugin.length();
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        dragMode = 0;
        if (const int p = padAt (e.position); p >= 0)
        {
            sel = p; refresh();
            if (e.mods.isPopupMenu()) padMenu (p); else plugin.previewPad (p, juce::jlimit (40, 127, (int) (127 - (e.position.y - padRect (p).getY()) * 0.6f)));
            return;
        }
        if (lane >= 0 && laneArea().contains (e.position))
        {
            plugin.edit.getUndoManager().beginNewTransaction ("AZ BEAT lane");
            if (e.mods.isAltDown()) { for (int st = 0; st < plugin.length(); ++st) plugin.setLane (sel, lane, st, beat::laneDef[lane], plugin.um()); repaint(); return; }
            dragMode = 4; lastLaneStep = -1; mouseDrag (e); return;
        }
        int p, s;
        if (cellAt (e.position, p, s))
        {
            sel = p; refresh();
            if (e.mods.isPopupMenu()) { rowMenu (p); return; }
            if (e.mods.isShiftDown()) { plugin.setRatchet (p, s, plugin.stepRatchet (p, s) % 4 + 1); if (plugin.stepVel (p, s) == 0) plugin.setStep (p, s, 100); repaint(); return; }
            const int v = plugin.stepVel (p, s);
            paintOn = v == 0; dragP = p; dragS = s; dragVel = v > 0 ? v : 100; dragMode = 1;
            plugin.edit.getUndoManager().beginNewTransaction ("AZ BEAT step");
            plugin.setStep (p, s, paintOn ? 100 : 0, plugin.um());
            if (paintOn) plugin.previewPad (p, 100);
            repaint(); return;
        }
        if (auto r = gridArea().withWidth (106); r.contains (e.position))
        {
            const int row = (int) ((e.position.y - r.getY()) / rowH);
            if (row >= 0 && row < beat::numPads) { sel = row; refresh(); if (e.mods.isPopupMenu()) rowMenu (row); else plugin.previewPad (row); }
            return;
        }
        // mikser: kanalni tanlash, M / S, chiqish shinasi
        for (int k = 0; k < numStrips; ++k)
        {
            if (! stripRect (k).contains (e.position)) continue;
            if (k >= beat::numPads) return;
            auto v = plugin.pad (k);
            if (sel != k) { sel = k; refresh(); }
            if (stripM (k).contains (e.position)) { v.setProperty ("mute", ! (bool) v["mute"], plugin.um()); refresh(); return; }
            if (stripS (k).contains (e.position)) { v.setProperty ("solo", ! (bool) v["solo"], plugin.um()); refresh(); return; }
            if (stripOut (k).contains (e.position))
            {
                juce::PopupMenu m; m.addSectionHeader (padName (k) + U (" â chiqish"));
                m.addItem (1, "Master", true, (int) v.getProperty ("bus", 0) == 0);
                for (int b = 1; b <= beat::numBuses; ++b) m.addItem (b + 1, busName (b), true, (int) v.getProperty ("bus", 0) == b);
                m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (stripOut (k).toNearestInt())), [this, k] (int r)
                { if (r > 0) { plugin.pad (k).setProperty ("bus", r - 1, plugin.um()); refresh(); } });
                return;
            }
            if (stripName (k).contains (e.position) && e.mods.isPopupMenu()) { startRename (0, k, stripName (k)); return; }
            if (stripName (k).contains (e.position)) plugin.previewPad (k);
            return;
        }
        auto wa = waveArea();
        if (wa.contains (e.position))
        {
            for (int i = 0; i < 2; ++i) if (envTabRect (i).contains (e.position)) { envTab = i; hoverNode = -1; repaint(); return; }
            if (const int n = envNodeAt (e.position); n >= 0)
            {
                plugin.edit.getUndoManager().beginNewTransaction ("AZ BEAT envelope");
                dragMode = 5; dragNode = n; return;
            }
        }
        if (wa.contains (e.position))
        {
            const float sx = wa.getX() + wa.getWidth() * (float) (double) plugin.pad (sel)["start"], ex = wa.getX() + wa.getWidth() * (float) (double) plugin.pad (sel)["end"];
            dragMode = std::abs (e.position.x - sx) < std::abs (e.position.x - ex) ? 2 : 3;
            mouseDrag (e);
        }
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragMode == 5) { dragEnvNode (dragNode, e.position); hoverNode = dragNode; repaint(); return; }
        if (dragMode == 4)
        {   // lane chizish (oraliq steplarni ham to'ldiradi)
            const int st = laneStepAt (e.position); if (st < 0) return;
            const int val = laneValueAt (e.position.y);
            if (lastLaneStep >= 0 && lastLaneStep != st)
            {
                const int a = juce::jmin (lastLaneStep, st), b = juce::jmax (lastLaneStep, st);
                for (int k = a; k <= b; ++k)
                {
                    const float t = b == a ? 1.0f : (float) (k - lastLaneStep) / (float) (st - lastLaneStep);
                    plugin.setLane (sel, lane, k, juce::roundToInt (lastLaneVal + (val - lastLaneVal) * t), plugin.um());
                }
            }
            else plugin.setLane (sel, lane, st, val, plugin.um());
            lastLaneStep = st; lastLaneVal = val; hoverStep = st;
            info = juce::String (beat::laneNames[lane]) + "  step " + juce::String (st + 1) + ": " + laneValueText (lane, val);
            repaint(); return;
        }
        if (dragMode == 1)
        {
            int p, s;
            if (std::abs (e.getDistanceFromDragStartY()) > 4 && std::abs (e.getDistanceFromDragStartX()) < 6 && plugin.stepVel (dragP, dragS) > 0)
            {   // velocity (vertikal)
                plugin.setStep (dragP, dragS, juce::jlimit (1, 127, dragVel - e.getDistanceFromDragStartY() * 2));
                info = "Velocity: " + juce::String (plugin.stepVel (dragP, dragS));
            }
            else if (cellAt (e.position, p, s) && p == dragP && s != dragS)
            {   // gorizontal sudrash = bo'yash
                plugin.setStep (p, s, paintOn ? 100 : 0, plugin.um()); dragS = s;
            }
            repaint(); return;
        }
        if (dragMode == 2 || dragMode == 3)
        {
            auto wa = waveArea();
            const double x = juce::jlimit (0.0, 1.0, (double) (e.position.x - wa.getX()) / wa.getWidth());
            auto v = plugin.pad (sel);
            if (dragMode == 2) v.setProperty ("start", juce::jmin (x, (double) v["end"] - 0.002), nullptr);
            else v.setProperty ("end", juce::jmax (x, (double) v["start"] + 0.002), nullptr);
            repaint();
        }
    }
    void mouseUp (const juce::MouseEvent&) override { if (dragMode == 2 || dragMode == 3 || dragMode == 5) plugin.previewPad (sel); dragMode = 0; }
    void mouseMove (const juce::MouseEvent& e) override
    {
        const int h = lane >= 0 && laneArea().contains (e.position) ? laneStepAt (e.position) : -1;
        if (h != hoverStep) { hoverStep = h; repaint(); }
        const int hn = waveArea().contains (e.position) ? envNodeAt (e.position) : -1;
        if (hn != hoverNode) { hoverNode = hn; repaint (waveArea().expanded (4).toNearestInt()); }
        setMouseCursor (hn >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
    }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (lane >= 0 && laneArea().contains (e.position))
            if (const int st = laneStepAt (e.position); st >= 0) { plugin.setLane (sel, lane, st, beat::laneDef[lane], plugin.um()); repaint(); return; }
        // nomlarni o'zgartirish: pad, sekvenser qatori, mikser kanali, shina
        if (const int p = padAt (e.position); p >= 0)
        { auto r = padRect (p); startRename (0, p, r.withTrimmedTop (r.getHeight() - 26).withHeight (24)); return; }
        if (auto r = gridArea().withWidth (106); r.contains (e.position))
        {
            const int row = (int) ((e.position.y - r.getY()) / rowH);
            if (row >= 0 && row < beat::numPads) startRename (0, row, r.withY (r.getY() + row * rowH).withHeight (rowH));
            return;
        }
        for (int k = 0; k < numStrips - 2; ++k)
            if (stripName (k).contains (e.position))
            {
                if (k < beat::numPads) startRename (0, k, stripName (k)); else startRename (1, k - beat::numPads + 1, stripName (k));
                return;
            }
        // envelope nuqtasini standart holatga qaytarish
        if (const int n = envNodeAt (e.position); n >= 0)
        {
            auto v = plugin.pad (sel); auto* um = plugin.um();
            if (envTab == 0) { if (n == 1) v.setProperty ("attack", 0.0, um); if (n == 2) v.setProperty ("hold", 0.0, um); if (n == 3) { v.setProperty ("env_dec", 0.0, um); v.setProperty ("sustain", 1.0, um); } if (n == 4) v.setProperty ("decay", 30.0, um); }
            else { if (n == 0) v.setProperty ("pe_amt", 0.0, um); if (n == 1) { v.setProperty ("pe_d1", 60.0, um); v.setProperty ("pe_brk", 0.3, um); } if (n == 2) v.setProperty ("pe_d2", 300.0, um); }
            refresh();
        }
    }
    // sichqoncha g'ildiragi: step velocity / lane qiymati / pad gain
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (e.mods.isCommandDown()) return;          // Ctrl+g'ildirak = oyna zoomi (PluginFrame)
        const float dy = (std::abs (w.deltaY) > std::abs (w.deltaX) ? w.deltaY : -w.deltaX) * (w.isReversed ? -1.0f : 1.0f);
        if (dy == 0) return;
        const int stepAmt = e.mods.isShiftDown() ? 1 : 4;
        const int d = dy > 0 ? stepAmt : -stepAmt;
        if (lane >= 0 && laneArea().contains (e.position))
        {
            if (const int st = laneStepAt (e.position); st >= 0)
            {
                const int nv = juce::jlimit (0, 127, plugin.laneVal (sel, lane, st) + d);
                plugin.setLane (sel, lane, st, nv, plugin.um()); hoverStep = st;
                info = juce::String (beat::laneNames[lane]) + "  step " + juce::String (st + 1) + ": " + laneValueText (lane, nv); repaint();
            }
            return;
        }
        int p, s;
        if (lane < 0 && cellAt (e.position, p, s))
        {
            if (const int v = plugin.stepVel (p, s); v > 0)
            {
                plugin.setStep (p, s, juce::jlimit (1, 127, v + d), plugin.um());
                info = "Velocity: " + juce::String (plugin.stepVel (p, s)); repaint();
            }
            return;
        }
        if (const int pp = padAt (e.position); pp >= 0)
        {
            auto v = plugin.pad (pp);
            const double ng = juce::jlimit (-36.0, 12.0, (double) v["gain"] + (dy > 0 ? 0.5 : -0.5) * (e.mods.isShiftDown() ? 0.2 : 1.0));
            v.setProperty ("gain", ng, plugin.um());
            if (pp == sel) gainK.setValue (ng, juce::dontSendNotification);
            padF[pp].setValue (ng, juce::dontSendNotification);
            info = "Pad " + juce::String (pp + 1) + " gain: " + juce::String (ng, 1) + " dB"; repaint();
            return;
        }
        Component::mouseWheelMove (e, w);
    }

    void padMenu (int p)
    {
        juce::PopupMenu m; m.addSectionHeader ("Pad " + juce::String (p + 1));
        m.addItem (1, U ("Sample yuklash...")); m.addItem (2, U ("Padni tozalash")); m.addSeparator();
        m.addItem (3, U ("Nusxa olish")); m.addItem (4, U ("Qo'yish"), copied.isValid());
        m.addSeparator(); m.addItem (5, U ("Nomini o'zgartirish..."));
        m.showMenuAsync ({}, [this, p] (int r)
        {
            if (r == 5) { auto rr = padRect (p); startRename (0, p, rr.withTrimmedTop (rr.getHeight() - 26).withHeight (24)); return; }
            if (r == 1) loadSmpB.triggerClick();
            if (r == 2) plugin.clearPad (p);
            if (r == 3) copied = plugin.pad (p).createCopy();
            if (r == 4 && copied.isValid()) { auto v = plugin.pad (p); const auto steps = v["steps"]; v.copyPropertiesFrom (copied, plugin.um()); v.setProperty ("idx", p, nullptr); v.setProperty ("steps", steps, nullptr); }
            refresh();
        });
    }
    void rowMenu (int p)
    {
        juce::PopupMenu m; m.addSectionHeader (U ("Qatorni to'ldirish: Pad ") + juce::String (p + 1));
        m.addItem (1, U ("Har 1/4 (4 stepda)")); m.addItem (2, U ("Har 1/8 (2 stepda)")); m.addItem (3, U ("Har step"));
        m.addItem (4, U ("Off-beat (3, 7, 11 ...)")); m.addItem (5, U ("Tasodifiy (Nerve random)")); m.addSeparator(); m.addItem (6, U ("Qatorni tozalash"));
        m.showMenuAsync ({}, [this, p] (int r)
        {
            if (r == 0) return;
            juce::Random rnd;
            plugin.edit.getUndoManager().beginNewTransaction ("AZ BEAT fill");
            for (int s = 0; s < plugin.length(); ++s)
            {
                int v = 0;
                if (r == 1) v = s % 4 == 0 ? 110 : 0;
                if (r == 2) v = s % 2 == 0 ? (s % 4 == 0 ? 110 : 80) : 0;
                if (r == 3) v = s % 4 == 0 ? 110 : s % 2 == 0 ? 85 : 65;
                if (r == 4) v = s % 4 == 2 ? 100 : 0;
                if (r == 5) v = rnd.nextFloat() < (s % 4 == 0 ? 0.6f : 0.25f) ? 60 + rnd.nextInt (60) : 0;
                plugin.setStep (p, s, v, plugin.um());
            }
            repaint();
        });
    }

    // ---------------------------------------------------------------- fayllarni sudrab tashlash
    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void fileDragMove (const juce::StringArray&, int x, int y) override { dropHint = padAt ({ (float) x, (float) y }); repaint(); }
    void fileDragExit (const juce::StringArray&) override { dropHint = -1; repaint(); }
    void filesDropped (const juce::StringArray& files, int x, int y) override { dropHint = -1; dropFiles (files, x, y); }
    bool isInterestedInDragSource (const SourceDetails& d) override { return dragFile (d) != juce::File(); }
    void itemDragMove (const SourceDetails& d) override { dropHint = padAt (d.localPosition.toFloat()); repaint(); }
    void itemDragExit (const SourceDetails&) override { dropHint = -1; repaint(); }
    void itemDropped (const SourceDetails& d) override { dropHint = -1; dropFiles ({ dragFile (d).getFullPathName() }, d.localPosition.x, d.localPosition.y); }
    static juce::File dragFile (const SourceDetails& d)
    {
        const auto s = d.description.toString();
        if (s.startsWith ("file:")) return juce::File (s.fromFirstOccurrenceOf ("file:", false, false));
        if (auto* ft = dynamic_cast<juce::FileTreeComponent*> (d.sourceComponent.get())) return ft->getSelectedFile();
        return {};
    }
    void dropFiles (const juce::StringArray& files, int x, int y)
    {
        int p = padAt ({ (float) x, (float) y });
        if (p < 0) { int pp, s; if (cellAt ({ (float) x, (float) y }, pp, s)) p = pp; }
        if (p < 0 && waveArea().contains ((float) x, (float) y)) p = sel;
        if (p < 0 && files.size() == 1) { doSlice (juce::File (files[0])); return; }   // bo'sh joyga: loopni bo'lish
        if (p < 0) p = sel;
        for (auto& f : files) { if (p >= beat::numPads) break; plugin.loadSample (p++, juce::File (f)); }
        sel = juce::jlimit (0, beat::numPads - 1, p - 1); refresh();
        info = U ("Sample yuklandi: ") + juce::File (files[0]).getFileName();
    }
    // ---- papkadagi sample'larni ketma-ket ko'rish (+ moslik bo'yicha)
    static juce::Array<juce::File> folderSamples (const juce::File& f)
    {
        auto fs = f.getParentDirectory().findChildFiles (juce::File::findFiles, false, "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
        struct Cmp { int compareElements (const juce::File& a, const juce::File& b) const { return a.getFileName().compareNatural (b.getFileName()); } } cmp;
        fs.sort (cmp);
        return fs;
    }
    void selectPadForTest (int p) { sel = p; refresh(); }
    void browse (int dir, bool onlyMatching)
    {
        juce::File cur (plugin.pad (sel)["file"].toString());
        if (! cur.existsAsFile()) { loadSmpB.triggerClick(); return; }
        auto fs = folderSamples (cur);
        int i = fs.indexOf (cur);
        if (fs.isEmpty()) return;
        for (int tries = 0; tries < fs.size(); ++tries)
        {
            i = (i + dir + fs.size()) % fs.size();
            if (! onlyMatching || matchFor (fs[i]).score == 2) break;
        }
        plugin.loadSample (sel, fs[i]);
        refresh(); plugin.previewPad (sel);
        info = juce::String (i + 1) + " / " + juce::String (fs.size()) + juce::String::fromUTF8 ("  \xc2\xb7  ") + fs[i].getFileName();
    }
    sync::Match matchFor (const juce::File& f)
    {
        const auto key = f.getFullPathName() + "|" + juce::String (projBpm(), 2) + "|" + projKeyString();
        if (auto it = matchCache.find (key); it != matchCache.end()) return it->second;
        auto m = sync::matchToProject (plugin.edit.engine, f, projBpm(), sync::keyFromString (projKeyString()));
        matchCache[key] = m; return m;
    }
    double projBpm() const { return plugin.edit.tempoSequence.getTempo (0)->getBpm(); }
    juce::String projKeyString() const { return plugin.edit.state.getProperty ("azKey", "A minor").toString(); }
    void updateMatch()
    {
        juce::File f (plugin.pad (sel)["file"].toString());
        if (! f.existsAsFile()) { matchText = {}; return; }
        auto m = matchFor (f);
        matchText = juce::String::fromUTF8 ("Loyiha (") + juce::String (projBpm(), 0) + " BPM, " + projKeyString() + "):  " + m.text; matchScore = m.score;
    }
    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k.getKeyCode() == juce::KeyPress::leftKey)  { browse (-1, k.getModifiers().isShiftDown()); return true; }
        if (k.getKeyCode() == juce::KeyPress::rightKey) { browse (+1, k.getModifiers().isShiftDown()); return true; }
        return false;
    }
    void doSlice (const juce::File& f) { info = U ("Slice: ") + plugin.sliceLoop (f, plugin.edit.tempoSequence.getTempo (0)->getBpm()); sel = 0; refresh(); }

    void patternToMidi()
    {
        te::AudioTrack* t = nullptr;
        for (auto* at : te::getAudioTracks (plugin.edit)) if (at->pluginList.indexOf (&plugin) >= 0) t = at;
        if (t == nullptr) return;
        auto& ts = plugin.edit.tempoSequence;
        const double rb = beat::rateBeats ((int) plugin.state["rate"]);
        const auto start = plugin.edit.getTransport().getPosition();
        const auto sb = ts.toBeats (start);
        const auto end = ts.toTime (sb + te::BeatDuration::fromBeats (plugin.length() * rb));
        plugin.edit.getUndoManager().beginNewTransaction ("Pattern -> MIDI");
        auto mc = t->insertMIDIClip (U ("AZ BEAT pattern"), { start, end }, nullptr);
        if (mc == nullptr) return;
        const double swing = (double) plugin.state["swing"];
        for (int p = 0; p < beat::numPads; ++p)
            for (int s = 0; s < plugin.length(); ++s)
                if (const int v = plugin.stepVel (p, s); v > 0)
                {
                    const int rat = plugin.stepRatchet (p, s);
                    for (int r = 0; r < rat; ++r)
                        mc->getSequence().addNote (beat::firstNote + p, te::BeatPosition::fromBeats (juce::jmax (0.0, s * rb + (s % 2 ? swing * rb * 0.5 : 0) + r * rb / rat
                                                                                           + (double) plugin.pad (p)["delay"] * 0.001 * ts.getTempo (0)->getBpm() / 60.0)),
                                                   te::BeatDuration::fromBeats (rb / rat * 0.9), juce::jmax (1, v - r * 12), 0, &plugin.edit.getUndoManager());
                }
        plugin.state.setProperty ("seqOn", false, plugin.um()); refresh();
        info = U ("Pattern MIDI klipga ko'chirildi (ichki SEQ o'chirildi \xe2\x80\x94 ikki marta chalmasligi uchun)");
    }

private:
    void timerCallback() override { if (++ticks % 8 == 0 || plugin.padPos[sel].load() >= 0.0f || ticks % 3 == 0 || plugin.playStep.load() != lastStep || juce::Time::getMillisecondCounter() - lastHitPaint < 300) { lastStep = plugin.playStep.load(); repaint(); }
                                    for (auto& h : plugin.padHit) if (juce::Time::getMillisecondCounter() - h.load() < 250) lastHitPaint = juce::Time::getMillisecondCounter(); }
    void chooseFile (const juce::String& title, const juce::String& pattern, bool save, std::function<void (juce::File)> cb)
    {
        chooser = std::make_unique<juce::FileChooser> (title, juce::File::getSpecialLocation (juce::File::userMusicDirectory), pattern);
        const int fl = (save ? juce::FileBrowserComponent::saveMode : juce::FileBrowserComponent::openMode) | juce::FileBrowserComponent::canSelectFiles;
        chooser->launchAsync (fl, [cb] (const juce::FileChooser& fc) { if (fc.getResult() != juce::File()) cb (fc.getResult()); });
    }
    AzBeatPlugin& plugin; int sel = 0, dragMode = 0, dragP = 0, dragS = 0, dragVel = 100, lastStep = -2, dropHint = -1; bool paintOn = true;
    juce::uint32 lastHitPaint = 0; int ticks = 0;
    juce::TextButton loadKitB, saveKitB, defaultB, sliceB, toMidiB, seqB, clearB, humanB, loadSmpB, revB, syncB, muteB, soloB;
    juce::ComboBox lenBox, rateBox, chokeBox;
    // Ctrl+g'ildirak — oyna zoomi uchun: knob/fader qiymatini o'zgartirmaydi
    struct WSlider : juce::Slider
    {
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override { if (! e.mods.isCommandDown()) juce::Slider::mouseWheelMove (e, w); }
    };
    WSlider gainK, panK, tuneK, fineK, atkK, decK, dlyK, swingK, masterK;
    WSlider busF[beat::numBuses], fxF, peAmtK, peD1K, peBrkK, peD2K, fCutK, fResK, fGainK, holdK, envDecK, susK;
    WSlider padF[beat::numPads];
    juce::TextEditor nameEd; int renameKind = 0, renameIdx = 0;
    int envTab = 0, hoverNode = -1, dragNode = -1;
    juce::OwnedArray<juce::TextButton> laneTabs, fTypeB, fSlopeB; juce::ComboBox busBox;
    int lane = -1, hoverStep = -1, lastLaneStep = -1, lastLaneVal = 64;
public:
    void demoLanes()   // skrinshot uchun: cutoff lane egri, filtr va pitch env namunasi
    {
        sel = 0;
        for (int st = 0; st < plugin.length(); ++st) plugin.setLane (0, beat::laneCut, st, 40 + (int) (80 * (0.5 + 0.5 * std::sin (st * 0.45))));
        auto v = plugin.pad (0); v.setProperty ("f_type", 1, nullptr); v.setProperty ("f_slope", 2, nullptr); v.setProperty ("f_cut", 1800.0, nullptr); v.setProperty ("f_res", 0.45, nullptr);
        v.setProperty ("pe_amt", 18.0, nullptr); v.setProperty ("pe_d1", 40.0, nullptr); v.setProperty ("pe_brk", 0.35, nullptr); v.setProperty ("pe_d2", 260.0, nullptr); v.setProperty ("bus", 1, nullptr);
        plugin.pad (2).setProperty ("bus", 2, nullptr); plugin.pad (3).setProperty ("bus", 3, nullptr);
        v.setProperty ("hold", 25.0, nullptr); v.setProperty ("env_dec", 90.0, nullptr); v.setProperty ("sustain", 0.45, nullptr); v.setProperty ("decay", 60.0, nullptr);
        renameBus (1, "Barabanlar"); renameBus (2, "Perkussiya");
        refresh();
    }
    void setLaneForTest (int l) { laneTabs[l + 1]->setToggleState (true, juce::sendNotification); lane = l; repaint(); }
private:
    juce::TextButton normB, prevB, nextB;
    std::map<juce::String, sync::Match> matchCache; juce::String matchText; int matchScore = 2;
    juce::ValueTree copied; juce::String info;
    std::unique_ptr<juce::FileChooser> chooser;
};

inline std::unique_ptr<te::Plugin::EditorComponent> AzBeatPlugin::createEditor() { return std::make_unique<BeatEditor> (*this); }
}
