// =============================================================================
//  AZ VOX · Suno Glue — avtomatik vokal obrobotkasi (v1: 5 bosqich + ANALIZ)
//  1 CLEAN      — HPF (24 dB/okt) + nafas/shovqin ekspanderi
//  2 TONE MATCH — zamonaviy vokal egri chizig'iga 7 polosali moslash + 2 ta rezonans kesish
//  3 HARMONIC   — yo'qolgan presence/air'ni garmonikalar bilan tiklash (exciter)
//  4 DE-ESS     — dinamik, faqat sibilans paytida
//  5 COMP x2    — FET (tez, 4:1) -> Opto (silliq, 2:1) + avtomatik daraja
//  ANALIZ: trek audiosini (AZ VOX'gacha bo'lgan zanjir bilan) render qilib, barcha bosqichlarni sozlaydi.
// =============================================================================
#pragma once
#include "AppEngine.h"
#include "TrackTools.h"
#include "InsertAnalysis.h"

namespace az
{
class AzVoxPlugin : public te::Plugin
{
public:
    static const char* getPluginName() { return "AZ VOX"; }
    static inline const char* xmlTypeName = "azvox";
    static constexpr int numEq = 7;
    static inline const double eqHz[numEq] = { 120, 250, 500, 1000, 2500, 5000, 10000 };

    explicit AzVoxPlugin (te::PluginCreationInfo info) : te::Plugin (info)
    {
        auto def = [this] (const char* k, double v) { if (! state.hasProperty (id (k))) state.setProperty (id (k), v, nullptr); };
        for (int i = 1; i <= 5; ++i) def (("on" + juce::String (i)).toRawUTF8(), 1);
        def ("hpf", 90); def ("breath", -8); def ("gate", -50);
        def ("match", 70); def ("resAmt", 100);
        for (int i = 0; i < numEq; ++i) def (("g" + juce::String (i)).toRawUTF8(), 0);
        def ("r1Hz", 0); def ("r1Db", 0); def ("r2Hz", 0); def ("r2Db", 0);
        def ("pres", 45); def ("presHz", 3500); def ("air", 35); def ("airHz", 12000);
        def ("dsHz", 6800); def ("dsMax", -4); def ("dsThr", -32);
        def ("fetThr", -22); def ("opThr", -20); def ("fetOff", 0); def ("opOff", 0);
        def ("outGain", 0); def ("trim", 0);
        def ("mClean", 70); def ("mWarm", 50); def ("mBright", 60); def ("mPunch", 50); def ("mix", 100);
        def ("ab", 0);
        pull();
    }
    ~AzVoxPlugin() override { notifyListenersOfDeletion(); }
    static juce::Identifier id (const char* k) { return juce::Identifier ("vx_" + juce::String (k)); }
    double get (const char* k) const { return state.getProperty (id (k)); }
    void set (const char* k, double v, bool undoable = true) { state.setProperty (id (k), v, undoable ? getUndoManager() : nullptr); }

    juce::String getName() const override                { return getPluginName(); }
    juce::String getPluginType() override                { return xmlTypeName; }
    juce::String getSelectableDescription() override     { return getName(); }
    juce::String getShortName (int) override              { return "AZ VOX"; }
    bool needsConstantBufferSize() override               { return false; }
    BusLayout getBusses() const override                  { return BusLayout::singlePassThrough(); }
    void valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& i) override
    {
        te::Plugin::valueTreePropertyChanged (v, i);
        if (v == state && i.toString().startsWith ("vx_")) pull();
    }
    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sr = info.sampleRate > 0 ? info.sampleRate : 48000.0;
        for (auto* f : allFilters()) f->reset();
        envGate = envDs = envFet = envOp = 0; gGate = 1; dirty = true;
    }
    void deinitialise() override {}
    void reset() override { for (auto* f : allFilters()) f->reset(); }

    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return;
        auto& buf = *fc.destBuffer; const int n = fc.bufferNumSamples, s0 = fc.bufferStartSample, nc = juce::jmin (2, buf.getNumChannels());
        if (nc == 0) return;
        if (dirty.exchange (false)) design();
        float inPk = 0, outPk = 0, maxFet = 0, maxOp = 0, maxDs = 0, maxGate = 0;
        float* ch[2] = { buf.getWritePointer (0, s0), buf.getWritePointer (nc > 1 ? 1 : 0, s0) };
        const bool ab = abOn.load();
        const float mix = ab ? 0.0f : mixV.load();
        const float aGate = coef (0.002), rGate = coef (0.08), aDs = coef (0.0015), rDs = coef (0.06);
        const float aFet = coef (fetAtk), rFet = coef (0.07), aOp = coef (0.012), rOp = coef (0.35);
        const float outG = juce::Decibels::decibelsToGain (outDb.load());
        rLim = coef (0.06);
        for (int i = 0; i < n; ++i)
        {
            float x[2] = { ch[0][i], ch[1][i] }, dry[2] = { x[0], x[1] };
            inPk = juce::jmax (inPk, std::abs (x[0]), std::abs (x[1]));
            // 1 CLEAN
            if (on[0])
            {
                for (int c = 0; c < nc; ++c) x[c] = hp2[c].process (hp1[c].process (x[c], 0), 0);
                const float lvl = juce::jmax (std::abs (x[0]), std::abs (x[1]));
                envGate += (lvl > envGate ? aGate : rGate) * (lvl - envGate);
                const float eDb = juce::Decibels::gainToDecibels (envGate, -120.0f);
                const float red = eDb < gateDb ? juce::jmax (breathDb, (eDb - gateDb)) : 0.0f;   // 1:2 ekspander, nafas chegarasi bilan
                const float tg = juce::Decibels::decibelsToGain (red);
                gGate += (tg < gGate ? 0.02f : 0.002f) * (tg - gGate);
                for (int c = 0; c < nc; ++c) x[c] *= gGate;
                maxGate = juce::jmin (maxGate, juce::Decibels::gainToDecibels (gGate, -60.0f));
            }
            // 2 TONE MATCH
            if (on[1]) for (int c = 0; c < nc; ++c) { float v = x[c]; for (int k = 0; k < numEq + 2; ++k) v = eq[k][c].process (v, 0); x[c] = v; }
            // 3 HARMONIC: presence va air garmonikalari
            if (on[2])
                for (int c = 0; c < nc; ++c)
                {
                    const float p = xpHp2[c].process (std::tanh (xpDrive * xpHp1[c].process (x[c], 0)), 0);
                    const float a = xaHp2[c].process (std::tanh (xaDrive * xaHp1[c].process (x[c], 0)), 0);
                    x[c] = airShelf[c].process (x[c] + presAmt * p + airAmt * a, 0);
                }
            // 4 DE-ESS: sibilans polosasini dinamik kamaytirish
            if (on[3])
            {
                float band[2] = {};
                for (int c = 0; c < nc; ++c) band[c] = dsLp[c].process (dsHp[c].process (x[c], 0), 0);
                const float lvl = juce::jmax (std::abs (band[0]), std::abs (band[1]));
                envDs += (lvl > envDs ? aDs : rDs) * (lvl - envDs);
                const float over = juce::Decibels::gainToDecibels (envDs, -120.0f) - dsThrDb;
                const float red = over > 0 ? juce::jmax (dsMaxDb, -over * 0.7f) : 0.0f;
                const float g = juce::Decibels::decibelsToGain (red);
                for (int c = 0; c < nc; ++c) x[c] -= band[c] * (1.0f - g);
                maxDs = juce::jmin (maxDs, red);
            }
            // 5 COMP x2: FET (cho'qqi) -> Opto (RMS)
            if (on[4])
            {
                const float lvl = juce::jmax (std::abs (x[0]), std::abs (x[1]));
                envFet += (lvl > envFet ? aFet : rFet) * (lvl - envFet);
                float grF = 0; { const float d = juce::Decibels::gainToDecibels (envFet, -120.0f) - fetThrDb; if (d > 0) grF = -d * (1.0f - 1.0f / 4.0f); }
                const float gF = juce::Decibels::decibelsToGain (grF);
                for (int c = 0; c < nc; ++c) x[c] *= gF;
                const float sq = 0.5f * (x[0] * x[0] + x[1] * x[1]);
                envOp += (sq > envOp ? aOp : rOp) * (sq - envOp);
                float grO = 0; { const float d = 10.0f * std::log10 (envOp + 1e-12f) + 3.0f - opThrDb; if (d > 0) grO = -d * 0.5f; }
                const float gO = juce::Decibels::decibelsToGain (grO);
                for (int c = 0; c < nc; ++c) x[c] *= gO;
                // pik nazorati (crest 9–14 dB): bir zumda ushlovchi, 60 ms qo'yib yuboruvchi limiter, FET chegarasidan +6 dB
                const float pk = juce::jmax (std::abs (x[0]), std::abs (x[1]));
                const float lim = juce::Decibels::decibelsToGain (fetThrDb + 6.0f);
                const float want = pk > lim ? lim / pk : 1.0f;
                gLim = want < gLim ? want : gLim + rLim * (1.0f - gLim);
                for (int c = 0; c < nc; ++c) x[c] *= gLim;
                maxFet = juce::jmin (maxFet, grF + juce::Decibels::gainToDecibels (gLim, -60.0f)); maxOp = juce::jmin (maxOp, grO);
            }
            for (int c = 0; c < nc; ++c)
            {
                const float w = x[c] * outG;
                ch[c][i] = dry[c] + mix * (w - dry[c]);
            }
            outPk = juce::jmax (outPk, std::abs (ch[0][i]), std::abs (ch[nc > 1 ? 1 : 0][i]));
        }
        mIn = juce::jmax (mIn.load() * 0.85f, inPk); mOut = juce::jmax (mOut.load() * 0.85f, outPk);
        mFet = juce::jmin (mFet.load() * 0.8f, maxFet); mOp = juce::jmin (mOp.load() * 0.8f, maxOp);
        mDs = juce::jmin (mDs.load() * 0.8f, maxDs); mGate = juce::jmin (mGate.load() * 0.8f, maxGate);
    }
    std::unique_ptr<EditorComponent> createEditor() override;

    std::atomic<float> mIn { 0 }, mOut { 0 }, mFet { 0 }, mOp { 0 }, mDs { 0 }, mGate { 0 };
    double sr = 48000.0;

