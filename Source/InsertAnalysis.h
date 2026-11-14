#pragma once
// AZ MUS — insert zanjiri tahlili: har plagin signalni yaxshilayaptimi yoki yomonlashtiryaptimi.
// Trek offline render qilinadi: avval barcha insertlar bypass, keyin ketma-ket bittadan yoqiladi.
// Har bosqich mixing/mastering me'yorlariga ko'ra 0..100 ball oladi; plagin hissasi = ball farqi (+ yashil, − qizil).
// Ballar balandlikka bog'liq emas (normallashtiriladi) — plagin shunchaki balandroq qilsa, "yaxshi" hisoblanmaydi.
#include "AppEngine.h"
#include "TrackTools.h"

namespace az
{
struct MixScore
{
    double total = 0, tonal = 0, dyn = 0, peak = 0, noise = 0, harsh = 0, stereo = 0;
    double devDb = 0, crestDb = 0, peakDb = -200, snrDb = 0, harshDb = -60, corr = 1;
    bool valid = false;
};

// maqsad tonal egrilari (nisbiy dB): vokal — zamonaviy pop vokal; mix — ~-4.5 dB/okt "pushti" balans
inline double targetCurve (double f, bool vocal)
{
    if (vocal)
    {
        double t = 0;
        if (f < 100) t -= 24 * std::log2 (100 / f);
        if (f > 600) t -= 2.4 * std::log2 (f / 600);
        t += 4.0 * std::exp (-std::pow (std::log2 (f / 3200), 2) / 0.6) + 3.5 * std::exp (-std::pow (std::log2 (f / 11000), 2) / 0.6)
           - 1.5 * std::exp (-std::pow (std::log2 (f / 420), 2) / 0.3);
        if (f > 15000) t -= 12 * std::log2 (f / 15000);
        return t;
    }
    double t = -4.5 * std::log2 (juce::jmax (60.0, f) / 1000.0);
    if (f < 35) t -= 18 * std::log2 (35 / f);
    if (f > 14000) t -= 10 * std::log2 (f / 14000);
    return t;
}

inline MixScore scoreBuffer (const juce::AudioBuffer<float>& b, double sr, bool vocal)
{
    MixScore s;
    const int n = b.getNumSamples(), nc = juce::jmin (2, b.getNumChannels());
    if (n < 4096 || nc == 0 || sr <= 0) return s;
    constexpr int order = 11, N = 1 << order, hop = N / 2;
    juce::dsp::FFT fft (order);
    std::vector<float> win (N), buf (2 * N);
    for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * i / (N - 1));
    // 1/3 oktava bandlari 50 Hz .. 16 kHz
    std::vector<double> bandF; for (double f = 50; f <= 16000; f *= std::pow (2.0, 1.0 / 3.0)) bandF.push_back (f);
    std::vector<double> bandE (bandF.size(), 0.0);
    std::vector<double> frameDb, harshR;
    double peak = 0; int clipped = 0;
    for (int c = 0; c < nc; ++c) { auto* d = b.getReadPointer (c); for (int i = 0; i < n; ++i) { const double a = std::abs (d[i]); peak = juce::jmax (peak, a); clipped += a >= 0.999; } }
    // kadrlar
    std::vector<double> rmsFrames;
    for (int s0 = 0; s0 + N <= n; s0 += hop)
    {
        double e = 0;
        for (int i = 0; i < N; ++i)
        {
            float v = 0; for (int c = 0; c < nc; ++c) v += b.getSample (c, s0 + i); v /= (float) nc;
            buf[(size_t) i] = v * win[(size_t) i]; e += (double) v * v;
        }
        rmsFrames.push_back (10 * std::log10 (e / N + 1e-20));
    }
    if (rmsFrames.empty()) return s;
    const double mx = *std::max_element (rmsFrames.begin(), rmsFrames.end());
    std::vector<double> active;
    double sumActive = 0;
    int fi = 0;
    for (int s0 = 0; s0 + N <= n; s0 += hop, ++fi)
    {
        if (rmsFrames[(size_t) fi] < mx - 40) continue;                  // faol kadrlar (sukunat hisobga olinmaydi)
        active.push_back (rmsFrames[(size_t) fi]); sumActive += std::pow (10.0, rmsFrames[(size_t) fi] / 10);
        std::fill (buf.begin(), buf.end(), 0.0f);
        for (int i = 0; i < N; ++i) { float v = 0; for (int c = 0; c < nc; ++c) v += b.getSample (c, s0 + i); buf[(size_t) i] = v / (float) nc * win[(size_t) i]; }
        fft.performFrequencyOnlyForwardTransform (buf.data());
        double tot = 0, hs = 0;
        for (int k = 1; k < N / 2; ++k)
        {
            const double f = k * sr / N, p = (double) buf[(size_t) k] * buf[(size_t) k];
            tot += p; if (f >= 4500 && f <= 9500) hs += p;
            if (f < 44 || f > 18000) continue;
            const int bi = juce::jlimit (0, (int) bandF.size() - 1, (int) std::round (3.0 * std::log2 (f / 50.0)));
            bandE[(size_t) bi] += p;
        }
        if (tot > 0) harshR.push_back (10 * std::log10 (hs / tot + 1e-12));
    }
    if (active.size() < 4) return s;
    // 1) tonal balans: o'rtacha 200 Hz–5 kHz bo'yicha normallashtirilgan og'ish
    std::vector<double> meas (bandF.size()), tgt (bandF.size());
    double mM = 0, mT = 0; int cnt = 0;
    for (size_t i = 0; i < bandF.size(); ++i)
    {
        meas[i] = 10 * std::log10 (bandE[i] + 1e-20); tgt[i] = targetCurve (bandF[i], vocal);
        if (bandF[i] >= 200 && bandF[i] <= 5000) { mM += meas[i]; mT += tgt[i]; ++cnt; }
    }
    mM /= cnt; mT /= cnt;
    double dev = 0, wsum = 0;
    for (size_t i = 0; i < bandF.size(); ++i)
    {
        const double w = bandF[i] < 80 || bandF[i] > 12000 ? 0.4 : 1.0;
        const double d = juce::jlimit (-18.0, 18.0, (meas[i] - mM) - (tgt[i] - mT));
        dev += w * d * d; wsum += w;
    }
    s.devDb = std::sqrt (dev / wsum);
    if (juce::SystemStats::getEnvironmentVariable ("AZ_TAHLIL_DEBUG", {}).isNotEmpty())
    {
        juce::String o; for (size_t i = 0; i < bandF.size(); i += 2) o << juce::roundToInt (bandF[i]) << ":" << juce::roundToInt ((meas[i] - mM) - (tgt[i] - mT)) << " ";
        std::fprintf (stderr, "dev %.1f | %s\n", s.devDb, o.toRawUTF8());
    }
    s.tonal = 100.0 * std::exp (-s.devDb / 10.0);              // 0 dB = 100, 5 dB = 61, 10 dB = 37, 20 dB = 14
    // 2) dinamika: crest (pik / faol RMS)
    const double rmsA = 10 * std::log10 (sumActive / (double) active.size() + 1e-20);
    s.peakDb = 20 * std::log10 (peak + 1e-12);
    s.crestDb = s.peakDb - rmsA;
    const double lo = vocal ? 9 : 7, hi = vocal ? 14 : 12;
    const double dist = s.crestDb < lo ? lo - s.crestDb : s.crestDb > hi ? s.crestDb - hi : 0;
    s.dyn = 100.0 * std::exp (-dist / 6.0);
    // 3) pik / klip
    s.peak = clipped > 2 ? 0.0 : s.peakDb > -0.1 ? 20.0 : s.peakDb > -1.0 ? 100.0 - 80.0 * (s.peakDb + 1.0) / 0.9 : 100.0;
    // 4) shovqin: faol daraja va sukunat pastki qismi farqi (raqamli sukunat hisobga olinmaydi)
    std::vector<double> nonSilent; for (auto v : rmsFrames) if (v > -140) nonSilent.push_back (v);
    std::sort (nonSilent.begin(), nonSilent.end()); std::sort (active.begin(), active.end());
    const double floorDb = nonSilent.empty() ? -200 : nonSilent[(size_t) (nonSilent.size() * 0.1)];
    s.snrDb = active[active.size() / 2] - floorDb;
    s.noise = juce::jlimit (0.0, 100.0, (s.snrDb - 25.0) / 30.0 * 100.0);
    // 5) keskinlik / sibilans: 4.5–9.5 kHz ulushining 95-persentili
    //    me'yor maqsad egrisidan: vokal ~ -12.6 dB, mix ~ -31.6 dB (+5 dB tolerantlik); sibilans = p95 - mediana (14 dB gacha normal)
    if (! harshR.empty())
    {
        std::sort (harshR.begin(), harshR.end());
        const double med = harshR[harshR.size() / 2], p95 = harshR[(size_t) (harshR.size() * 0.95)];
        s.harshDb = med;
        const double expected = vocal ? -12.6 : -31.6;
        s.harsh = 100.0 * std::exp (-juce::jmax (0.0, med - (expected + 5.0)) / 6.0) * std::exp (-juce::jmax (0.0, (p95 - med) - 14.0) / 8.0);
    }
    else s.harsh = 100.0;
    // 6) stereo / faza
    if (nc == 2)
    {
        double lr = 0, ll = 0, rr = 0;
        auto* L = b.getReadPointer (0); auto* R = b.getReadPointer (1);
        for (int i = 0; i < n; ++i) { lr += (double) L[i] * R[i]; ll += (double) L[i] * L[i]; rr += (double) R[i] * R[i]; }
        s.corr = ll > 0 && rr > 0 ? lr / std::sqrt (ll * rr) : 1.0;
    }
    s.stereo = s.corr >= 0.3 ? 100.0 : s.corr >= 0 ? 70.0 + 100.0 * s.corr : juce::jmax (0.0, 70.0 + 70.0 * s.corr);
    s.total = 0.35 * s.tonal + 0.20 * s.dyn + 0.15 * s.peak + 0.10 * s.noise + 0.10 * s.harsh + 0.10 * s.stereo;
    s.valid = true;
    return s;
}

