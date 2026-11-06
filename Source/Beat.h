// =============================================================================
//  AZ MUS — AZ BEAT: beat yaratish treki
//  · Battery tuzilmasi: 4×4 = 16 pad (C1–D#2), har padda sample, gain/pan/tune, start/end, attack/decay,
//    reverse, choke guruhi, mute/solo, temp bilan sinxron loop
//  · Xfer Nerve mexanizmi: har pad uchun step-sekvenser (velocity, ratchet), swing, uzunlik 16/32/64,
//    loopni transient bo'yicha 16 padga bo'lish (Slice) + patternni avtomatik yozish, Pattern → MIDI
// =============================================================================
#pragma once
#include "SampleSync.h"
#include "Theme.h"

namespace az
{
namespace beat
{
constexpr int numPads = 16, maxSteps = 64, firstNote = 36;
inline const juce::Identifier PAD ("PAD");
inline const char* rateNames[] = { "1/16", "1/8", "1/32", "1/16T" };
inline double rateBeats (int r) { return r == 1 ? 0.5 : r == 2 ? 0.125 : r == 3 ? 0.25 * 2.0 / 3.0 : 0.25; }

struct PadRT
{
    std::shared_ptr<const juce::AudioBuffer<float>> sample; double sampleRate = 48000;
    float gain = 1, pan = 0, tune = 0, start = 0, end = 1, attackMs = 0, decayMs = 30, delayMs = 0;
    bool reverse = false, mute = false, solo = false; int choke = 0;
    juce::uint8 steps[maxSteps] {}, ratchet[maxSteps] {};
};
struct Snapshot
{
    PadRT pads[numPads];
    bool seqOn = true, anySolo = false; int length = 16, rate = 0; float swing = 0, master = 1;
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
                            "808", "Shaker", "Crash", "Perc", "Snap", "Kick 2", "Snare 2", "Ride" };
    juce::Array<juce::File> files;
    juce::Random rnd (7);
    for (int p = 0; p < numPads; ++p)
    {
        auto f = dir.getChildFile (juce::String (p + 1).paddedLeft ('0', 2) + " " + names[p] + ".wav");
        files.add (f);
        if (f.existsAsFile()) continue;
        const int sr = 48000; const double dur = p == 4 || p == 10 || p == 15 ? 1.1 : p == 8 ? 1.4 : 0.6;
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
        for (int i = 0; i < beat::numPads; ++i) pad (i);
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

    void initialise (const te::PluginInitialisationInfo& info) override { outRate = info.sampleRate > 0 ? info.sampleRate : 48000.0; for (auto& v : voices) v.active = false; }
    void deinitialise() override {}
    void reset() override { for (auto& v : voices) v.active = false; }

    // ---- model (ValueTree; xabar oqimida)
    juce::ValueTree pad (int i)
    {
        for (auto c : state) if (c.hasType (beat::PAD) && (int) c["idx"] == i) return c;
        juce::ValueTree p (beat::PAD);
        p.setProperty ("idx", i, nullptr); p.setProperty ("file", "", nullptr); p.setProperty ("name", "", nullptr);
        for (auto [k, v] : std::initializer_list<std::pair<const char*, double>> { { "gain", 0 }, { "pan", 0 }, { "tune", 0 }, { "fine", 0 }, { "start", 0 }, { "end", 1 }, { "attack", 0 }, { "decay", 30 }, { "delay", 0 } })
            p.setProperty (k, v, nullptr);
        for (auto k : { "reverse", "mute", "solo", "sync", "norm" }) p.setProperty (k, false, nullptr);
        p.setProperty ("choke", 0, nullptr);
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
            const double maxShift = 0.25 * bpm / 60.0;          // delay: +-250 ms
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
                            const double at = base + swing + r * rb / rat + sn->pads[p].delayMs * 0.001 * bpm / 60.0;
                            if (at < b0 || at >= b1) continue;
                            trigger (*sn, p, juce::jmax (1, vel - r * 12), juce::jlimit (0, n - 1, (int) ((at - b0) * 60.0 / bpm * outRate)));
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
            p.reverse = v["reverse"]; p.mute = v["mute"]; p.solo = v["solo"]; p.choke = v["choke"];
            s->anySolo |= p.solo;
            auto st = intList (v["steps"], 0), ra = intList (v["ratchet"], 1);
            for (int k = 0; k < beat::maxSteps; ++k) { p.steps[k] = (juce::uint8) juce::jlimit (0, 127, st[k]); p.ratchet[k] = (juce::uint8) juce::jlimit (1, 4, ra[k]); }
        }
        const juce::SpinLock::ScopedLockType l (snapLock);
        snap = s;
    }