private:
    tt::Biquad hp1[2], hp2[2], eq[numEq + 2][2], xpHp1[2], xpHp2[2], xaHp1[2], xaHp2[2], airShelf[2], dsHp[2], dsLp[2];
    float envGate = 0, envDs = 0, envFet = 0, envOp = 0, gGate = 1, gLim = 1, rLim = 0.001f;
    bool on[5] { true, true, true, true, true };
    std::atomic<bool> dirty { true }, abOn { false };
    std::atomic<float> mixV { 1 }, outDb { 0 };
    float gateDb = -50, breathDb = -8, dsThrDb = -32, dsMaxDb = -4, fetThrDb = -22, opThrDb = -20, fetAtk = 0.003f;
    float presAmt = 0, airAmt = 0, xpDrive = 2, xaDrive = 2;

    std::vector<tt::Biquad*> allFilters()
    {
        std::vector<tt::Biquad*> v;
        for (int c = 0; c < 2; ++c)
        {
            for (auto* f : { &hp1[c], &hp2[c], &xpHp1[c], &xpHp2[c], &xaHp1[c], &xaHp2[c], &airShelf[c], &dsHp[c], &dsLp[c] }) v.push_back (f);
            for (int k = 0; k < numEq + 2; ++k) v.push_back (&eq[k][c]);
        }
        return v;
    }
    float coef (double sec) const { return (float) (1.0 - std::exp (-1.0 / (juce::jmax (1e-4, sec) * sr))); }

    // makrolar va parametrlar -> ichki qiymatlar
    void pull()
    {
        for (int i = 0; i < 5; ++i) on[i] = (int) get (("on" + juce::String (i + 1)).toRawUTF8()) != 0;
        abOn = (int) get ("ab") != 0;
        mixV = (float) juce::jlimit (0.0, 1.0, get ("mix") / 100.0);
        outDb = (float) (get ("outGain") + get ("trim"));
        dirty = true;
    }
    void design()
    {
        const double cl = get ("mClean") / 70.0, wm = (get ("mWarm") - 50) / 50.0, br = get ("mBright") / 60.0, pu = (get ("mPunch") - 50) / 50.0;
        const double hpf = juce::jlimit (30.0, 250.0, get ("hpf") * (0.85 + 0.15 * cl));
        for (int c = 0; c < 2; ++c) { hp1[c].set (3, hpf, 0, 0.5412, sr); hp2[c].set (3, hpf, 0, 1.3066, sr); }
        breathDb = (float) juce::jlimit (-30.0, 0.0, get ("breath") * cl);
        gateDb = (float) get ("gate");
        const double m = get ("match") / 100.0;
        for (int k = 0; k < numEq; ++k)
        {
            double gdb = get (("g" + juce::String (k)).toRawUTF8()) * m;
            if (k == 1) gdb += 2.5 * wm;                 // WARMTH: 250 Hz
            if (k == 2) gdb -= 1.0 * wm;
            if (k >= 4) gdb += 2.0 * (br - 1.0);         // BRIGHT: 2.5k / 5k / 10k
            gdb = juce::jlimit (-12.0, 12.0, gdb);
            const int type = k == 0 ? 1 : k == numEq - 1 ? 2 : 0;
            for (int c = 0; c < 2; ++c) eq[k][c].set (type, eqHz[k], gdb, k == 0 || k == numEq - 1 ? 0.7 : 0.9, sr);
        }
        const double ra = get ("resAmt") / 100.0;
        for (int r = 0; r < 2; ++r)
        {
            const double hz = get (r == 0 ? "r1Hz" : "r2Hz"), db = get (r == 0 ? "r1Db" : "r2Db") * ra;
            for (int c = 0; c < 2; ++c) eq[numEq + r][c].set (0, hz > 20 ? hz : 1000, hz > 20 ? db : 0, 5.0, sr);
        }
        const double ph = get ("presHz"), ah = get ("airHz");
        presAmt = (float) juce::jlimit (0.0, 1.2, get ("pres") / 100.0 * br) * 0.45f;
        airAmt  = (float) juce::jlimit (0.0, 1.2, get ("air") / 100.0 * br) * 0.35f;
        xpDrive = (float) (2.0 + 2.0 * juce::jmax (0.0, wm)); xaDrive = 2.5f;
        for (int c = 0; c < 2; ++c)
        {
            xpHp1[c].set (3, ph * 0.5, 0, 0.7071, sr); xpHp2[c].set (3, ph, 0, 0.7071, sr);
            xaHp1[c].set (3, ah * 0.5, 0, 0.7071, sr); xaHp2[c].set (3, ah * 0.75, 0, 0.7071, sr);
            airShelf[c].set (2, ah, 6.0 * airAmt / 0.35, 0.7, sr);
            dsHp[c].set (3, get ("dsHz") * 0.75, 0, 0.7071, sr); dsLp[c].set (4, juce::jmin (sr * 0.45, get ("dsHz") * 1.4), 0, 0.7071, sr);
        }
        dsThrDb = (float) get ("dsThr"); dsMaxDb = (float) juce::jlimit (-18.0, 0.0, get ("dsMax"));
        fetThrDb = (float) (get ("fetThr") + get ("fetOff") - 5.0 * pu);
        opThrDb  = (float) (get ("opThr") + get ("opOff"));
        fetAtk = (float) juce::jlimit (0.001, 0.02, 0.003 + 0.005 * pu);
    }
};