// render paytida progress oynasi ichki xabar tsiklini aylantiradi — tahlil qayta chaqirilmasin
inline bool& analysisBusy() { static bool b = false; return b; }
struct BusyScope { bool ok; BusyScope() : ok (! analysisBusy()) { if (ok) analysisBusy() = true; } ~BusyScope() { if (ok) analysisBusy() = false; } };

inline bool isVocalTrack (te::AudioTrack& t)
{
    const auto n = t.getName().toLowerCase();
    return n.contains ("vox") || n.contains ("vocal") || n.contains ("vokal") || n.contains ("voc") || n.contains ("ovoz") || n.contains (juce::String::fromUTF8 ("\xd0\xb2\xd0\xbe\xd0\xba")) || (trackIsMono (t) && ! AppEngine::isInstrumentTrack (t));
}

// zanjir imzosi: plaginlar tartibi va yoqilganligi o'zgarsa, eski natija "eskirgan" deb ko'rsatiladi
inline juce::String insertSignature (te::AudioTrack& t)
{
    juce::String s;
    for (auto* p : AppEngine::userPlugins (t)) s << p->itemID.toString() << (p->isEnabled() ? "+" : "-") << ";";
    return s;
}

// tahlil oralig'i: lokatorlar (agar audio bilan kesishsa) yoki kursor / birinchi klipdan 20 s
inline te::TimeRange analysisRange (te::AudioTrack& t)
{
    auto& tr = t.edit.getTransport();
    te::TimePosition a = te::TimePosition::fromSeconds (1e9), b;
    for (auto* c : t.getClips()) { a = juce::jmin (a, c->getPosition().getStart()); b = juce::jmax (b, c->getPosition().getEnd()); }
    if (b <= a) return {};
    const auto lr = tr.getLoopRange();
    if (lr.getLength() >= 2s && lr.getStart() < b && lr.getEnd() > a)
        return { juce::jmax (a, lr.getStart()), juce::jmin (b, juce::jmin (lr.getEnd(), lr.getStart() + 30s)) };
    auto st = tr.getPosition() >= a && tr.getPosition() < b - 2s ? tr.getPosition() : a;
    return { st, juce::jmin (b, st + 20s) };
}