    struct Voice
    {
        bool active = false, releasing = false;
        int pad = 0, delay = 0; double pos = 0, inc = 1, a = 0, b = 0; float gl = 1, gr = 1, env = 0, atkInc = 1, decStartGain = 1, relInc = 0.01f;
        int decayLen = 0; bool reverse = false;
        std::shared_ptr<const juce::AudioBuffer<float>> buf;
    };
    void trigger (const beat::Snapshot& s, int p, int vel, int offset)
    {
        if (p < 0 || p >= beat::numPads) return;
        const auto& pd = s.pads[p];
        if (pd.sample == nullptr || pd.sample->getNumSamples() < 2 || pd.mute || (s.anySolo && ! pd.solo)) return;
        if (pd.choke > 0) for (auto& v : voices) if (v.active && s.pads[v.pad].choke == pd.choke) { v.releasing = true; v.relInc = 1.0f / (0.004f * (float) outRate); }
        Voice* v = nullptr;
        for (auto& x : voices) if (! x.active) { v = &x; break; }
        if (v == nullptr) { v = &voices[(size_t) (nextSteal++ % voices.size())]; }
        const int N = pd.sample->getNumSamples();
        v->a = juce::jlimit (0.0, (double) N - 2, (double) pd.start * N);
        v->b = juce::jlimit (v->a + 1, (double) N - 1, (double) pd.end * N);
        v->reverse = pd.reverse; v->pos = v->reverse ? v->b : v->a;
        v->inc = pd.sampleRate / outRate * std::pow (2.0, pd.tune / 12.0);
        const float g = pd.gain * s.master * std::pow (juce::jlimit (1, 127, vel) / 127.0f, 1.2f);
        const float pan = juce::jlimit (-1.0f, 1.0f, pd.pan);
        v->gl = g * std::cos ((pan + 1) * juce::MathConstants<float>::pi * 0.25f) * 1.4142f;
        v->gr = g * std::sin ((pan + 1) * juce::MathConstants<float>::pi * 0.25f) * 1.4142f;
        v->atkInc = pd.attackMs > 0.1f ? 1.0f / (pd.attackMs * 0.001f * (float) outRate) : 1.0f;
        v->env = pd.attackMs > 0.1f ? 0.0f : 1.0f;
        v->decayLen = (int) (pd.decayMs * 0.001 * outRate * v->inc);   // manba namunalarida
        v->releasing = false; v->relInc = 0.0f; v->pad = p; v->delay = offset; v->buf = pd.sample; v->active = true;
        padHit[p] = juce::Time::getMillisecondCounter();
    }
    void render (const beat::Snapshot&, juce::AudioBuffer<float>& out, int s0, int n)
    {
        auto* L = out.getWritePointer (0, s0);
        auto* R = out.getNumChannels() > 1 ? out.getWritePointer (1, s0) : nullptr;
        for (auto& v : voices)
        {
            if (! v.active) continue;
            const auto& b = *v.buf;
            const float* c0 = b.getReadPointer (0); const float* c1 = b.getReadPointer (juce::jmin (1, b.getNumChannels() - 1));
            for (int i = juce::jmax (0, v.delay); i < n; ++i)
            {
                const int ip = (int) v.pos; const float fr = (float) (v.pos - ip);
                if (ip < 0 || ip + 1 >= b.getNumSamples()) { v.active = false; break; }
                float xl = c0[ip] + (c0[ip + 1] - c0[ip]) * fr, xr = c1[ip] + (c1[ip + 1] - c1[ip]) * fr;
                // oxirida decay (fade)
                const double remain = v.reverse ? v.pos - v.a : v.b - v.pos;
                float fade = 1.0f;
                if (v.decayLen > 0 && remain < v.decayLen) fade = (float) (remain / v.decayLen);
                if (v.env < 1.0f && ! v.releasing) v.env = juce::jmin (1.0f, v.env + v.atkInc);
                if (v.releasing) { v.env -= v.relInc > 0 ? v.relInc : 0.01f; if (v.env <= 0) { v.active = false; break; } }
                const float g = v.env * fade;
                L[i] += xl * v.gl * g; if (R) R[i] += xr * v.gr * g;
                v.pos += v.reverse ? -v.inc : v.inc;
                if (v.reverse ? v.pos <= v.a : v.pos >= v.b) { v.active = false; break; }
            }
            v.delay = juce::jmax (0, v.delay - n);
        }
    }

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
        sliceB.setTooltip (U ("Loopni transientlar bo'yicha 16 padga bo'lish va patternni avtomatik yozish (Nerve)"));
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
        dlyK.setTooltip (U ("Delay: sample'ni kechiktirish (+) yoki ertaroq chalish (\xe2\x88\x92), millisekundda")); knob (swingK, 0, 75, 1, "%"); knob (masterK, -24, 6, 0.1, " dB");
        panK.getProperties().set ("bipolar", true); tuneK.getProperties().set ("bipolar", true); fineK.getProperties().set ("bipolar", true);
        gainK.setDoubleClickReturnValue (true, 0); panK.setDoubleClickReturnValue (true, 0); tuneK.setDoubleClickReturnValue (true, 0); fineK.setDoubleClickReturnValue (true, 0);