// ---------------------------------------------------------------- ANALIZ
struct VoxDiag
{
    double lufs = -70, centroid = 0, crest = 0, midShare = 0, airShare = 0, f0med = 0, f0lo = 0, f0hi = 0;
    std::vector<float> spec;           // 60 nuqta, 30 Hz–18 kHz, nisbiy dB
};
inline double kWeightedLufs (const juce::AudioBuffer<float>& b, double sr)
{
    tt::Biquad s1, s2; s1.set (2, 1681.0, 4.0, 0.7071, sr); s2.set (3, 38.0, 0, 0.5, sr);
    const int n = b.getNumSamples(), nc = juce::jmin (2, b.getNumChannels()), blk = (int) (0.4 * sr), hop = (int) (0.1 * sr);
    std::vector<double> z ((size_t) n, 0.0);
    for (int c = 0; c < nc; ++c)
    {
        s1.reset(); s2.reset();
        auto* d = b.getReadPointer (c);
        for (int i = 0; i < n; ++i) { const double v = s2.process (s1.process (d[i], 0), 0); z[(size_t) i] += v * v; }
    }
    std::vector<double> L;
    for (int s = 0; s + blk <= n; s += hop) { double e = 0; for (int i = 0; i < blk; ++i) e += z[(size_t) (s + i)]; L.push_back (e / blk); }
    auto gated = [&] (double thr) { double e = 0; int k = 0; for (auto v : L) if (-0.691 + 10 * std::log10 (v + 1e-20) > thr) { e += v; ++k; } return k ? e / k : 0.0; };
    const double a = gated (-70); if (a <= 0) return -70;
    const double rel = -0.691 + 10 * std::log10 (a) - 10; const double g = gated (rel);
    return g > 0 ? -0.691 + 10 * std::log10 (g) : -70;
}
inline VoxDiag voxDiag (const juce::AudioBuffer<float>& b, double sr)
{
    VoxDiag d; const int n = b.getNumSamples(), nc = juce::jmin (2, b.getNumChannels());
    if (n < 8192 || nc == 0) return d;
    d.lufs = kWeightedLufs (b, sr);
    std::vector<float> mono ((size_t) n);
    for (int i = 0; i < n; ++i) { float v = 0; for (int c = 0; c < nc; ++c) v += b.getSample (c, i); mono[(size_t) i] = v / nc; }
    constexpr int order = 13, N = 1 << order; juce::dsp::FFT fft (order);
    std::vector<float> w (2 * N); std::vector<double> P (N / 2, 0.0);
    double pk = 0; for (auto v : mono) pk = juce::jmax (pk, (double) std::abs (v));
    // faol kadrlar
    std::vector<double> fr; const int hop = N / 2;
    for (int s = 0; s + N <= n; s += hop) { double e = 0; for (int i = 0; i < N; ++i) e += (double) mono[(size_t) (s + i)] * mono[(size_t) (s + i)]; fr.push_back (10 * std::log10 (e / N + 1e-20)); }
    if (fr.empty()) return d;
    const double mx = *std::max_element (fr.begin(), fr.end());
    double eAct = 0; int kAct = 0, fi = 0;
    for (int s = 0; s + N <= n; s += hop, ++fi)
    {
        if (fr[(size_t) fi] < mx - 35) continue;
        eAct += std::pow (10.0, fr[(size_t) fi] / 10); ++kAct;
        std::fill (w.begin(), w.end(), 0.0f);
        for (int i = 0; i < N; ++i) w[(size_t) i] = mono[(size_t) (s + i)] * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * i / (N - 1)));
        fft.performFrequencyOnlyForwardTransform (w.data());
        for (int k = 0; k < N / 2; ++k) P[(size_t) k] += (double) w[(size_t) k] * w[(size_t) k];
    }
    if (kAct == 0) return d;
    d.crest = 20 * std::log10 (pk + 1e-12) - 10 * std::log10 (eAct / kAct + 1e-20);
    double tot = 0, cs = 0, mid = 0, air = 0;
    for (int k = 1; k < N / 2; ++k)
    {
        const double f = k * sr / N; if (f < 20 || f > 20000) continue;
        tot += P[(size_t) k]; cs += f * P[(size_t) k];
        if (f >= 200 && f < 1000) mid += P[(size_t) k];
        if (f >= 2500) air += P[(size_t) k];
    }
    if (tot <= 0) return d;
    d.centroid = cs / tot; d.midShare = 100 * mid / tot; d.airShare = 100 * air / tot;
    double top = -300;
    for (int i = 0; i < 60; ++i)
    {
        const double fc = 30.0 * std::pow (18000.0 / 30.0, i / 59.0); double e = 0; int c = 0;
        for (int k = (int) (fc / 1.12 * N / sr); k <= (int) (fc * 1.12 * N / sr) && k < N / 2; ++k) if (k > 0) { e += P[(size_t) k]; ++c; }
        const float v = (float) (10 * std::log10 (e / juce::jmax (1, c) + 1e-20)); d.spec.push_back (v); top = juce::jmax (top, (double) v);
    }
    for (auto& v : d.spec) v -= (float) top;
    // asosiy ton (autokorrelyatsiya, faol kadrlarda)
    std::vector<double> f0s; const int W = 2048;
    for (int s = 0; s + W <= n; s += (int) (0.05 * sr))
    {
        double e = 0; for (int i = 0; i < W; ++i) e += (double) mono[(size_t) (s + i)] * mono[(size_t) (s + i)];
        if (10 * std::log10 (e / W + 1e-20) < mx - 25) continue;
        const int lo = (int) (sr / 900), hi = (int) (sr / 70);
        double best = 0; int bk = 0; double r0 = e;
        for (int k = lo; k < hi; ++k) { double r = 0; for (int i = 0; i + k < W; i += 2) r += (double) mono[(size_t) (s + i)] * mono[(size_t) (s + i + k)]; if (r > best) { best = r; bk = k; } }
        if (bk > 0 && best > 0.25 * r0) f0s.push_back (sr / bk);
        if (f0s.size() > 400) break;
    }
    if (f0s.size() > 5) { std::sort (f0s.begin(), f0s.end()); d.f0med = f0s[f0s.size() / 2]; d.f0lo = f0s[f0s.size() / 10]; d.f0hi = f0s[f0s.size() * 9 / 10]; }
    return d;
}
inline juce::String specToString (const std::vector<float>& s) { juce::String o; for (auto v : s) o << juce::String (v, 1) << ","; return o; }
inline std::vector<float> specFromString (const juce::String& s) { std::vector<float> v; for (auto& t : juce::StringArray::fromTokens (s, ",", {})) if (t.isNotEmpty()) v.push_back (t.getFloatValue()); return v; }