inline bool renderTrackTo (te::AudioTrack& t, te::TimeRange range, const juce::File& out)
{
    auto& e = t.edit; auto& eng = e.engine; auto& dm = eng.getDeviceManager();
    te::Renderer::Parameters r (e);
    r.destFile = out; r.audioFormat = eng.getAudioFileFormatManager().getWavFormat(); r.bitDepth = 32;
    r.sampleRateForAudio = dm.getSampleRate() > 7000 ? dm.getSampleRate() : 48000.0;
    r.blockSizeForAudio  = dm.getBlockSize() > 0 ? dm.getBlockSize() : 512;
    r.time = range;
    juce::BigInteger bits; bits.setBit (te::getAllTracks (e).indexOf (&t)); r.tracksToDo = bits;
    r.usePlugins = true; r.useMasterPlugins = false;
    out.deleteFile();
    ScopedControlRoomBypass crb (e);
    te::Renderer::renderToFile ("AZ insert tahlili", r);
    return out.existsAsFile() && out.getSize() > 1000;
}

inline bool readWav (const juce::File& f, juce::AudioBuffer<float>& b, double& sr)
{
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> rd (wav.createReaderFor (f.createInputStream().release(), true));
    if (rd == nullptr) return false;
    b.setSize ((int) juce::jmin (2u, rd->numChannels), (int) rd->lengthInSamples);
    rd->read (&b, 0, (int) rd->lengthInSamples, 0, true, rd->numChannels > 1);
    sr = rd->sampleRate; return true;
}

