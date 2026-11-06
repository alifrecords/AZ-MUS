// =============================================================================
//  AZ MUS — sample/loop'larni loyiha BPM va tonalligiga moslashtirish
//  · BPM va tonallik fayl nomidan o'qiladi ("Loop_128bpm_Am.wav"), bo'lmasa audiodan taxmin qilinadi
//  · Oldindan tinglash va trekka qo'yishda: time-stretch (temp) + pitch shift (tonallik)
// =============================================================================
#pragma once
#include "AppEngine.h"

namespace az::sync
{
inline const char* noteNames[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };

struct KeyInfo { int root = -1; bool minor = false; bool guessed = false; bool valid() const { return root >= 0; } };
inline juce::String keyName (KeyInfo k) { return k.valid() ? juce::String (noteNames[k.root]) + (k.minor ? "m" : "") : juce::String ("?"); }

// ---------------------------------------------------------------- fayl nomidan
inline double bpmFromName (const juce::String& name)
{
    const auto s = name.toLowerCase();
    for (int i = s.indexOf ("bpm"); i >= 0; i = s.indexOf (i + 1, "bpm"))
    {
        int e = i; while (e > 0 && s[e - 1] == ' ') --e;
        int b = e; while (b > 0 && (juce::CharacterFunctions::isDigit (s[b - 1]) || s[b - 1] == '.')) --b;
        const double v = s.substring (b, e).getDoubleValue();
        if (v >= 50 && v <= 250) return v;
        // "bpm128" ko'rinishi
        int a = i + 3; while (a < s.length() && s[a] == ' ') ++a;
        int z = a; while (z < s.length() && (juce::CharacterFunctions::isDigit (s[z]) || s[z] == '.')) ++z;
        const double w = s.substring (a, z).getDoubleValue();
        if (w >= 50 && w <= 250) return w;
    }
    return 0;
}

inline int noteIndex (juce::juce_wchar l, juce::juce_wchar acc)
{
    static const int base[] = { 9, 11, 0, 2, 4, 5, 7 };   // A B C D E F G
    const auto u = juce::CharacterFunctions::toUpperCase (l);
    if (u < 'A' || u > 'G') return -1;
    int n = base[u - 'A'];
    if (acc == '#') n += 1; if (acc == 'b') n -= 1;
    return (n + 12) % 12;
}

inline KeyInfo keyFromName (const juce::String& name)
{
    auto toks = juce::StringArray::fromTokens (name.replaceCharacters ("-.()[]", "______"), "_ ", {});
    for (int i = toks.size(); --i >= 0;)   // oxiridan (odatda kalit oxirida)
    {
        auto t = toks[i].trim();
        if (t.isEmpty() || t.length() > 7) continue;
        const auto l = t[0];
        if (l < 'A' || l > 'G') { if (! (l >= 'a' && l <= 'g' && t.length() >= 2)) continue; }
        int p = 1; juce::juce_wchar acc = 0;
        if (p < t.length() && (t[p] == '#' || t[p] == 'b')) { acc = t[p]; ++p; }
        const auto rest = t.substring (p);
        const auto rl = rest.toLowerCase();
        bool minor = false;
        if (rest.isEmpty() || rest == "M" || rl == "maj" || rl == "major") minor = false;
        else if (rest == "m" || rl == "min" || rl == "minor" || rl == "mi") minor = true;
        else continue;
        if (rest.isEmpty() && i + 1 < toks.size())   // "Bb minor", "C major" (alohida so'z)
        {
            const auto nx = toks[i + 1].toLowerCase();
            if (nx == "minor" || nx == "min" || nx == "moll") minor = true;
            else if (nx == "major" || nx == "maj" || nx == "dur") minor = false;
            else if (t.length() == 1 && i == 0) continue;   // boshidagi yakka harf — nom bo'lishi mumkin
        }
        const int n = noteIndex (l, acc);
        if (n >= 0) return { n, minor, false };
    }
    return {};
}

inline KeyInfo keyFromString (const juce::String& s)   // "A minor", "C# major"
{
    if (s.isEmpty()) return {};
    juce::juce_wchar acc = s.length() > 1 && (s[1] == '#' || s[1] == 'b') ? s[1] : 0;
    return { noteIndex (s[0], acc), s.containsIgnoreCase ("minor"), false };
}

inline bool isDrumName (const juce::String& n)
{
    const auto s = n.toLowerCase();
    for (auto* w : { "kick", "snare", "hat", "hh", "clap", "perc", "rim", "tom", "crash", "ride", "cymbal", "shaker", "snap", "drum", "kik", "bd_", "sd_", "fx", "riser", "impact" })
        if (s.contains (w)) return true;
    return false;
}

// ---------------------------------------------------------------- audiodan tonallik (chroma + Krumhansl)
inline KeyInfo detectKey (const juce::AudioBuffer<float>& b, double sr)
{
    constexpr int order = 13, N = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> win ((size_t) N * 2);
    double chroma[12] {};
    const int total = juce::jmin (b.getNumSamples(), (int) (sr * 40));
    int frames = 0;
    for (int s = 0; s + N <= total; s += N / 2, ++frames)
    {
        std::fill (win.begin(), win.end(), 0.0f);
        for (int i = 0; i < N; ++i)
        {
            float v = 0; for (int c = 0; c < b.getNumChannels(); ++c) v += b.getSample (c, s + i);
            win[(size_t) i] = v * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) N));
        }
        fft.performFrequencyOnlyForwardTransform (win.data());
        for (int k = 1; k < N / 2; ++k)
        {
            const double f = k * sr / N;
            if (f < 55 || f > 2000) continue;
            const double midi = 69 + 12 * std::log2 (f / 440.0);
            const int pc = ((int) std::lround (midi) % 12 + 12) % 12;
            chroma[pc] += (double) win[(size_t) k] * win[(size_t) k];
        }
    }
    if (frames == 0) return {};
    static const double maj[] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
    static const double mnr[] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };
    auto corr = [&] (const double* prof, int root)
    {
        double mx = 0, my = 0; for (int i = 0; i < 12; ++i) { mx += chroma[i]; my += prof[i]; } mx /= 12; my /= 12;
        double sxy = 0, sxx = 0, syy = 0;
        for (int i = 0; i < 12; ++i) { const double x = chroma[(i + root) % 12] - mx, y = prof[i] - my; sxy += x * y; sxx += x * x; syy += y * y; }
        return sxy / std::sqrt (juce::jmax (1e-12, sxx * syy));
    };
    KeyInfo best; double bc = -2;
    for (int r = 0; r < 12; ++r)
    {
        if (const double c = corr (maj, r); c > bc) { bc = c; best = { r, false, true }; }
        if (const double c = corr (mnr, r); c > bc) { bc = c; best = { r, true, true }; }
    }
    return bc > 0.3 ? best : KeyInfo {};
}