// ANALIZ: zanjirni AZ VOX'gacha render qilish, tahlil, parametrlarni yozish (bitta undo), oldin/keyin diagnozi
inline juce::String analyseVox (AzVoxPlugin& vx)
{
    BusyScope busy; if (! busy.ok) return U ("Tahlil allaqachon bajarilmoqda...");
    auto* t = dynamic_cast<te::AudioTrack*> (vx.getOwnerTrack());
    if (t == nullptr) return U ("AZ VOX trekda emas");
    auto range = analysisRange (*t);
    if (range.getLength() < 1s) return U ("Trekda audio yo'q");
    // uzunroq oraliq (45 s gacha) — butun ijroni yaxshiroq ko'rish uchun
    {
        te::TimePosition a = te::TimePosition::fromSeconds (1e9), b; for (auto* c : t->getClips()) { a = juce::jmin (a, c->getPosition().getStart()); b = juce::jmax (b, c->getPosition().getEnd()); }
        const auto lr = t->edit.getTransport().getLoopRange();
        if (! (lr.getLength() >= 2s && lr.getStart() < b && lr.getEnd() > a)) range = { a, juce::jmin (b, a + 45s) };
    }
    t->edit.getTransport().stop (false, true);
    auto plugs = AppEngine::userPlugins (*t);
    const int me = plugs.indexOf (&vx);
    std::vector<bool> orig; for (auto* p : plugs) orig.push_back (p->isEnabled());
    auto setEn = [] (te::Plugin& p, bool v) { p.state.setProperty (te::IDs::enabled, v, nullptr); };
    for (int i = juce::jmax (0, me); i < plugs.size(); ++i) setEn (*plugs[i], false);
    const auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("az_vox_tahlil.wav");
    juce::AudioBuffer<float> b; double sr = 48000;
    const bool ok = renderTrackTo (*t, range, tmp) && readWav (tmp, b, sr);
    if (! ok) { for (int i = 0; i < plugs.size(); ++i) setEn (*plugs[i], orig[(size_t) i]); return U ("ANALIZ: render bajarilmadi"); }
    const auto d0 = voxDiag (b, sr);
    // --- EQ: 7 polosa, vokal maqsad egrisiga (200 Hz–5 kHz o'rtachasi bo'yicha normallashtirilgan)
    std::vector<double> meas (AzVoxPlugin::numEq), tgt (AzVoxPlugin::numEq);
    {
        constexpr int order = 13, N = 1 << order; juce::dsp::FFT fft (order); std::vector<float> w (2 * N); std::vector<double> P (N / 2, 0.0);
        const int n = b.getNumSamples(), nc = b.getNumChannels(); int frames = 0;
        for (int s = 0; s + N <= n; s += N / 2)
        {
            std::fill (w.begin(), w.end(), 0.0f); double e = 0;
            for (int i = 0; i < N; ++i) { float v = 0; for (int c = 0; c < nc; ++c) v += b.getSample (c, s + i); w[(size_t) i] = v * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * i / (N - 1))); e += (double) v * v; }
            if (e / N < 1e-7) continue;
            fft.performFrequencyOnlyForwardTransform (w.data()); ++frames;
            for (int k = 0; k < N / 2; ++k) P[(size_t) k] += (double) w[(size_t) k] * w[(size_t) k];
        }
        auto bandDb = [&] (double fc, double oct) { double e = 0; int c = 0; for (int k = juce::jmax (1, (int) (fc / std::pow (2, oct / 2) * N / sr)); k <= (int) (fc * std::pow (2, oct / 2) * N / sr) && k < N / 2; ++k) { e += P[(size_t) k]; ++c; } return 10 * std::log10 (e / juce::jmax (1, c) + 1e-20); };
        double mM = 0, mT = 0;
        for (int k = 0; k < AzVoxPlugin::numEq; ++k) { meas[(size_t) k] = bandDb (AzVoxPlugin::eqHz[k], 1.0); tgt[(size_t) k] = targetCurve (AzVoxPlugin::eqHz[k], true); }
        for (int k = 1; k <= 4; ++k) { mM += meas[(size_t) k]; mT += tgt[(size_t) k]; }
        mM /= 4; mT /= 4;
        // rezonanslar: 250–2500 Hz, silliqlangan spektrdan > 4 dB yuqori cho'qqilar
        std::vector<std::pair<double, double>> peaks;
        for (double f = 250; f <= 2500; f *= std::pow (2.0, 1.0 / 24))
        {
            const double v = bandDb (f, 1.0 / 12), sm = bandDb (f, 1.2);
            peaks.push_back ({ v - sm, f });
        }
        std::sort (peaks.rbegin(), peaks.rend());
        std::vector<std::pair<double, double>> res;
        for (auto& [ex, f] : peaks) { if (ex < 3.5) break; bool far = true; for (auto& r : res) far &= std::abs (std::log2 (f / r.second)) > 0.4; if (far) res.push_back ({ ex, f }); if (res.size() == 2) break; }
        juce::ignoreUnused (frames);
        auto& um = t->edit.getUndoManager(); um.beginNewTransaction ("AZ VOX ANALIZ");
        double presDef = 0, airDef = 0;
        for (int k = 0; k < AzVoxPlugin::numEq; ++k)
        {
            const double want = (tgt[(size_t) k] - mT) - (meas[(size_t) k] - mM);
            const double g = juce::jlimit (-9.0, 9.0, want);
            vx.set (("g" + juce::String (k)).toRawUTF8(), std::round (g * 10) / 10);
            if (k == 4) presDef = juce::jmax (0.0, want - 9.0);
            if (k >= 5) airDef = juce::jmax (airDef, want - 9.0);
        }
        vx.set ("match", 75);
        vx.set ("r1Hz", res.size() > 0 ? std::round (res[0].second) : 0); vx.set ("r1Db", res.size() > 0 ? -juce::jmin (6.0, res[0].first) : 0);
        vx.set ("r2Hz", res.size() > 1 ? std::round (res[1].second) : 0); vx.set ("r2Db", res.size() > 1 ? -juce::jmin (6.0, res[1].first) : 0);
        vx.set ("pres", juce::jlimit (15.0, 100.0, 25.0 + presDef * 5.0));
        vx.set ("air", juce::jlimit (10.0, 100.0, 20.0 + airDef * 4.0));
        // sibilans markazi: 5–10 kHz dagi eng kuchli 1/3 okt
        double bestF = 6800, bestV = -300; for (double f = 5000; f <= 10000; f *= std::pow (2.0, 1.0 / 6)) { const double v = bandDb (f, 1.0 / 3); if (v > bestV) { bestV = v; bestF = f; } }
        vx.set ("dsHz", std::round (bestF / 100) * 100);
    }
    // --- dinamika / daraja / CLEAN
    {
        const int n = b.getNumSamples(), nc = b.getNumChannels(), W = (int) (0.05 * sr);
        std::vector<double> lv;
        for (int s = 0; s + W <= n; s += W) { double e = 0; for (int i = 0; i < W; ++i) for (int c = 0; c < nc; ++c) e += (double) b.getSample (c, s + i) * b.getSample (c, s + i); lv.push_back (10 * std::log10 (e / (W * nc) + 1e-20)); }
        std::vector<double> act, quiet; const double mx = lv.empty() ? -100 : *std::max_element (lv.begin(), lv.end());
        for (auto v : lv) { if (v > mx - 30) act.push_back (v); else if (v > -140) quiet.push_back (v); }
        std::sort (act.begin(), act.end()); std::sort (quiet.begin(), quiet.end());
        const double med = act.empty() ? -30 : act[act.size() / 2], p90 = act.empty() ? -20 : act[act.size() * 9 / 10];
        const double floorDb = quiet.empty() ? med - 50 : quiet[quiet.size() / 4];
        vx.set ("gate", std::round (juce::jlimit (-75.0, -30.0, juce::jmax (floorDb + 8, med - 28))));
        vx.set ("fetThr", std::round (p90 - 2));                  // baland joylar ~ -5..-7 dB GR
        vx.set ("opThr", std::round (med + 1));
        // chiqish: vokal stemi ~ -18 LUFS (siqishdan keyingi taxminiy daraja)
        const double estGr = 4.0;
        vx.set ("outGain", std::round (juce::jlimit (-12.0, 24.0, -18.0 - (d0.lufs - estGr)) * 10) / 10);
        if (d0.f0lo > 0) vx.set ("hpf", std::round (juce::jlimit (60.0, 140.0, d0.f0lo * 0.75)));
        const double s90 = act.empty() ? -30 : p90;
        vx.set ("dsThr", std::round (s90 - 14));
    }
    // profil
    juce::String prof = U ("Aniqlanmadi");
    if (d0.f0med > 0)
        prof = (d0.f0med < 165 ? U ("Erkak (past)") : d0.f0med < 260 ? U ("Erkak tenor / ayol alt") : U ("Ayol / yuqori ovoz"))
             + "  \xc2\xb7  " + juce::String (juce::roundToInt (d0.f0lo)) + U ("\xe2\x80\x93") + juce::String (juce::roundToInt (d0.f0hi)) + " Hz";
    vx.state.setProperty ("vx_profile", prof, nullptr);
    vx.state.setProperty ("vx_d0", juce::String (d0.lufs, 1) + ";" + juce::String (juce::roundToInt (d0.centroid)) + ";" + juce::String (d0.crest, 1) + ";" + juce::String (d0.midShare, 1) + ";" + juce::String (d0.airShare, 2), nullptr);
    vx.state.setProperty ("vx_spec0", specToString (d0.spec), nullptr);
    // natijani o'lchash: AZ VOX yoqilgan holda
    setEn (vx, true);
    juce::AudioBuffer<float> b1; double sr1 = sr;
    if (renderTrackTo (*t, range, tmp) && readWav (tmp, b1, sr1))
    {
        auto d1 = voxDiag (b1, sr1);
        // daraja kalibrlash: haqiqiy natijani o'lchab, chiqishni -18 LUFS ga aniq keltirish
        if (d1.lufs > -69 && std::abs (d1.lufs + 18.0) > 0.4)
        {
            vx.set ("outGain", std::round (juce::jlimit (-24.0, 30.0, vx.get ("outGain") + (-18.0 - d1.lufs)) * 10) / 10);
            if (renderTrackTo (*t, range, tmp) && readWav (tmp, b1, sr1)) d1 = voxDiag (b1, sr1);
        }
        vx.state.setProperty ("vx_d1", juce::String (d1.lufs, 1) + ";" + juce::String (juce::roundToInt (d1.centroid)) + ";" + juce::String (d1.crest, 1) + ";" + juce::String (d1.midShare, 1) + ";" + juce::String (d1.airShare, 2), nullptr);
        vx.state.setProperty ("vx_spec1", specToString (d1.spec), nullptr);
    }
    for (int i = 0; i < plugs.size(); ++i) setEn (*plugs[i], plugs[i] == &vx ? true : orig[(size_t) i]);
    tmp.deleteFile();
    t->edit.restartPlayback();
    return U ("AZ VOX ANALIZ: ") + prof + U ("  \xc2\xb7  ") + juce::String (d0.lufs, 1) + U (" LUFS \xe2\x86\x92 sozlandi");
}