        auto bindPad = [this] (juce::Slider& s, const char* id) { s.onValueChange = [this, &s, id] { plugin.pad (sel).setProperty (id, s.getValue(), plugin.um()); repaint(); }; };
        bindPad (gainK, "gain"); bindPad (panK, "pan"); bindPad (tuneK, "tune"); bindPad (fineK, "fine"); bindPad (atkK, "attack"); bindPad (decK, "decay"); bindPad (dlyK, "delay");
        swingK.onValueChange = [this] { plugin.state.setProperty ("swing", swingK.getValue() / 100.0, plugin.um()); };
        masterK.onValueChange = [this] { plugin.state.setProperty ("master", masterK.getValue(), plugin.um()); };
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
        refresh();
        setSize (1180, 700);
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
        repaint();
    }

    // ---------------------------------------------------------------- joylashuv
    juce::Rectangle<float> padRect (int i) const
    {
        const int row = 3 - i / 4, col = i % 4;              // Battery: 1-pad pastki chapda (MPC kabi)
        const float s = 86, g = 8;
        return { 16 + col * (s + g), 64 + row * (s + g), s, s };
    }
    juce::Rectangle<float> gridArea() const { return { 420, 64, (float) getWidth() - 436, 16 * 20.0f }; }
    juce::Rectangle<float> cell (int p, int s) const
    {
        auto g = gridArea().withTrimmedLeft (110);
        const float w = g.getWidth() / plugin.length();
        return { g.getX() + s * w, g.getY() + p * 20.0f, w, 20.0f };
    }
    juce::Rectangle<float> waveArea() const { return { 16, 470, 560, 150 }; }