// tonallikni loyihaga keltirish uchun yarim tonlar (-6..+5); minor<->major nisbiy tonallik orqali
inline int semitoneShift (KeyInfo file, KeyInfo proj)
{
    if (! file.valid() || ! proj.valid()) return 0;
    int target = proj.root;
    if (file.minor && ! proj.minor) target = (proj.root + 9) % 12;
    if (! file.minor && proj.minor) target = (proj.root + 3) % 12;
    int d = ((target - file.root) % 12 + 12) % 12;
    if (d > 5) d -= 12;
    return d;
}

// fayl uzunligidan BPM taxmini (1/2/4/8/16 takt, 4/4)
inline double guessBpmFromLength (double seconds, double projBpm)
{
    double best = 0, bd = 1e9;
    for (int bars : { 1, 2, 4, 8, 16 })
    {
        const double bpm = bars * 4 * 60.0 / seconds;
        if (bpm < 65 || bpm > 190) continue;
        if (std::abs (bpm - projBpm) < bd) { bd = std::abs (bpm - projBpm); best = bpm; }
    }
    return best;
}

struct FileInfo
{
    double lengthSec = 0, sampleRate = 48000, bpm = 0; bool bpmGuessed = false;
    KeyInfo key; bool drum = false, loop = false;
};

inline FileInfo analyse (te::Engine& e, const juce::File& f, double projBpm, juce::AudioBuffer<float>* outBuf = nullptr)
{
    FileInfo fi;
    std::unique_ptr<juce::AudioFormatReader> r (e.getAudioFileFormatManager().readFormatManager.createReaderFor (f));
    if (! r) return fi;
    fi.sampleRate = r->sampleRate; fi.lengthSec = (double) r->lengthInSamples / r->sampleRate;
    const auto name = f.getFileNameWithoutExtension();
    fi.drum = isDrumName (name);
    fi.bpm = bpmFromName (name);
    fi.loop = fi.bpm > 0 || name.containsIgnoreCase ("loop") || (fi.lengthSec >= 1.9 && ! fi.drum) || (fi.drum && fi.lengthSec >= 1.9 && name.containsIgnoreCase ("loop"));
    if (fi.bpm <= 0 && fi.loop) { fi.bpm = guessBpmFromLength (fi.lengthSec, projBpm); fi.bpmGuessed = fi.bpm > 0; }
    fi.key = keyFromName (name);
    juce::AudioBuffer<float> tmp;
    auto& b = outBuf ? *outBuf : tmp;
    if (outBuf || (! fi.key.valid() && ! fi.drum))
    {
        b.setSize ((int) juce::jmax (1u, r->numChannels), (int) std::min<juce::int64> (r->lengthInSamples, (juce::int64) (r->sampleRate * 600)));
        r->read (&b, 0, b.getNumSamples(), 0, true, true);
    }
    if (! fi.key.valid() && ! fi.drum && b.getNumSamples() > 0) fi.key = detectKey (b, fi.sampleRate);
    return fi;
}