// ---------------------------------------------------------------- Oyna (maketdagi tuzilish)
class VoxEditor : public te::Plugin::EditorComponent, private juce::Timer
{
public:
    explicit VoxEditor (AzVoxPlugin& p) : vx (p)
    {
        struct K { const char* key; const char* name; double lo, hi, step; const char* unit; };
        static const K stageKnobs[] = {
            { "hpf", "HPF", 30, 250, 1, " Hz" },     { "breath", "Nafas", -24, 0, 0.5, " dB" },
            { "match", "Match", 0, 100, 1, " %" },   { "resAmt", "Rezonans", 0, 100, 1, " %" },
            { "pres", "Presence", 0, 100, 1, " %" }, { "air", "Air", 0, 100, 1, " %" },
            { "dsHz", "Chastota", 4000, 10000, 100, " Hz" }, { "dsMax", "Maks", -12, 0, 0.5, " dB" },
            { "fetOff", "FET", -12, 12, 0.5, " dB" },  { "opOff", "Opto", -12, 12, 0.5, " dB" } };
        static const K macroKnobs[] = {
            { "mClean", "CLEAN", 0, 100, 1, " %" }, { "mWarm", "WARMTH", 0, 100, 1, " %" }, { "mBright", "BRIGHT", 0, 100, 1, " %" },
            { "mPunch", "PUNCH", 0, 100, 1, " %" }, { "mix", "MIX", 0, 100, 1, " %" }, { "trim", "OUTPUT", -12, 12, 0.1, " dB" } };
        for (auto& k : stageKnobs) addKnob (k.key, k.name, k.lo, k.hi, k.step, k.unit, false);
        for (auto& k : macroKnobs) addKnob (k.key, k.name, k.lo, k.hi, k.step, k.unit, true);
        for (int i = 0; i < 5; ++i)
        {
            auto* b = power.add (new juce::TextButton());
            b->setClickingTogglesState (true);
            b->onClick = [this, i, b] { vx.set (("on" + juce::String (i + 1)).toRawUTF8(), b->getToggleState() ? 1 : 0); b->setButtonText (b->getToggleState() ? "ON" : "OFF"); repaint(); };
            addAndMakeVisible (b);
        }
        for (auto* b : { &analyseB, &abB })
            addAndMakeVisible (b);
        analyseB.setButtonText (U ("\xe2\x9a\xa1 ANALIZ"));
        analyseB.setToggleState (true, juce::dontSendNotification);
        analyseB.onClick = [this] { runAnalyse(); };
        abB.setButtonText ("A / B"); abB.setClickingTogglesState (true);
        abB.setTooltip (U ("A/B: asl ovoz bilan solishtirish (AZ VOX vaqtincha o'chadi)"));
        abB.onClick = [this] { vx.set ("ab", abB.getToggleState() ? 1 : 0, false); };
        analyseB.setTooltip (U ("Trek audiosini tahlil qiladi va barcha bosqichlarni avtomatik sozlaydi (lokatorlar yoki birinchi 45 s)"));
        setSize (1180, 720);
        sync(); startTimerHz (20);
    }
    bool allowWindowResizing() override { return false; }
    juce::ComponentBoundsConstrainer* getBoundsConstrainer() override { return nullptr; }