    void resized() override
    {
        auto top = juce::Rectangle<int> (200, 14, getWidth() - 214, 30);
        loadKitB.setBounds (top.removeFromLeft (64)); top.removeFromLeft (4); saveKitB.setBounds (top.removeFromLeft (70)); top.removeFromLeft (4);
        defaultB.setBounds (top.removeFromLeft (66)); top.removeFromLeft (16);
        sliceB.setBounds (top.removeFromLeft (110)); top.removeFromLeft (4); toMidiB.setBounds (top.removeFromLeft (120)); top.removeFromLeft (16);
        seqB.setBounds (top.removeFromLeft (52)); top.removeFromLeft (4); lenBox.setBounds (top.removeFromLeft (88)); top.removeFromLeft (4);
        rateBox.setBounds (top.removeFromLeft (70)); top.removeFromLeft (4); clearB.setBounds (top.removeFromLeft (76)); top.removeFromLeft (4);
        humanB.setBounds (top.removeFromLeft (86));
        // pad paneli (pastki)
        const int y = 470;
        int x = 596;
        for (auto* k : { &gainK, &panK, &tuneK, &fineK, &atkK, &decK, &dlyK }) { k->setBounds (x, y + 6, 58, 78); x += 60; }
        x = 596;
        loadSmpB.setBounds (x, y + 98, 90, 26); revB.setBounds (x + 96, y + 98, 80, 26); syncB.setBounds (x + 182, y + 98, 62, 26);
        chokeBox.setBounds (x + 250, y + 98, 96, 26); muteB.setBounds (x + 352, y + 98, 30, 26); soloB.setBounds (x + 386, y + 98, 30, 26);
        normB.setBounds (x, y + 130, 120, 24);
        prevB.setBounds (16, y - 22, 26, 18); nextB.setBounds (46, y - 22, 26, 18);
        swingK.setBounds (getWidth() - 152, y + 6, 64, 78); masterK.setBounds (getWidth() - 80, y + 6, 64, 78);
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
        caps (g, "SEQUENCER" + dot + String (L) + " step" + dot + String (beat::rateNames[jlimit (0, 3, (int) plugin.state["rate"])]), { ga.getX(), 48, 400, 12 }, 8.5f);
        for (int p = 0; p < beat::numPads; ++p)
        {
            auto v = plugin.pad (p);
            auto row = Rectangle<float> (ga.getX(), ga.getY() + p * 20.0f, 106, 18);
            g.setColour (p == sel ? beatColour (p).withAlpha (0.25f) : Colours::white.withAlpha (0.03f)); g.fillRoundedRectangle (row, 4);
            label (g, String (p + 1) + "  " + (v["name"].toString().isEmpty() ? String::fromUTF8 ("\xe2\x80\x94") : v["name"].toString()), row.reduced (6, 0), 9, (bool) v["mute"] ? dim2 : text, Justification::centredLeft, 500);
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
                    if (ra[s] > 1) label (g, "x" + String (ra[s]), c, 8, Colours::white, Justification::centred, 700);
                }
            }
        }
        if (ps >= 0 && ps < L) { auto c = cell (0, ps); g.setColour (Colours::white.withAlpha (0.5f)); g.fillRect (c.getX(), ga.getY() - 5, c.getWidth(), 3.0f); }
        label (g, U ("Bosish: qo'yish/olish \xc2\xb7 vertikal sudrash: velocity \xc2\xb7 Shift+bosish: ratchet x2/x3/x4 \xc2\xb7 o'ng tugma: qatorni to'ldirish"),
               { ga.getX(), ga.getBottom() + 6, ga.getWidth(), 14 }, 9, dim2, Justification::centredLeft, 400);
        // tanlangan pad: to'lqin + start/end
        auto wa = waveArea();
        caps (g, "PAD " + String (sel + 1) + String::fromUTF8 ("  \xc2\xb7  ") + plugin.pad (sel)["name"].toString(), { wa.getX() + 64, wa.getY() - 18, 300, 12 }, 8.5f, beatColour (sel));
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
            label (g, String (sr > 0 ? N / sr : 0.0, 2) + " s", wa.reduced (8, 4), 9, dim, Justification::bottomRight, 500);
        }
        else label (g, U ("Sample yo'q \xe2\x80\x94 media brauzerdan sudrab tashlang yoki \"Sample...\""), wa, 10.5f, dim, Justification::centred, 500);
        const char* kn[] = { "GAIN", "PAN", "TUNE", "FINE", "ATTACK", "DECAY", "DELAY" };
        int x = 596; for (auto* n : kn) { caps (g, n, { (float) x, 458, 58, 10 }, 8, n[1] == 'E' && n[2] == 'L' ? Colour (0xff4ad6ff) : dim, Justification::centred); x += 60; }
        caps (g, "SWING", { (float) getWidth() - 152, 458, 64, 10 }, 8, dim, Justification::centred);
        caps (g, "MASTER", { (float) getWidth() - 80, 458, 64, 10 }, 8, dim, Justification::centred);
        g.setColour (Colours::white.withAlpha (0.06f)); g.drawVerticalLine (getWidth() - 160, 456, 560);
        if (dropHint >= 0) { g.setColour (Colours::white.withAlpha (0.8f)); g.drawRoundedRectangle (padRect (dropHint), 10, 2.5f); }
        if (info.isNotEmpty()) label (g, info, { 16, (float) getHeight() - 26, (float) getWidth() - 32, 18 }, 10, silver, Justification::centredLeft, 500);
        label (g, U ("MIDI: C1 (36) = Pad 1 \xe2\x80\xa6 D#2 (51) = Pad 16"), { 16, (float) getHeight() - 26, (float) getWidth() - 32, 18 }, 9, dim2, Justification::centredRight, 400);
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
        p = (int) ((pt.y - g.getY()) / 20.0f); s = (int) ((pt.x - g.getX()) / (g.getWidth() / plugin.length()));
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
            const int row = (int) ((e.position.y - r.getY()) / 20.0f);
            if (row >= 0 && row < beat::numPads) { sel = row; refresh(); if (e.mods.isPopupMenu()) rowMenu (row); else plugin.previewPad (row); }
            return;
        }
        auto wa = waveArea();
        if (wa.contains (e.position))
        {
            const float sx = wa.getX() + wa.getWidth() * (float) (double) plugin.pad (sel)["start"], ex = wa.getX() + wa.getWidth() * (float) (double) plugin.pad (sel)["end"];
            dragMode = std::abs (e.position.x - sx) < std::abs (e.position.x - ex) ? 2 : 3;
            mouseDrag (e);
        }
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
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
    void mouseUp (const juce::MouseEvent&) override { if (dragMode == 2 || dragMode == 3) plugin.previewPad (sel); dragMode = 0; }

    void padMenu (int p)
    {
        juce::PopupMenu m; m.addSectionHeader ("Pad " + juce::String (p + 1));
        m.addItem (1, U ("Sample yuklash...")); m.addItem (2, U ("Padni tozalash")); m.addSeparator();
        m.addItem (3, U ("Nusxa olish")); m.addItem (4, U ("Qo'yish"), copied.isValid());
        m.showMenuAsync ({}, [this, p] (int r)
        {
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
    void timerCallback() override { if (++ticks % 8 == 0 || plugin.playStep.load() != lastStep || juce::Time::getMillisecondCounter() - lastHitPaint < 300) { lastStep = plugin.playStep.load(); repaint(); }
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
    juce::Slider gainK, panK, tuneK, fineK, atkK, decK, dlyK, swingK, masterK;
    juce::TextButton normB, prevB, nextB;
    std::map<juce::String, sync::Match> matchCache; juce::String matchText; int matchScore = 2;
    juce::ValueTree copied; juce::String info;
    std::unique_ptr<juce::FileChooser> chooser;
};

inline std::unique_ptr<te::Plugin::EditorComponent> AzBeatPlugin::createEditor() { return std::make_unique<BeatEditor> (*this); }
}