// ---------------------------------------------------------------- offline time-stretch + pitch (SoundTouch)
// speedRatio: 2 = ikki marta sekinroq (uzunroq), 0.5 = tezroq
inline juce::AudioBuffer<float> stretch (const juce::AudioBuffer<float>& in, double sr, double speedRatio, double semis)
{
    const int ch = juce::jmax (1, in.getNumChannels());
    if (std::abs (speedRatio - 1.0) < 1e-4 && std::abs (semis) < 1e-4) return in;
    te::TimeStretcher ts;
    const int block = 1024;
    ts.initialise (sr, block, ch, te::TimeStretcher::soundtouchBetter, {}, false);
    ts.setSpeedAndPitch ((float) speedRatio, (float) semis);
    const int expected = (int) (in.getNumSamples() * speedRatio);
    juce::AudioBuffer<float> out (ch, expected + block * 8); out.clear();
    juce::AudioBuffer<float> inBlk (ch, ts.getMaxFramesNeeded() + block), outBlk (ch, block);
    int readPos = 0, written = 0;
    auto append = [&] (int n)
    {
        n = juce::jmin (n, out.getNumSamples() - written);
        for (int c = 0; c < ch; ++c) out.copyFrom (c, written, outBlk, c, 0, n);
        written += n;
    };
    while (readPos < in.getNumSamples() && written < out.getNumSamples())
    {
        const int need = juce::jmax (1, ts.getFramesNeeded());
        inBlk.clear();
        const int n = juce::jmin (need, in.getNumSamples() - readPos);
        for (int c = 0; c < ch; ++c) inBlk.copyFrom (c, 0, in, juce::jmin (c, in.getNumChannels() - 1), readPos, n);
        readPos += n;
        outBlk.clear();
        const int got = ts.processData (inBlk.getArrayOfReadPointers(), need, outBlk.getArrayOfWritePointers());
        append (got);
    }
    for (int guard = 0; guard < 64 && written < out.getNumSamples(); ++guard)
    {
        outBlk.clear();
        const int got = ts.flush (outBlk.getArrayOfWritePointers());
        if (got <= 0) break;
        append (got);
    }
    // algoritm kechikishini kesib, kutilgan uzunlikka keltiramiz
    const int lat = juce::jlimit (0, juce::jmax (0, written - 1), ts.getLatencySamples());
    const int len = juce::jmax (1, juce::jmin (expected, written - lat));
    juce::AudioBuffer<float> res (ch, len);
    for (int c = 0; c < ch; ++c) res.copyFrom (c, 0, out, c, lat, len);
    return res;
}
// ---------------------------------------------------------------- one-shot (kick, 808, bass) asosiy tonini topish
inline double detectPitchHz (const juce::AudioBuffer<float>& b, double sr)
{
    constexpr int order = 15, N = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> w ((size_t) N * 2, 0.0f);
    const int len = juce::jmin (b.getNumSamples(), (int) (sr * 0.6), N);
    if (len < 512) return 0;
    for (int i = 0; i < len; ++i)
    {
        float v = 0; for (int c = 0; c < b.getNumChannels(); ++c) v += b.getSample (c, i);
        w[(size_t) i] = v * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) len));
    }
    fft.performFrequencyOnlyForwardTransform (w.data());
    int best = 0; float bv = 0;
    const int k0 = (int) (28.0 / sr * N), k1 = (int) (1200.0 / sr * N);
    for (int k = juce::jmax (1, k0); k < k1; ++k) if (w[(size_t) k] > bv) { bv = w[(size_t) k]; best = k; }
    if (best == 0) return 0;
    // parabolik aniqlash
    const double a = w[(size_t) best - 1], c0 = w[(size_t) best], d = w[(size_t) best + 1];
    const double den = a - 2 * c0 + d;
    const double off = std::abs (den) < 1e-12 ? 0.0 : juce::jlimit (-0.5, 0.5, 0.5 * (a - d) / den);
    return (best + off) * sr / N;
}
inline int hzToMidi (double hz) { return hz > 0 ? (int) std::lround (69 + 12 * std::log2 (hz / 440.0)) : -1; }
inline bool noteInKey (int pc, KeyInfo k)
{
    if (! k.valid()) return true;
    static const int maj[] = { 0, 2, 4, 5, 7, 9, 11 }, mnr[] = { 0, 2, 3, 5, 7, 8, 10 };
    const int rel = ((pc - k.root) % 12 + 12) % 12;
    for (int i : (k.minor ? mnr : maj)) if (i == rel) return true;
    return false;
}