inline juce::String scoreText (const MixScore& s)
{
    return juce::String (juce::roundToInt (s.total)) + " ball  (tonal " + juce::String (juce::roundToInt (s.tonal))
         + ", dinamika " + juce::String (juce::roundToInt (s.dyn)) + ", pik " + juce::String (juce::roundToInt (s.peak))
         + ", shovqin " + juce::String (juce::roundToInt (s.noise)) + ", keskinlik " + juce::String (juce::roundToInt (s.harsh))
         + ", faza " + juce::String (juce::roundToInt (s.stereo)) + ")";
}

// natija: har plagin state'ida "azDelta" (ball farqi), "azDeltaInfo" (izoh); trekda "azInsSig", "azInsBase", "azInsFinal"
inline juce::String analyseInserts (AppEngine& app, te::AudioTrack& t, std::function<void (float)> progress = nullptr)
{
    BusyScope busy; if (! busy.ok) return U ("Tahlil allaqachon bajarilmoqda...");
    auto plugs = AppEngine::userPlugins (t);
    if (plugs.isEmpty()) return U ("Insert yo'q \xe2\x80\x94 tahlil qilinadigan narsa yo'q");
    const auto range = analysisRange (t);
    if (range.getLength() < 1s) return U ("Trekda audio yo'q");
    t.edit.getTransport().stop (false, true);
    const bool vocal = isVocalTrack (t);
    std::vector<bool> orig; for (auto* p : plugs) orig.push_back (p->isEnabled());
    auto setEn = [] (te::Plugin& p, bool on) { p.state.setProperty (te::IDs::enabled, on, nullptr); };   // undo tarixiga tushmaydi
    const auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("az_insert_tahlil.wav");
    auto measure = [&] () -> MixScore
    {
        juce::AudioBuffer<float> b; double sr = 48000;
        if (! renderTrackTo (t, range, tmp) || ! readWav (tmp, b, sr)) return {};
        return scoreBuffer (b, sr, vocal);
    };
    for (auto* p : plugs) setEn (*p, false);
    auto prev = measure();
    const auto base = prev;
    int steps = 0, total = 0; for (bool o : orig) total += o;
    for (int i = 0; i < plugs.size(); ++i)
    {
        auto* p = plugs[i];
        if (! orig[(size_t) i]) { p->state.removeProperty ("azDelta", nullptr); p->state.setProperty ("azDeltaInfo", U ("Bypass \xe2\x80\x94 baholanmadi"), nullptr); continue; }
        setEn (*p, true);
        const auto cur = measure();
        if (cur.valid && prev.valid)
        {
            const double d = cur.total - prev.total;
            auto part = [] (const char* n, double a, double b) { const int v = juce::roundToInt (b - a); return v == 0 ? juce::String() : juce::String (n) + (v > 0 ? " +" : " ") + juce::String (v) + "  "; };
            juce::String info = part ("tonal", prev.tonal, cur.tonal) + part ("dinamika", prev.dyn, cur.dyn) + part ("pik", prev.peak, cur.peak)
                              + part ("shovqin", prev.noise, cur.noise) + part ("keskinlik", prev.harsh, cur.harsh) + part ("faza", prev.stereo, cur.stereo);
            p->state.setProperty ("azDelta", d, nullptr);
            p->state.setProperty ("azDeltaInfo", (info.isEmpty() ? U ("Sezilarli o'zgarish yo'q") : info.trim()) + "\n" + U ("Keyin: ") + scoreText (cur), nullptr);
        }
        prev = cur;
        if (progress) progress ((float) ++steps / (float) juce::jmax (1, total));
    }
    for (int i = 0; i < plugs.size(); ++i) setEn (*plugs[i], orig[(size_t) i]);
    tmp.deleteFile();
    t.state.setProperty ("azInsSig", insertSignature (t), nullptr);
    t.state.setProperty ("azInsBase", base.valid ? base.total : -1.0, nullptr);
    t.state.setProperty ("azInsFinal", prev.valid ? prev.total : -1.0, nullptr);
    t.state.setProperty ("azInsTarget", vocal ? "vokal" : "mix", nullptr);
    t.edit.restartPlayback();
    if (! base.valid || ! prev.valid) return U ("Tahlil: render bajarilmadi");
    const int d = juce::roundToInt (prev.total - base.total);
    return U ("Insert tahlili (") + (vocal ? U ("vokal me'yori") : U ("mix me'yori")) + "): " + juce::String (juce::roundToInt (base.total)) + U (" \xe2\x86\x92 ")
         + juce::String (juce::roundToInt (prev.total)) + " ball (" + (d >= 0 ? "+" : "") + juce::String (d) + ")";
}
}