    void runAnalyse()
    {
        status = U ("Tahlil qilinmoqda..."); repaint();
        juce::Component::SafePointer<VoxEditor> sp (this);
        juce::MessageManager::callAsync ([sp]
        {
            if (sp == nullptr) return;
            const auto msg = analyseVox (sp->vx);
            if (sp != nullptr) { sp->status = msg; sp->sync(); sp->repaint(); }
        });
    }

    void resized() override
    {
        analyseB.setBounds (getWidth() - 270, 18, 110, 36);
        abB.setBounds (getWidth() - 150, 18, 64, 36);
        // bosqich kartalari
        const int cy = 368, cw = (getWidth() - 40 - 4 * 10) / 5;
        for (int i = 0; i < 5; ++i)
        {
            const int x = 20 + i * (cw + 10);
            power[i]->setBounds (x + cw - 46, cy + 9, 36, 19);
            knobs[i * 2]->setBounds (x + 12, cy + 58, 70, 70);
            knobs[i * 2 + 1]->setBounds (x + cw - 82, cy + 58, 70, 70);
        }
        // makrolar
        const int my = 560;
        for (int i = 0; i < 6; ++i) knobs[10 + i]->setBounds (32 + i * 118, my + 22, 84, 84);
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        g.fillAll (bg0);
        auto win = getLocalBounds().toFloat().reduced (4);
        g.setColour (bg1); g.fillRoundedRectangle (win, 14);
        g.setColour (red.withAlpha (0.8f)); g.drawRoundedRectangle (win.reduced (0.75f), 14, 1.5f);
        // sarlavha
        logoMark (g, { 20, 16, 40, 40 });
        label (g, "AZ VOX", { 70, 14, 120, 26 }, 21, text, Justification::centredLeft, 800);
        label (g, U ("\xc2\xb7 Suno Glue"), { 152, 14, 160, 26 }, 21, red, Justification::centredLeft, 800);
        caps (g, U ("AVTOMATIK VOKAL OBROBOTKASI \xc2\xb7 AZIMXODJAYEV MUSIC"), { 70, 40, 360, 12 }, 8.5f, dim);
        auto chip = [&] (Rectangle<float> r, const String& cap, const String& v)
        {
            panel (g, r, 8); caps (g, cap, r.reduced (12, 6).withHeight (10), 8, dim);
            label (g, v, r.reduced (12, 0).withTrimmedTop (18), 11.5f, text, Justification::centredLeft, 600);
        };
        chip ({ 430, 16, 250, 40 }, U ("OVOZ PROFILI"), vx.state["vx_profile"].toString().isEmpty() ? U ("ANALIZ bosing") : vx.state["vx_profile"].toString());
        chip ({ 690, 16, 200, 40 }, U ("MAQSAD"), U ("Zamonaviy pop vokal"));
        // spektr
        auto sp = Rectangle<float> (20, 70, 760, 284);
        panel (g, sp, 12);
        caps (g, U ("SPEKTR"), { sp.getX() + 14, sp.getY() + 10, 200, 12 });
        auto pl = sp.reduced (42, 30).withTrimmedRight (-28).withTrimmedBottom (-6);
        auto X = [&] (double f) { return pl.getX() + pl.getWidth() * (float) (std::log (f / 30.0) / std::log (18000.0 / 30.0)); };
        auto Y = [&] (double db) { return pl.getY() + pl.getHeight() * (float) (-juce::jlimit (-56.0, 2.0, db) / 56.0); };
        for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
        {
            g.setColour (Colours::white.withAlpha (0.05f)); g.drawVerticalLine ((int) X (f), pl.getY(), pl.getBottom());
            label (g, tt::hzText (f), { X (f) - 20, pl.getBottom() + 2, 40, 12 }, 9, dim2, Justification::centred);
        }
        for (int db = 0; db >= -48; db -= 12)
        {
            g.setColour (Colours::white.withAlpha (0.05f)); g.drawHorizontalLine ((int) Y (db), pl.getX(), pl.getRight());
            label (g, String (db), { pl.getX() - 34, Y (db) - 6, 28, 12 }, 9, dim2, Justification::centredRight);
        }
        auto curve = [&] (const std::vector<float>& v) { Path p; for (size_t i = 0; i < v.size(); ++i) { const double f = 30.0 * std::pow (18000.0 / 30.0, (double) i / (v.size() - 1)); const auto pt = Point<float> (X (f), Y (v[i])); if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt); } return p; };
        const auto s0 = specFromString (vx.state["vx_spec0"].toString()), s1 = specFromString (vx.state["vx_spec1"].toString());
        if (s0.size() > 10)
        {
            // maqsad: s0 bilan 300 Hz da hizalangan
            std::vector<float> tg (s0.size());
            const int k300 = (int) std::round (59.0 * std::log (300.0 / 30.0) / std::log (600.0));
            for (size_t i = 0; i < tg.size(); ++i) tg[i] = (float) targetCurve (30.0 * std::pow (600.0, (double) i / 59.0), true);
            const float off = s0[(size_t) k300] - tg[(size_t) k300]; for (auto& v : tg) v += off;
            if (s1.size() == s0.size())
            {
                auto fill = curve (s1); fill.lineTo (X (18000), pl.getBottom()); fill.lineTo (X (30), pl.getBottom()); fill.closeSubPath();
                g.setGradientFill (ColourGradient (red.withAlpha (0.35f), 0, pl.getY(), red.withAlpha (0.0f), 0, pl.getBottom(), false)); g.fillPath (fill);
            }
            g.setColour (Colour (0xff9b9890)); g.strokePath (curve (s0), PathStrokeType (1.8f));
            Path dashed; float dl[] = { 6, 5 }; PathStrokeType (1.4f).createDashedStroke (dashed, curve (tg), dl, 2);
            g.setColour (Colours::white.withAlpha (0.7f)); g.fillPath (dashed);
            if (s1.size() == s0.size()) { g.setColour (red); g.strokePath (curve (s1), PathStrokeType (2.4f)); }
        }
        else label (g, U ("\xe2\x9a\xa1 ANALIZ bosing \xe2\x80\x94 vokal tahlil qilinadi va barcha bosqichlar avtomatik sozlanadi"), pl, 12, dim, Justification::centred, 500);
        // legenda
        auto leg = [&] (float x, Colour c, const String& t2, bool dash) { g.setColour (c); if (dash) { g.fillRect (x, sp.getY() + 15, 5.0f, 2.0f); g.fillRect (x + 8, sp.getY() + 15, 5.0f, 2.0f); } else g.fillRect (x, sp.getY() + 15, 14.0f, 2.5f);
                                                             label (g, t2, { x + 18, sp.getY() + 8, 120, 16 }, 9.5f, silver, Justification::centredLeft, 500); };
        leg (sp.getRight() - 330, Colour (0xff9b9890), U ("Hozirgi"), false);
        leg (sp.getRight() - 250, Colours::white, U ("Maqsad"), true);
        leg (sp.getRight() - 170, red, U ("Natija (AZ VOX)"), false);
        // diagnoz kartalari
        const auto d0 = StringArray::fromTokens (vx.state["vx_d0"].toString(), ";", {}), d1 = StringArray::fromTokens (vx.state["vx_d1"].toString(), ";", {});
        struct C { const char* cap; int idx; const char* unit; const char* goal; };
        static const C cards[] = { { "BALANDLIK (LUFS)", 0, "", "-18" }, { "YORQINLIK (CENTROID)", 1, " Hz", "~1.5k" }, { "DINAMIKA (CREST)", 2, " dB", "9-14" },
                                   { "200 Hz-1 kHz ULUSHI", 3, " %", "~55%" }, { "PRESENCE/AIR 2.5k+", 4, " %", "~10%" } };
        for (int i = 0; i < 5; ++i)
        {
            auto r = Rectangle<float> (792, 70.0f + i * 58.0f, getWidth() - 812.0f, 50);
            panel (g, r, 9);
            caps (g, cards[i].cap, r.reduced (12, 7).withHeight (10), 8, dim);
            const String a = d0.size() > cards[i].idx ? d0[cards[i].idx] + cards[i].unit : String ("-");
            const String b = d1.size() > cards[i].idx ? d1[cards[i].idx] + cards[i].unit : String ("-");
            label (g, a, r.reduced (12, 0).withTrimmedTop (20).withWidth (110), 14, Colour (0xffff7a7a), Justification::centredLeft, 700);
            label (g, U ("\xe2\x86\x92 ") + b, r.reduced (12, 0).withTrimmedTop (20).withTrimmedLeft (104), 15, red, Justification::centredLeft, 800);
            label (g, U ("me'yor ") + cards[i].goal, r.reduced (12, 0).withTrimmedTop (20), 9, dim, Justification::centredRight, 500);
        }
        // bosqich kartalari
        static const char* names[] = { "CLEAN", "TONE MATCH", "HARMONIC", "DE-ESS", "COMP x2" };
        static const char* descs[] = { "HPF, nafas va shovqin ekspanderi", "Maqsad egriga 7 polosa + rezonans kesish", "Presence / air'ni garmonika bilan tiklash",
                                       "Dinamik, faqat s/sh paytida", "FET (tez 4:1) -> Opto (silliq 2:1)" };
        const int cy = 368, cw = (getWidth() - 40 - 4 * 10) / 5;
        for (int i = 0; i < 5; ++i)
        {
            auto r = Rectangle<float> (20.0f + i * (cw + 10), (float) cy, (float) cw, 176);
            const bool onS = power[i]->getToggleState();
            panel (g, r, 11); if (onS) { g.setColour (red.withAlpha (0.35f)); g.drawRoundedRectangle (r.reduced (0.5f), 11, 1); }
            auto nb = Rectangle<float> (r.getX() + 12, r.getY() + 11, 17, 17);
            g.setColour (red); g.fillRoundedRectangle (nb, 4);
            label (g, String (i + 1), nb, 10, flatTheme() ? Colour (0xff0a0a0a) : Colours::white, Justification::centred, 800);
            label (g, names[i], { r.getX() + 36, r.getY() + 10, r.getWidth() - 150, 19 }, 12, onS ? text : dim, Justification::centredLeft, 800);
            label (g, U (descs[i]), { r.getX() + 12, r.getY() + 32, r.getWidth() - 20, 24 }, 9.5f, dim, Justification::topLeft, 400);
            for (int k = 0; k < 2; ++k)
            {
                auto* kn = knobs[i * 2 + k];
                label (g, kn->getTextFromValue (kn->getValue()), { (float) kn->getX() - 10, (float) kn->getBottom() + 2, (float) kn->getWidth() + 20, 14 }, 10.5f, text, Justification::centred, 700);
                label (g, kn->getName(), { (float) kn->getX() - 10, (float) kn->getBottom() + 16, (float) kn->getWidth() + 20, 12 }, 9, dim, Justification::centred, 500);
            }
            // jonli ko'rsatkich
            const float v = i == 0 ? vx.mGate.load() : i == 3 ? vx.mDs.load() : i == 4 ? vx.mFet.load() + vx.mOp.load() : 0.0f;
            if (i == 0 || i == 3 || i == 4)
            {
                auto bar = Rectangle<float> (r.getX() + 12, r.getBottom() - 12, r.getWidth() - 24, 4);
                g.setColour (Colours::white.withAlpha (0.06f)); g.fillRoundedRectangle (bar, 2);
                g.setColour (red); g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * jlimit (0.0f, 1.0f, -v / 18.0f)), 2);
                label (g, (i == 0 ? U ("Gate ") : i == 3 ? U ("De-ess ") : U ("GR ")) + String (v, 1) + " dB", { r.getRight() - 150, r.getY() + 10, 98, 19 }, 9, v < -0.5f ? red : dim, Justification::centredRight, 700);
            }
            const auto tag = Rectangle<float> (r.getRight() - 50, r.getBottom() - 8, 40, 14);
            juce::ignoreUnused (tag);
        }
        // makrolar
        auto mr = Rectangle<float> (20, 552, 740, 156);
        panel (g, mr, 12);
        caps (g, U ("MAKRO \xc2\xb7 BITTA RUCHKA \xe2\x80\x94 BUTUN ZANJIR"), { mr.getX() + 14, mr.getY() + 10, 400, 12 });
        for (int i = 10; i < 16; ++i)
        {
            auto* kn = knobs[i];
            label (g, kn->getName(), { (float) kn->getX() - 10, (float) kn->getBottom() + 2, (float) kn->getWidth() + 20, 14 }, 10, text, Justification::centred, 800);
            label (g, kn->getTextFromValue (kn->getValue()), { (float) kn->getX() - 10, (float) kn->getBottom() + 16, (float) kn->getWidth() + 20, 12 }, 9.5f, dim, Justification::centred, 500);
        }
        // metrlar
        auto me = Rectangle<float> (772, 552, getWidth() - 792.0f, 156);
        panel (g, me, 12);
        caps (g, U ("METR"), { me.getX() + 14, me.getY() + 10, 100, 12 });
        auto meter = [&] (int i, const String& n, float frac)
        {
            auto r = Rectangle<float> (me.getX() + 20 + i * 44, me.getY() + 32, 18, 96);
            g.setColour (Colours::white.withAlpha (0.06f)); g.fillRoundedRectangle (r, 4);
            auto f = r.withTrimmedTop (r.getHeight() * (1 - jlimit (0.0f, 1.0f, frac)));
            g.setGradientFill (ColourGradient (red, 0, r.getBottom(), red.brighter (0.4f), 0, r.getY(), false)); g.fillRoundedRectangle (f, 4);
            label (g, n, { r.getX() - 10, r.getBottom() + 4, 38, 12 }, 8.5f, dim, Justification::centred, 600);
        };
        auto lv = [] (float a) { return (juce::Decibels::gainToDecibels (a, -60.0f) + 60.0f) / 60.0f; };
        meter (0, "IN", lv (vx.mIn.load())); meter (1, "GR", -(vx.mFet.load() + vx.mOp.load()) / 18.0f); meter (2, "OUT", lv (vx.mOut.load()));
        g.setColour (silver); g.setFont (font (10.5f, 500));
        g.drawFittedText (status, Rectangle<float> (me.getX() + 150, me.getY() + 30, me.getWidth() - 162, 110).toNearestInt(), Justification::topLeft, 7, 1.0f);
    }