// loyihaga moslik: 2 = mos, 1 = yaqin, 0 = mos emas
struct Match { int score = 2; juce::String text; };
inline Match matchToProject (te::Engine& e, const juce::File& f, double projBpm, KeyInfo projKey)
{
    juce::AudioBuffer<float> b; auto fi = analyse (e, f, projBpm, &b);
    Match m; juce::StringArray parts;
    const auto check = juce::String::fromUTF8 (" \xe2\x9c\x93"), cross = juce::String::fromUTF8 (" \xe2\x9c\x97"), approx = juce::String::fromUTF8 (" \xe2\x89\x88");
    if (fi.loop && fi.bpm > 0)
    {
        const double r = fi.bpm / projBpm;
        const bool ok = std::abs (r - 1) < 0.02 || std::abs (r - 2) < 0.04 || std::abs (r - 0.5) < 0.01;
        const bool near = std::abs (r - 1) < 0.12;
        parts.add (juce::String (fi.bpm, 1) + (fi.bpmGuessed ? "?" : "") + " BPM" + (ok ? check : near ? approx : cross));
        m.score = juce::jmin (m.score, ok ? 2 : near ? 1 : 0);
    }
    const bool oneShot = ! fi.loop && fi.lengthSec < 3.0;
    if (! oneShot && ! fi.drum && fi.key.valid())
    {
        const int d = ((fi.key.root - projKey.root) % 12 + 12) % 12;
        const bool same = fi.key.minor == projKey.minor && d == 0;
        const bool rel = (fi.key.minor && ! projKey.minor && d == 9) || (! fi.key.minor && projKey.minor && d == 3);
        const bool fifth = d == 5 || d == 7;
        parts.add (keyName (fi.key) + (fi.key.guessed ? "?" : "") + (same || rel ? check : fifth ? approx : cross));
        m.score = juce::jmin (m.score, same || rel ? 2 : fifth ? 1 : 0);
    }
    else if (oneShot && b.getNumSamples() > 0)
    {
        const double hz = detectPitchHz (b, fi.sampleRate);
        const int midi = hzToMidi (hz);
        const bool tonal = ! fi.drum || f.getFileName().containsIgnoreCase ("808") || f.getFileName().containsIgnoreCase ("kick") || f.getFileName().containsIgnoreCase ("bass");
        if (midi > 0 && midi < 128 && tonal)
        {
            const bool in = noteInKey (midi % 12, projKey);
            parts.add (juce::MidiMessage::getMidiNoteName (midi, true, true, 3) + " (" + juce::String ((int) hz) + " Hz)" + (in ? check : cross));
            m.score = juce::jmin (m.score, in ? 2 : 0);
        }
    }
    if (parts.isEmpty()) parts.add (fi.drum ? juce::String::fromUTF8 ("perkussiya") : juce::String::fromUTF8 ("tahlil yo'q"));
    m.text = parts.joinIntoString (juce::String::fromUTF8 ("  \xc2\xb7  "));
    return m;
}
}
