// =============================================================================
//  AZ MUS — AZ Pitch: vokal tahlili va tahriri (VariAudio / Melodyne uslubi)
//  · YIN (FFT orqali) bilan asosiy ton egri chizig'i, notalarga bo'lish
//  · TD-PSOLA bilan pitch o'zgartirish (formantlar saqlanadi — ovoz "multfilm" bo'lmaydi)
//  · Avto-tuzatish: notalarni tonallikka (scale) aniq joylash, egri chiziqni tekislash
//  · Garmoniya / xor: tertsiya, kvarta, kvinta, oktava — tonallikka mos (diatonik) intervallar
//  · Vokal tozalash: HPF, nafas pasaytirish, gate, de-esser
// =============================================================================
#pragma once
#include "SampleSync.h"

namespace az::pitch
{
constexpr double anaRate = 16000.0, hopSec = 0.01;   // tahlil: 16 kHz, 10 ms kadr

struct Analysis
{
    double sampleRate = 48000; int numSamples = 0;
    std::vector<float> f0;        // Hz (0 = ovozsiz), har 10 ms
    std::vector<float> rms;       // dB
    double frameTime (int i) const { return i * hopSec; }
    int frames() const { return (int) f0.size(); }
    float midiAt (int i) const { return f0[(size_t) i] > 0 ? (float) (69.0 + 12.0 * std::log2 (f0[(size_t) i] / 440.0)) : -1.0f; }
};

struct Note
{
    double start = 0, end = 0;     // soniya (fayl ichida)
    float orig = 60, pitch = 60;   // midi (kasr bilan): asl markaz va tahrirlangan
    float straighten = 0;          // 0..1 — tebranishni tekislash
    float gainDb = 0;
};

// ---------------------------------------------------------------- tahlil
inline Analysis analyse (const juce::AudioBuffer<float>& in, double sr)
{
    Analysis a; a.sampleRate = sr; a.numSamples = in.getNumSamples();
    // mono + past chastota filtri + 16 kHz ga resample
    const int N = in.getNumSamples();
    std::vector<float> mono ((size_t) N);
    for (int i = 0; i < N; ++i) { float v = 0; for (int c = 0; c < in.getNumChannels(); ++c) v += in.getSample (c, i); mono[(size_t) i] = v / (float) juce::jmax (1, in.getNumChannels()); }
    juce::IIRFilter lp1, lp2; lp1.setCoefficients (juce::IIRCoefficients::makeLowPass (sr, 6500)); lp2.setCoefficients (juce::IIRCoefficients::makeLowPass (sr, 6500));
    lp1.processSamples (mono.data(), N); lp2.processSamples (mono.data(), N);
    const double ratio = sr / anaRate;
    const int M = (int) (N / ratio);
    std::vector<float> x ((size_t) M + 2048, 0.0f);
    { juce::LagrangeInterpolator li; li.process (ratio, mono.data(), x.data(), M); }

    constexpr int W = 1024, maxTau = 256, minTau = 14, fftOrder = 12, F = 1 << fftOrder;
    juce::dsp::FFT fft (fftOrder);
    std::vector<juce::dsp::Complex<float>> A (F), B (F), C (F), tmp (F);
    std::vector<double> prefix ((size_t) M + 2049, 0.0);
    for (int i = 0; i < M + 2048; ++i) prefix[(size_t) i + 1] = prefix[(size_t) i] + (double) x[(size_t) i] * x[(size_t) i];
    const int hop = (int) (anaRate * hopSec);
    double peakRms = -200;
    std::vector<float> cmnd (maxTau + 1);
    for (int s = 0; s + W + maxTau < M + 2048 && s < M; s += hop)
    {
        // rms
        const double e0 = prefix[(size_t) (s + W)] - prefix[(size_t) s];
        const float rdb = (float) (10.0 * std::log10 (juce::jmax (1e-12, e0 / W)));
        a.rms.push_back (rdb); peakRms = juce::jmax (peakRms, (double) rdb);
        if (rdb < -55.0f) { a.f0.push_back (0); continue; }
        // r(tau) FFT orqali
        for (int i = 0; i < F; ++i) { A[(size_t) i] = { i < W ? x[(size_t) (s + i)] : 0.0f, 0.0f }; B[(size_t) i] = { i < W + maxTau ? x[(size_t) (s + i)] : 0.0f, 0.0f }; }
        fft.perform (A.data(), tmp.data(), false); A.swap (tmp);
        fft.perform (B.data(), tmp.data(), false); B.swap (tmp);
        for (int i = 0; i < F; ++i) C[(size_t) i] = std::conj (A[(size_t) i]) * B[(size_t) i];
        fft.perform (C.data(), tmp.data(), true);
        // CMNDF
        double run = 0; cmnd[0] = 1;
        for (int tau = 1; tau <= maxTau; ++tau)
        {
            const double r = tmp[(size_t) tau].real();          // perform(inverse) normallashtiradi (1/F)
            const double et = prefix[(size_t) (s + tau + W)] - prefix[(size_t) (s + tau)];
            const double d = juce::jmax (0.0, e0 + et - 2.0 * r);
            run += d;
            cmnd[(size_t) tau] = (float) (run > 0 ? d * tau / run : 1.0);
        }
        int best = -1;
        for (int tau = minTau; tau < maxTau; ++tau)
            if (cmnd[(size_t) tau] < 0.15f)
            {
                while (tau + 1 < maxTau && cmnd[(size_t) tau + 1] < cmnd[(size_t) tau]) ++tau;
                best = tau; break;
            }
        if (best < 0)
        {   // global minimum, agar yetarlicha davriy bo'lsa
            int bi = minTau; for (int tau = minTau; tau < maxTau; ++tau) if (cmnd[(size_t) tau] < cmnd[(size_t) bi]) bi = tau;
            if (cmnd[(size_t) bi] < 0.3f) best = bi;
        }
        if (best <= minTau || best >= maxTau - 1) { a.f0.push_back (0); continue; }
        const double y0 = cmnd[(size_t) best - 1], y1 = cmnd[(size_t) best], y2 = cmnd[(size_t) best + 1];
        const double den = y0 - 2 * y1 + y2;
        const double t = best + (std::abs (den) > 1e-12 ? juce::jlimit (-0.5, 0.5, 0.5 * (y0 - y2) / den) : 0.0);
        a.f0.push_back ((float) (anaRate / t));
    }
    // nisbiy jimlik: eng balanddan 45 dB past — ovozsiz
    for (size_t i = 0; i < a.f0.size(); ++i) if (a.rms[i] < peakRms - 45) a.f0[i] = 0;
    // yakka "sakrashlar"ni tozalash (oktava xatolari) — 5 kadrlik median
    std::vector<float> f = a.f0;
    for (size_t i = 2; i + 2 < f.size(); ++i)
    {
        if (a.f0[i] <= 0) continue;
        std::array<float, 5> w { a.f0[i - 2], a.f0[i - 1], a.f0[i], a.f0[i + 1], a.f0[i + 2] };
        int voiced = 0; for (auto v : w) voiced += v > 0;
        if (voiced < 3) continue;
        std::sort (w.begin(), w.end());
        const float med = w[(size_t) (5 - voiced + voiced / 2)];
        if (std::abs (12 * std::log2 (a.f0[i] / med)) > 5) f[i] = med;
    }
    a.f0 = f;
    // kadr vaqti = oyna markazi (W/2 = 32 ms): boshiga 3 ta bo'sh kadr qo'shamiz
    a.f0.insert (a.f0.begin(), 3, 0.0f); a.rms.insert (a.rms.begin(), 3, -100.0f);
    return a;
}

// ---------------------------------------------------------------- notalarga bo'lish
inline std::vector<Note> segment (const Analysis& a)
{
    std::vector<Note> notes;
    const int n = a.frames();
    int i = 0;
    while (i < n)
    {
        while (i < n && a.f0[(size_t) i] <= 0) ++i;
        if (i >= n) break;
        int s = i; std::vector<float> cur;
        while (i < n && a.f0[(size_t) i] > 0)
        {
            const float m = a.midiAt (i);
            if (cur.size() >= 6)
            {
                std::vector<float> c2 (cur.end() - 6, cur.end()); std::sort (c2.begin(), c2.end());
                const float med = c2[3];
                // 3 kadr (30 ms) davomida 0.7 yarim tondan uzoq — yangi nota
                bool jump = std::abs (m - med) > 0.7f;
                for (int k = 1; k < 3 && jump; ++k) jump = i + k < n && a.f0[(size_t) (i + k)] > 0 && std::abs (a.midiAt (i + k) - med) > 0.7f;
                if (jump) break;
            }
            cur.push_back (m); ++i;
        }
        if ((int) cur.size() >= 6)        // >= 60 ms
        {
            std::vector<float> mid (cur.begin() + (long) (cur.size() / 5), cur.end() - (long) (cur.size() / 5));
            std::sort (mid.begin(), mid.end());
            Note nt; nt.start = a.frameTime (s); nt.end = a.frameTime (s + (int) cur.size());
            nt.orig = nt.pitch = mid[mid.size() / 2];
            notes.push_back (nt);
        }
        else if (! notes.empty() && s - (int) (notes.back().end / hopSec) <= 2) notes.back().end = a.frameTime (s + (int) cur.size());
    }
    return notes;
}

// ---------------------------------------------------------------- tonallik (scale)
inline bool inScale (int pc, sync::KeyInfo k) { return sync::noteInKey (pc, k); }
inline int nearestScaleNote (float midi, sync::KeyInfo k)
{
    const int r = (int) std::lround (midi);
    for (int d = 0; d <= 6; ++d)
    {
        const int up = r + d, dn = r - d;
        // yaqinroqni tanlash
        const bool upOk = inScale (((up % 12) + 12) % 12, k), dnOk = inScale (((dn % 12) + 12) % 12, k);
        if (upOk && dnOk) return std::abs (up - midi) <= std::abs (dn - midi) ? up : dn;
        if (upOk) return up; if (dnOk) return dn;
    }
    return r;
}
// diatonik interval: degreeSteps = 2 (tertsiya), 3 (kvarta), 4 (kvinta), 7 (oktava); manfiy — pastga
inline int diatonicShift (int midi, int degreeSteps, sync::KeyInfo k)
{
    if (! k.valid() || degreeSteps % 7 == 0) return degreeSteps / 7 * 12;
    static const int maj[] = { 0, 2, 4, 5, 7, 9, 11 }, mnr[] = { 0, 2, 3, 5, 7, 8, 10 };
    const int* sc = k.minor ? mnr : maj;
    const int base = nearestScaleNote ((float) midi, k);
    const int rel = ((base - k.root) % 12 + 12) % 12;
    int deg = 0; for (int i = 0; i < 7; ++i) if (sc[i] == rel) deg = i;
    const int tgtDeg = deg + degreeSteps;
    const int oct = (int) std::floor (tgtDeg / 7.0);
    const int target = base - rel + sc[((tgtDeg % 7) + 7) % 7] + 12 * oct;
    return target - midi;
}

// "AI" avto-tuzatish: noto'g'ri (fals) notalarni topib, tonallikka joylaydi
struct AutoResult { int fixedNotes = 0; float avgCentsBefore = 0, avgCentsAfter = 0; };
inline AutoResult autoCorrect (std::vector<Note>& notes, sync::KeyInfo key, float strength = 1.0f, float straighten = 0.35f)
{
    AutoResult r; double sb = 0, sa = 0;
    for (auto& n : notes)
    {
        const int target = nearestScaleNote (n.orig, key);
        const float err = n.orig - (float) target;
        sb += std::abs (err) * 100.0;
        if (std::abs (err) > 0.08f || std::abs (n.pitch - (float) target) > 0.08f) ++r.fixedNotes;
        n.pitch = n.orig + ((float) target - n.orig) * strength;
        n.straighten = juce::jmax (n.straighten, straighten);
        sa += std::abs (n.pitch - (float) target) * 100.0;
    }
    if (! notes.empty()) { r.avgCentsBefore = (float) (sb / notes.size()); r.avgCentsAfter = (float) (sa / notes.size()); }
    return r;
}

// ---------------------------------------------------------------- har kadr uchun siljish (yarim tonlarda)
inline std::vector<float> shiftCurve (const Analysis& a, const std::vector<Note>& notes, float extraSemis = 0, float detuneCents = 0, juce::Random* rnd = nullptr, const std::function<float (const Note&)>& perNote = {})
{
    std::vector<float> sh ((size_t) a.frames(), 0.0f);
    std::vector<float> w ((size_t) a.frames(), 0.0f);
    for (auto& n : notes)
    {
        const int f0 = (int) (n.start / hopSec), f1 = juce::jmin (a.frames(), (int) std::ceil (n.end / hopSec));
        const float det = rnd ? (rnd->nextFloat() * 2 - 1) * detuneCents / 100.0f : 0.0f;
        const float base = (n.pitch - n.orig) + extraSemis + det + (perNote ? perNote (n) : 0.0f);
        for (int i = juce::jmax (0, f0); i < f1; ++i)
        {
            float s = base;
            const float m = a.midiAt (i);
            if (m > 0 && n.straighten > 0) s += -(m - n.orig) * n.straighten;      // tebranish/drift tekislanadi
            sh[(size_t) i] = s; w[(size_t) i] = 1;
        }
    }
    // notalar orasida silliq o'tish (30 ms)
    std::vector<float> out = sh;
    for (int i = 0; i < (int) sh.size(); ++i)
    {
        double sum = 0, ws = 0;
        for (int k = -3; k <= 3; ++k) { const int q = i + k; if (q < 0 || q >= (int) sh.size() || w[(size_t) q] == 0) continue; sum += sh[(size_t) q]; ws += 1; }
        if (ws > 0 && w[(size_t) i] > 0) out[(size_t) i] = (float) (sum / ws);
    }
    return out;
}

// ---------------------------------------------------------------- TD-PSOLA
inline juce::AudioBuffer<float> psola (const juce::AudioBuffer<float>& in, const Analysis& a, const std::vector<float>& shiftSemis, const std::vector<float>* gainDb = nullptr)
{
    const double sr = a.sampleRate; const int N = in.getNumSamples(), C = in.getNumChannels();
    juce::AudioBuffer<float> out (C, N); out.makeCopyOf (in);
    if (a.frames() == 0) return out;
    auto f0At = [&] (double t) -> double
    {
        const double fi = t / hopSec; const int i = (int) fi; const double fr = fi - i;
        if (i < 0 || i + 1 >= a.frames()) return i >= 0 && i < a.frames() ? a.f0[(size_t) i] : 0.0;
        const float f1 = a.f0[(size_t) i], f2 = a.f0[(size_t) i + 1];
        if (f1 <= 0 || f2 <= 0) return f1 > 0 ? f1 : f2;
        return f1 + (f2 - f1) * fr;
    };
    auto shAt = [&] (double t) -> double
    {
        const double fi = t / hopSec; const int i = juce::jlimit (0, a.frames() - 1, (int) fi); const int j = juce::jmin (a.frames() - 1, i + 1);
        const double fr = juce::jlimit (0.0, 1.0, fi - i);
        return shiftSemis[(size_t) i] + (shiftSemis[(size_t) j] - shiftSemis[(size_t) i]) * fr;
    };
    // epoch qidirish uchun past chastotali mono
    std::vector<float> lp ((size_t) N);
    for (int i = 0; i < N; ++i) { float v = 0; for (int c = 0; c < C; ++c) v += in.getSample (c, i); lp[(size_t) i] = v; }
    { juce::IIRFilter f; f.setCoefficients (juce::IIRCoefficients::makeLowPass (sr, 1200)); f.processSamples (lp.data(), N); }

    // ovozli oraliqlar
    const int F = a.frames();
    int fi = 0;
    juce::AudioBuffer<float> syn (C, N); syn.clear();
    std::vector<float> wsum ((size_t) N, 0.0f), mask ((size_t) N, 0.0f);
    while (fi < F)
    {
        while (fi < F && a.f0[(size_t) fi] <= 0) ++fi;
        if (fi >= F) break;
        const int fs = fi; while (fi < F && a.f0[(size_t) fi] > 0) ++fi;
        const int s0 = (int) (fs * hopSec * sr), s1 = juce::jmin (N, (int) (fi * hopSec * sr));
        float mx = 0; for (int q = fs; q < fi; ++q) mx = juce::jmax (mx, std::abs (shiftSemis[(size_t) q]));
        if (mx < 0.005f && gainDb == nullptr) continue;                 // o'zgarmagan joy — asl audio
        // analiz belgilari (epoch)
        std::vector<int> marks;
        {
            double T = sr / juce::jmax (50.0, f0At (s0 / sr));
            int p = s0; float best = -1;
            for (int q = s0; q < juce::jmin (N, s0 + (int) T); ++q) if (lp[(size_t) q] > best) { best = lp[(size_t) q]; p = q; }
            while (p < s1)
            {
                marks.push_back (p);
                T = sr / juce::jmax (50.0, f0At (p / sr));
                const int c = p + (int) T, r = juce::jmax (2, (int) (T * 0.15));
                int bp = c; float bv = -1e9f;
                for (int q = juce::jmax (p + 1, c - r); q <= juce::jmin (N - 1, c + r); ++q) if (lp[(size_t) q] > bv) { bv = lp[(size_t) q]; bp = q; }
                if (bp <= p) bp = p + (int) T;
                p = bp;
            }
        }
        if (marks.size() < 2) continue;
        // sintez
        double sPos = marks.front(); size_t ai = 0;
        while (sPos < s1)
        {
            while (ai + 1 < marks.size() && std::abs (marks[ai + 1] - sPos) < std::abs (marks[ai] - sPos)) ++ai;
            const int am = marks[ai];
            const int Ta = ai + 1 < marks.size() ? marks[ai + 1] - am : (ai > 0 ? am - marks[ai - 1] : (int) (sr / 200));
            const double sh = shAt (sPos / sr);
            const float g = gainDb ? juce::Decibels::decibelsToGain ((*gainDb)[(size_t) juce::jlimit (0, F - 1, (int) (sPos / sr / hopSec))]) : 1.0f;
            const int sp = (int) sPos;
            for (int k = -Ta; k < Ta; ++k)
            {
                const int src = am + k, dst = sp + k;
                if (src < 0 || src >= N || dst < 0 || dst >= N) continue;
                const float w = 0.5f + 0.5f * std::cos (juce::MathConstants<float>::pi * (float) k / (float) Ta);
                for (int c = 0; c < C; ++c) syn.addSample (c, dst, in.getSample (c, src) * w * g);
                wsum[(size_t) dst] += w;
            }
            const double f0s = f0At (sPos / sr);
            const double Tt = (f0s > 0 ? sr / f0s : Ta) / std::pow (2.0, sh / 12.0);
            sPos += juce::jmax (8.0, Tt);
        }
        // maska (10 ms o'tish)
        const int ramp = (int) (0.01 * sr);
        for (int q = s0; q < s1; ++q)
        {
            const float m = juce::jmin (1.0f, juce::jmin ((float) (q - s0) / ramp, (float) (s1 - q) / ramp));
            mask[(size_t) q] = juce::jmax (mask[(size_t) q], juce::jlimit (0.0f, 1.0f, m));
        }
    }
    for (int q = 0; q < N; ++q)
    {
        const float m = mask[(size_t) q];
        if (m <= 0) continue;
        const float ws = juce::jmax (0.35f, wsum[(size_t) q]);
        for (int c = 0; c < C; ++c) out.setSample (c, q, in.getSample (c, q) * (1 - m) + syn.getSample (c, q) / ws * m);
    }
    return out;
}

// ---------------------------------------------------------------- vokal tozalash
struct CleanSettings { bool hpf = true; float breathDb = -12.0f, gateDb = -30.0f, deessDb = -6.0f; };
inline void clean (juce::AudioBuffer<float>& b, double sr, const Analysis& a, const CleanSettings& cs)
{
    const int N = b.getNumSamples(), C = b.getNumChannels();
    if (cs.hpf)
        for (int c = 0; c < C; ++c)
        {
            juce::IIRFilter h1, h2; h1.setCoefficients (juce::IIRCoefficients::makeHighPass (sr, 80)); h2.setCoefficients (juce::IIRCoefficients::makeHighPass (sr, 80));
            h1.processSamples (b.getWritePointer (c), N); h2.processSamples (b.getWritePointer (c), N);
        }
    // ovozli joylar o'rtacha darajasi
    std::vector<float> vr; for (int i = 0; i < a.frames(); ++i) if (a.f0[(size_t) i] > 0) vr.push_back (a.rms[(size_t) i]);
    std::sort (vr.begin(), vr.end());
    const float voicedMed = vr.empty() ? -20.0f : vr[vr.size() / 2];
    // kadr bo'yicha gain: nafas (ovozsiz, lekin eshitiladigan) va jimlik (gate)
    std::vector<float> g ((size_t) a.frames(), 0.0f);
    for (int i = 0; i < a.frames(); ++i)
    {
        if (a.f0[(size_t) i] > 0) continue;
        const float r = a.rms[(size_t) i];
        if (r < voicedMed - 40) g[(size_t) i] = cs.gateDb;
        else if (r < voicedMed - 10) g[(size_t) i] = cs.breathDb;
    }
    // ovozli notalar atrofini himoya (40 ms) — undosh harflar kesilmasin
    std::vector<float> g2 = g;
    for (int i = 0; i < a.frames(); ++i) if (a.f0[(size_t) i] > 0) for (int k = -4; k <= 4; ++k) { const int q = i + k; if (q >= 0 && q < a.frames()) g2[(size_t) q] = 0; }
    // silliqlash
    std::vector<float> gs = g2;
    for (int i = 0; i < a.frames(); ++i) { double s = 0; int n = 0; for (int k = -2; k <= 2; ++k) { const int q = i + k; if (q >= 0 && q < a.frames()) { s += g2[(size_t) q]; ++n; } } gs[(size_t) i] = (float) (s / n); }
    for (int q = 0; q < N; ++q)
    {
        const double fi = q / sr / hopSec; const int i = juce::jlimit (0, juce::jmax (0, a.frames() - 1), (int) fi);
        const int j = juce::jmin (juce::jmax (0, a.frames() - 1), i + 1); const float fr = (float) (fi - i);
        if (a.frames() == 0) break;
        const float db = gs[(size_t) i] + (gs[(size_t) j] - gs[(size_t) i]) * juce::jlimit (0.0f, 1.0f, fr);
        if (db < -0.01f) { const float k = juce::Decibels::decibelsToGain (db); for (int c = 0; c < C; ++c) b.getWritePointer (c)[q] *= k; }
    }
    // de-esser: 5.5 kHz dan yuqori polosa, sibilyant paytida pasaytiriladi
    if (cs.deessDb < -0.1f)
        for (int c = 0; c < C; ++c)
        {
            auto* d = b.getWritePointer (c);
            juce::IIRFilter hp; hp.setCoefficients (juce::IIRCoefficients::makeHighPass (sr, 5500));
            std::vector<float> hi (d, d + N); hp.processSamples (hi.data(), N);
            float envH = 0, envA = 0; const float at = std::exp (-1.0f / (0.002f * (float) sr)), rl = std::exp (-1.0f / (0.06f * (float) sr));
            for (int q = 0; q < N; ++q)
            {
                const float h = std::abs (hi[(size_t) q]), x = std::abs (d[q]);
                envH = h > envH ? at * envH + (1 - at) * h : rl * envH + (1 - rl) * h;
                envA = x > envA ? at * envA + (1 - at) * x : rl * envA + (1 - rl) * x;
                float red = 0;
                if (envA > 0.01f && envH > envA * 0.45f) red = juce::jlimit (0.0f, 1.0f, (envH / juce::jmax (1e-6f, envA) - 0.45f) / 0.35f);
                const float k = juce::Decibels::decibelsToGain (cs.deessDb * red);
                d[q] = d[q] - hi[(size_t) q] * (1 - k);
            }
        }
}

// ---------------------------------------------------------------- saqlash (ValueTree)
inline const juce::Identifier NOTE ("NOTE");
inline juce::String encode (const std::vector<float>& v)
{
    juce::MemoryBlock mb (v.data(), v.size() * sizeof (float)); return mb.toBase64Encoding();
}
inline std::vector<float> decode (const juce::String& s)
{
    juce::MemoryBlock mb; mb.fromBase64Encoding (s);
    std::vector<float> v (mb.getSize() / sizeof (float)); if (! v.empty()) std::memcpy (v.data(), mb.getData(), v.size() * sizeof (float));
    return v;
}
inline void storeAnalysis (juce::ValueTree p, const Analysis& a)
{
    p.setProperty ("sr", a.sampleRate, nullptr); p.setProperty ("ns", a.numSamples, nullptr);
    p.setProperty ("f0", encode (a.f0), nullptr); p.setProperty ("rms", encode (a.rms), nullptr);
}
inline Analysis loadAnalysis (const juce::ValueTree& p)
{
    Analysis a; a.sampleRate = p["sr"]; a.numSamples = p["ns"]; a.f0 = decode (p["f0"]); a.rms = decode (p["rms"]);
    a.rms.resize (a.f0.size(), -100.0f);
    return a;
}
inline void storeNotes (juce::ValueTree p, const std::vector<Note>& notes, juce::UndoManager* um)
{
    for (int i = p.getNumChildren(); --i >= 0;) if (p.getChild (i).hasType (NOTE)) p.removeChild (i, um);
    for (auto& n : notes)
    {
        juce::ValueTree v (NOTE);
        v.setProperty ("s", n.start, nullptr); v.setProperty ("e", n.end, nullptr); v.setProperty ("o", n.orig, nullptr);
        v.setProperty ("p", n.pitch, nullptr); v.setProperty ("st", n.straighten, nullptr); v.setProperty ("g", n.gainDb, nullptr);
        p.addChild (v, -1, um);
    }
}
inline std::vector<Note> loadNotes (const juce::ValueTree& p)
{
    std::vector<Note> r;
    for (auto v : p) if (v.hasType (NOTE)) r.push_back ({ (double) v["s"], (double) v["e"], (float) (double) v["o"], (float) (double) v["p"], (float) (double) v["st"], (float) (double) v["g"] });
    return r;
}
// nota gain'lari kadr bo'yicha
inline std::vector<float> gainCurve (const Analysis& a, const std::vector<Note>& notes)
{
    std::vector<float> g ((size_t) a.frames(), 0.0f); bool any = false;
    for (auto& n : notes) if (std::abs (n.gainDb) > 0.01f)
    { any = true; for (int i = juce::jmax (0, (int) (n.start / hopSec)); i < juce::jmin (a.frames(), (int) std::ceil (n.end / hopSec)); ++i) g[(size_t) i] = n.gainDb; }
    if (! any) g.clear();
    return g;
}
inline juce::AudioBuffer<float> renderNotes (const juce::AudioBuffer<float>& in, const Analysis& a, const std::vector<Note>& notes)
{
    auto sh = shiftCurve (a, notes);
    auto g = gainCurve (a, notes);
    return psola (in, a, sh, g.empty() ? nullptr : &g);
}

// ---------------------------------------------------------------- garmoniya / xor
struct Voice { juce::String name; int degree; float detuneCents; double delayMs; float pan, gainDb; };
inline std::vector<Voice> preset (int id)
{
    switch (id)
    {
        case 0: return { { juce::String::fromUTF8 ("Tertsiya \xe2\x86\x91"), 2, 6, 18, 0.35f, -4 } };
        case 1: return { { juce::String::fromUTF8 ("Tertsiya \xe2\x86\x93"), -2, 6, 22, -0.35f, -4 } };
        case 2: return { { juce::String::fromUTF8 ("Kvarta \xe2\x86\x91"), 3, 6, 20, 0.3f, -5 } };
        case 3: return { { juce::String::fromUTF8 ("Kvinta \xe2\x86\x91"), 4, 6, 20, -0.3f, -5 } };
        case 4: return { { juce::String::fromUTF8 ("Kvarta \xe2\x86\x93"), -3, 6, 24, 0.25f, -5 } };
        case 5: return { { juce::String::fromUTF8 ("Oktava \xe2\x86\x93"), -7, 4, 15, 0.0f, -7 } };
        case 6: return { { juce::String::fromUTF8 ("Xor: tertsiya \xe2\x86\x91"), 2, 8, 20, 0.45f, -6 },
                         { juce::String::fromUTF8 ("Xor: kvinta \xe2\x86\x91"), 4, 8, 28, -0.45f, -7 },
                         { juce::String::fromUTF8 ("Xor: oktava \xe2\x86\x93"), -7, 6, 14, 0.0f, -9 } };
        case 7: return { { juce::String::fromUTF8 ("Katta xor: unison L"), 0, 12, 24, -0.8f, -8 },
                         { juce::String::fromUTF8 ("Katta xor: unison R"), 0, 12, 31, 0.8f, -8 },
                         { juce::String::fromUTF8 ("Katta xor: tertsiya \xe2\x86\x91"), 2, 10, 22, 0.5f, -7 },
                         { juce::String::fromUTF8 ("Katta xor: kvinta \xe2\x86\x91"), 4, 10, 27, -0.5f, -8 },
                         { juce::String::fromUTF8 ("Katta xor: tertsiya \xe2\x86\x93"), -2, 10, 19, -0.3f, -8 },
                         { juce::String::fromUTF8 ("Katta xor: oktava \xe2\x86\x93"), -7, 8, 16, 0.3f, -10 } };
        default: return {};
    }
}
inline const char* presetNames[] = { "3 \xe2\x86\x91", "3 \xe2\x86\x93", "4 \xe2\x86\x91", "5 \xe2\x86\x91", "4 \xe2\x86\x93", "8 \xe2\x86\x93", "XOR 3", "XOR 6" };

inline juce::AudioBuffer<float> renderVoice (const juce::AudioBuffer<float>& in, const Analysis& a, const std::vector<Note>& notes, const Voice& v, sync::KeyInfo key, int seed)
{
    juce::Random rnd (seed);
    auto sh = shiftCurve (a, notes, 0.0f, v.detuneCents, &rnd, [&] (const Note& n) { return (float) diatonicShift ((int) std::lround (n.pitch), v.degree, key); });
    auto out = psola (in, a, sh);
    // vaqt siljishi (inson kabi kechikish) + gain
    const int d = (int) (v.delayMs * 0.001 * a.sampleRate);
    juce::AudioBuffer<float> res (out.getNumChannels(), out.getNumSamples()); res.clear();
    for (int c = 0; c < out.getNumChannels(); ++c) res.copyFrom (c, juce::jmin (d, out.getNumSamples()), out, c, 0, juce::jmax (0, out.getNumSamples() - d));
    res.applyGain (juce::Decibels::decibelsToGain (v.gainDb));
    return res;
}
}