private:
    AzVoxPlugin& vx;
    juce::OwnedArray<juce::Slider> knobs; juce::StringArray keys;
    juce::OwnedArray<juce::TextButton> power;
    juce::TextButton analyseB, abB;
    juce::String status = U ("Tayyor. \xe2\x9a\xa1 ANALIZ \xe2\x80\x94 avtomatik sozlash. Har bir ruchkani qo'lda ham o'zgartirish mumkin.");

    void addKnob (const char* key, const char* name, double lo, double hi, double step, const char* unit, bool big)
    {
        auto* s = knobs.add (new juce::Slider (juce::String::fromUTF8 (name)));
        s->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag); s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s->setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        s->setRange (lo, hi, step); s->setTextValueSuffix (unit);
        s->setColour (juce::Slider::rotarySliderFillColourId, red);
        if (lo < 0 && hi > 0) s->getProperties().set ("bipolar", true);
        juce::ignoreUnused (big);
        keys.add (key);
        const juce::String k (key);
        s->onValueChange = [this, s, k] { vx.set (k.toRawUTF8(), s->getValue()); repaint(); };
        s->onDragStart = [this] { vx.edit.getUndoManager().beginNewTransaction ("AZ VOX"); };
        addAndMakeVisible (s);
    }
    void sync()
    {
        for (int i = 0; i < knobs.size(); ++i) knobs[i]->setValue (vx.get (keys[i].toRawUTF8()), juce::dontSendNotification);
        for (int i = 0; i < 5; ++i) { power[i]->setToggleState ((int) vx.get (("on" + juce::String (i + 1)).toRawUTF8()) != 0, juce::dontSendNotification); power[i]->setButtonText (power[i]->getToggleState() ? "ON" : "OFF"); }
        abB.setToggleState ((int) vx.get ("ab") != 0, juce::dontSendNotification);
    }
    void timerCallback() override { repaint (772, 552, getWidth() - 792, 156); for (int i : { 0, 3, 4 }) { const int cw = (getWidth() - 80) / 5; repaint (20 + i * (cw + 10), 368, cw, 176); } }
};
inline std::unique_ptr<te::Plugin::EditorComponent> AzVoxPlugin::createEditor() { return std::make_unique<VoxEditor> (*this); }
}
