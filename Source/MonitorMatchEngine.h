// AZ MUS — Monitor Match dvigateli (alifrecords/AZmonitors, monitor-match branch)
#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <complex>
#include <sstream>
#include <cstdlib>
#include <string>
// Monitor Match — monitor chastota javobi profillari.
// Qiymatlar dB'da, o'rta chastota (~200 Hz – 2 kHz) atrofida 0 dB ga normallashtirilgan.
// DIQQAT: bu ishlab chiqaruvchilar e'lon qilgan grafiklar va mustaqil o'lchovlar asosida
// tuzilgan TAXMINIY egri chiziqlar. Ularni o'zgartirish uchun shu faylni tahrirlang.

#include <vector>
#include <cmath>
#include <algorithm>

namespace mm
{
constexpr double kPi = 3.14159265358979323846;

struct CurvePoint
{
    double hz;
    double db;
};

struct MonitorProfile
{
    const char* brand;
    const char* model;
    const char8_t* note;
    std::vector<CurvePoint> points;
};

// Log-chastota o'qida silliq (kosinus) interpolyatsiya.
inline double interpolateDb (const std::vector<CurvePoint>& pts, double hz)
{
    if (pts.empty())               return 0.0;
    if (hz <= pts.front().hz)      return pts.front().db;
    if (hz >= pts.back().hz)       return pts.back().db;

    auto it = std::upper_bound (pts.begin(), pts.end(), hz,
                                [] (double f, const CurvePoint& p) { return f < p.hz; });
    const auto& b = *it;
    const auto& a = *(it - 1);
    const double t = std::log (hz / a.hz) / std::log (b.hz / a.hz);
    const double s = 0.5 - 0.5 * std::cos (t * kPi);
    return a.db + (b.db - a.db) * s;
}

// Sizning monitorlaringiz (manba).
inline const std::vector<MonitorProfile>& sourceProfiles()
{
    static const std::vector<MonitorProfile> p {
        { "KRK", "Rokit 6 G4", u8"6\" woofer, bass ko'tarilgan",
          { {20,-30},{30,-18},{38,-9},{45,-3},{55,1.5},{70,2.5},{90,2.0},{120,0.8},{200,0},
            {350,-1.0},{600,-0.5},{1000,0},{2000,0.8},{3500,1.5},{5000,0.8},{8000,0.5},
            {12000,1.0},{16000,0},{20000,-2},{24000,-6} } },
        { "ADAM AUDIO", "A8X", u8"8.5\" woofer, X-ART tviter",
          { {20,-24},{28,-12},{33,-6},{38,-3},{50,1.0},{65,1.2},{100,0.5},{200,0},{500,0},
            {1000,-0.8},{2000,-0.3},{4000,0.5},{7000,1.5},{10000,2.0},{14000,2.0},{18000,1.0},
            {22000,-1},{24000,-2} } },
        { "KRK", "Rokit 5 G4", u8"5\" woofer, 60-140 Hz ko'tarilgan",
          { {20,-34},{35,-20},{45,-10},{52,-4},{60,0},{75,2.5},{100,2.5},{140,1},{220,0},{400,-0.8},
            {800,-0.3},{1500,0.3},{3000,1.2},{5000,0.5},{9000,0.5},{13000,1},{17000,-0.5},{20000,-3},{24000,-8} } },
        { "KRK", "Rokit 8 G4", u8"8\" woofer, bass ko'tarilgan",
          { {20,-22},{28,-10},{34,-3},{42,1.5},{60,2.8},{90,2.2},{140,0.8},{220,0},{400,-1},{800,-0.4},
            {1500,0.3},{3000,1.2},{5000,0.6},{9000,0.4},{13000,0.8},{17000,-0.5},{20000,-2.5},{24000,-7} } },
        { "ADAM AUDIO", "T5V", u8"5\" woofer, U-ART tviter",
          { {20,-34},{35,-18},{45,-8},{55,-2},{70,0.5},{100,1},{200,0},{600,-0.3},{1200,-0.6},{2500,0.2},
            {5000,1},{8000,1.8},{12000,1.8},{16000,1},{20000,-1},{24000,-4} } },
        { "ADAM AUDIO", "T7V", u8"7\" woofer, U-ART tviter",
          { {20,-28},{30,-14},{40,-5},{50,-1},{70,1},{100,0.8},{200,0},{600,-0.3},{1200,-0.6},{2500,0.2},
            {5000,1},{8000,1.8},{12000,1.8},{16000,1},{20000,-1},{24000,-4} } },
        { "YAMAHA", "HS5", u8"5\" woofer, bass kam, o'rta oldinda",
          { {20,-36},{40,-18},{55,-8},{70,-2.5},{90,0},{150,0.4},{300,0},{700,-0.3},{1500,0.6},{2500,2},
            {3500,2},{5000,0.5},{8000,0},{12000,0.5},{16000,-0.5},{20000,-3},{24000,-7} } },
        { "JBL", "305P MkII", u8"5\" woofer, waveguide",
          { {20,-32},{35,-16},{45,-6},{55,-1.5},{70,0.5},{120,0.3},{300,0},{800,0},{1500,-0.4},{2500,-0.2},
            {4000,0.3},{8000,0.5},{12000,0},{16000,-0.8},{20000,-3},{24000,-7} } },
        { "PRESONUS", "Eris E5", u8"5.25\" woofer, bass va yuqori ko'tarilgan",
          { {20,-34},{40,-16},{50,-7},{60,-2},{80,1.5},{110,2},{160,0.8},{250,0},{500,-0.8},{1000,0},
            {2000,1},{3500,1.8},{6000,0.5},{10000,1.5},{14000,1},{18000,-1},{22000,-5},{24000,-8} } },
        { "M-AUDIO", "BX5 D3", u8"5\" woofer, 'smile' egri chiziq",
          { {20,-34},{40,-16},{52,-6},{65,0},{85,2.5},{120,2},{200,0.3},{400,-1},{900,-0.5},{1800,0.8},
            {3000,2},{5000,0.5},{9000,1},{13000,1.5},{17000,-1},{20000,-4},{24000,-9} } },
        { "MACKIE", "CR4-X", u8"Multimedia, 100-200 Hz bo'rtgan",
          { {20,-38},{50,-20},{65,-10},{80,-3},{100,1},{140,2.5},{200,1.5},{350,0},{700,-1},{1500,0.5},
            {3000,2},{5000,1},{8000,0},{12000,1.5},{16000,-1},{20000,-5},{24000,-10} } },
        { "BEHRINGER", "Truth B2030A", u8"5.25\" woofer, ribbon tviter",
          { {20,-28},{35,-12},{45,-4},{55,-0.5},{80,1.5},{150,0.5},{300,0},{800,-0.5},{1500,0.3},{3000,1},
            {6000,-0.5},{10000,0.5},{14000,1},{18000,-1},{22000,-4},{24000,-6} } },
    };
    return p;
}

// Eshitmoqchi bo'lgan monitorlar (maqsad).
inline const std::vector<MonitorProfile>& targetProfiles()
{
    static const std::vector<MonitorProfile> p {
        { "FOCAL", "Shape 65", u8"Tiniq o'rta chastota",
          { {20,-24},{30,-14},{40,-6},{50,-2},{70,0},{150,0.3},{400,0},{1000,0.5},{1800,1.0},
            {3000,0.3},{5000,-0.5},{8000,0},{12000,0.5},{18000,-0.5},{22000,-3},{24000,-5} } },
        { "GENELEC", "8340A", u8"Eng tekis javob",
          { {20,-18},{30,-9},{38,-3},{45,-1},{60,0},{20000,0},{22000,-1},{24000,-2} } },
        { "YAMAHA", "HS8", u8"Halol, oldinga chiqqan o'rta",
          { {20,-26},{30,-14},{38,-6},{45,-2.5},{60,0},{100,0.5},{250,0},{700,-0.3},{1500,0.5},
            {2500,2.0},{3500,2.0},{5000,0.5},{8000,0},{12000,0.5},{16000,0},{20000,-2},{24000,-6} } },
        { "YAMAHA", "NS-10M", u8"Klassik, bass kam, o'rta qattiq",
          { {20,-36},{40,-20},{60,-11},{80,-6},{100,-4},{150,-1.5},{250,0},{500,0.5},{1000,1.5},
            {1500,2.5},{2000,2.0},{3000,0.5},{5000,0},{8000,-0.5},{12000,-1.0},{16000,-3},
            {20000,-7},{24000,-10} } },
        { "REFERENCE", "Flat", u8"Ideal tekis javob",
          { {20,0},{24000,0} } },
        { "NEUMANN", "KH 310", u8"Studiya etaloni, tekis",
          { {20,-20},{30,-8},{35,-3},{45,0},{20000,0},{22000,-1.5},{24000,-3} } },
        { "AURATONE", "5C", u8"Mono 'kubik': faqat o'rta chastota",
          { {20,-40},{60,-24},{100,-14},{150,-7},{200,-3},{300,0},{1000,1.5},{2000,2},{3000,1},{5000,-2},
            {8000,-6},{12000,-12},{16000,-18},{24000,-30} } },
        { "TEKSHIRUV", "Smartfon", u8"Telefon dinamigi: bass yo'q",
          { {20,-48},{100,-36},{200,-24},{300,-14},{500,-5},{800,0},{1500,2},{2500,4},{4000,3},{6000,0},
            {10000,-4},{14000,-10},{18000,-20},{24000,-30} } },
        { "TEKSHIRUV", "Avtomobil", u8"Mashina salonidagi bass va yumshoq yuqori",
          { {20,4},{40,6},{60,6},{90,4},{150,2},{250,0},{600,-1},{1200,0},{2500,1},{5000,0},{8000,-2},
            {12000,-4},{16000,-7},{20000,-12},{24000,-16} } },
    };
    return p;
}
} // namespace mm

#include <string>

namespace mm
{
// Korreksiya sozlamalari (parametrlardan yig'iladi).
struct Settings
{
    int    source     = 0;      // sourceProfiles() indeksi; oxirgisidan keyingisi = o'z o'lchovingiz
    int    target     = 1;      // targetProfiles() indeksi
    double amount     = 1.0;    // 0..1 — emulyatsiya kuchi
    double bass       = 0.85;   // 0..1 — 250 Hz dan pastdagi korreksiya kuchi
    double maxBoostDb = 6.0;    // ko'tarishning yuqori chegarasi
    bool   minPhase   = false;  // false = linear phase (mastering), true = zero latency
    bool   autoGain   = true;   // balandlikni avtomatik tenglash

    bool operator== (const Settings& o) const
    {
        auto same = [] (double a, double b) { return std::abs (a - b) < 1.0e-9; };
        return source == o.source && target == o.target && same (amount, o.amount) && same (bass, o.bass)
            && same (maxBoostDb, o.maxBoostDb) && minPhase == o.minPhase && autoGain == o.autoGain;
    }
    bool operator!= (const Settings& o) const { return ! (*this == o); }
};

class CorrectionModel
{
public:
    std::vector<CurvePoint> custom;   // foydalanuvchi o'lchovi (REW va h.k.)

    double sourceDb (const Settings&, double hz) const;
    double targetDb (const Settings&, double hz) const;
    double rawCorrectionDb (const Settings&, double hz) const;
    double smoothedCorrectionDb (const Settings&, double hz) const;
    double gainCompDb (const Settings&) const;
    double correctionDb (const Settings&, double hz) const;

    // Mono FIR impuls javobi. Linear phase'da kechikish = length / 2.
    std::vector<float> buildImpulse (const Settings&, double sampleRate, int length) const;
};

int firLengthFor (double sampleRate);

// REW / ARTA / boshqa dasturlardan eksport qilingan "chastota  dB" matnini o'qiydi,
// 1/6 oktava silliqlaydi va 300 Hz–3 kHz bo'yicha 0 dB ga normallashtiradi.
std::vector<CurvePoint> parseMeasurementText (const std::string& text);
} // namespace mm

#include <complex>
#include <sstream>
#include <cstdlib>

namespace mm
{
namespace mmdetail
{
using cd = std::complex<double>;

inline void fft (std::vector<cd>& a, bool inverse)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = 2.0 * kPi / (double) len * (inverse ? 1.0 : -1.0);
        for (size_t i = 0; i < n; i += len)
            for (size_t j = 0; j < len / 2; ++j)
            {
                const cd w (std::cos (ang * (double) j), std::sin (ang * (double) j));
                const cd u = a[i + j], v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
            }
    }
    if (inverse)
        for (auto& x : a) x /= (double) n;
}

inline double dbToGain (double db) { return std::pow (10.0, db / 20.0); }
} // namespace mmdetail
using namespace mmdetail;

inline int firLengthFor (double sr)
{
    // 44.1/48 kHz da ~170 ms: 30 Hz gacha aniqlik (xato < 0.6 dB).
    if (sr <= 50000.0)  return 16384;
    if (sr <= 100000.0) return 32768;
    return 65536;
}

inline double CorrectionModel::sourceDb (const Settings& s, double hz) const
{
    const auto& src = sourceProfiles();
    if (s.source >= 0 && s.source < (int) src.size())
        return interpolateDb (src[(size_t) s.source].points, hz);
    return custom.empty() ? 0.0 : interpolateDb (custom, hz);
}

inline double CorrectionModel::targetDb (const Settings& s, double hz) const
{
    const auto& t = targetProfiles();
    const int i = std::clamp (s.target, 0, (int) t.size() - 1);
    return interpolateDb (t[(size_t) i].points, hz);
}

inline double CorrectionModel::rawCorrectionDb (const Settings& s, double hz) const
{
    const double src = sourceDb (s, hz);
    const double tgt = targetDb (s, hz);
    double c = tgt - src;

    // Bass korreksiyasini alohida boshqarish (250 Hz atrofida silliq o'tish).
    const double wb = 1.0 / (1.0 + std::pow (hz / 250.0, 4.0));
    c *= s.amount * ((1.0 - wb) + wb * s.bass);

    // Himoya: dinamik o'zi chiqara olmaydigan joyda ko'tarmaymiz
    // (manba -6 dB dan pastda boost kamayadi, -12 dB da butunlay yo'q).
    const double protect = std::clamp ((src + 12.0) / 6.0, 0.0, 1.0);
    c = std::min (c, s.maxBoostDb * protect);
    c = std::max (c, -18.0);

    if (hz < 20.0)    c *= std::pow (hz / 20.0, 2.0);
    if (hz > 20000.0) c *= std::max (0.0, 1.0 - (hz - 20000.0) / 4000.0);
    return c;
}

inline double CorrectionModel::gainCompDb (const Settings& s) const
{
    // 100 Hz – 8 kHz oralig'idagi o'rtacha korreksiyani qaytarib olamiz,
    // shunda emulyatsiya yoqilganda ovoz balandroq/pastroq bo'lib "aldamaydi".
    constexpr int n = 64;
    double sum = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double hz = 100.0 * std::pow (80.0, (double) i / (n - 1));
        sum += rawCorrectionDb (s, hz);
    }
    return -sum / n;
}

inline double CorrectionModel::smoothedCorrectionDb (const Settings& s, double hz) const
{
    // 1/6 oktava silliqlash: keskin burilishlar FIR'da aniq takrorlanadi va quloqqa tabiiy.
    double sum = 0.0;
    for (int k = -4; k <= 4; ++k)
        sum += rawCorrectionDb (s, hz * std::pow (2.0, k / 48.0));
    return sum / 9.0;
}

inline double CorrectionModel::correctionDb (const Settings& s, double hz) const
{
    return smoothedCorrectionDb (s, hz) + (s.autoGain ? gainCompDb (s) : 0.0);
}

inline std::vector<float> CorrectionModel::buildImpulse (const Settings& s, double sr, int N) const
{
    std::vector<float> out ((size_t) N, 0.0f);
    const double comp = s.autoGain ? gainCompDb (s) : 0.0;
    auto magAt = [&] (double f) { return dbToGain (smoothedCorrectionDb (s, std::max (f, 1.0)) + comp); };

    if (! s.minPhase)
    {
        // Linear phase: nol fazali spektr -> IFFT -> markazga siljitish -> Hann oyna.
        std::vector<cd> X ((size_t) N);
        for (int k = 0; k <= N / 2; ++k)
        {
            const double m = magAt ((double) k * sr / N);
            X[(size_t) k] = m;
            if (k > 0 && k < N / 2) X[(size_t) (N - k)] = m;
        }
        fft (X, true);
        for (int n = 0; n < N; ++n)
        {
            const double x = X[(size_t) ((n + N / 2) % N)].real();
            const double w = 0.5 - 0.5 * std::cos (2.0 * kPi * n / (N - 1));
            out[(size_t) n] = (float) (x * w);
        }
    }
    else
    {
        // Minimum phase (kepstr usuli) — kechikishsiz.
        const int M = N * (N >= 32768 ? 2 : 4);
        std::vector<cd> X ((size_t) M);
        for (int k = 0; k <= M / 2; ++k)
        {
            const double lm = std::log (std::max (magAt ((double) k * sr / M), 1.0e-6));
            X[(size_t) k] = lm;
            if (k > 0 && k < M / 2) X[(size_t) (M - k)] = lm;
        }
        fft (X, true);                         // real kepstr
        for (int n = 1; n < M / 2; ++n) X[(size_t) n] *= 2.0;
        for (int n = M / 2 + 1; n < M; ++n) X[(size_t) n] = 0.0;
        fft (X, false);
        for (auto& v : X) v = std::exp (v);
        fft (X, true);
        for (int n = 0; n < N; ++n)
        {
            double w = 1.0;
            if (n >= N / 2) w = 0.5 + 0.5 * std::cos (kPi * (n - N / 2) / (double) (N / 2));
            out[(size_t) n] = (float) (X[(size_t) n].real() * w);
        }
    }
    return out;
}

inline std::vector<CurvePoint> parseMeasurementText (const std::string& text)
{
    std::vector<CurvePoint> raw;
    std::istringstream in (text);
    std::string line;
    while (std::getline (in, line))
    {
        if (line.empty() || line[0] == '*' || line[0] == '#' || line[0] == ';') continue;
        for (auto& ch : line) if (ch == ';' || ch == '\t') ch = ' ';

        // "100, 85.2, -10" yoki "100 85.2" kabi formatlar (o'nlik ajratuvchi — nuqta)
        std::string l2 = line;
        for (auto& ch : l2) if (ch == ',') ch = ' ';

        const char* p = l2.c_str();
        char* end = nullptr;
        const double f = std::strtod (p, &end);
        if (end == p) continue;
        p = end;
        const double d = std::strtod (p, &end);
        if (end == p) continue;
        if (f > 5.0 && f < 48000.0 && std::isfinite (d))
            raw.push_back ({ f, d });
    }
    if (raw.size() < 10) return {};
    std::sort (raw.begin(), raw.end(), [] (auto& a, auto& b) { return a.hz < b.hz; });

    // 1/6 oktava silliqlash, 1/24 oktava qadamli setkada.
    std::vector<CurvePoint> out;
    const double fLo = std::max (raw.front().hz, 15.0);
    const double fHi = std::min (raw.back().hz, 24000.0);
    const double halfBand = std::pow (2.0, 1.0 / 12.0);
    for (double f = fLo; f <= fHi; f *= std::pow (2.0, 1.0 / 24.0))
    {
        double sum = 0.0; int cnt = 0;
        for (auto& r : raw)
            if (r.hz >= f / halfBand && r.hz <= f * halfBand) { sum += r.db; ++cnt; }
        out.push_back ({ f, cnt > 0 ? sum / cnt : interpolateDb (raw, f) });
    }
    if (out.size() < 4) return {};

    double ref = 0.0; int n = 0;
    for (auto& p : out) if (p.hz >= 300.0 && p.hz <= 3000.0) { ref += p.db; ++n; }
    ref = n > 0 ? ref / n : 0.0;
    for (auto& p : out) p.db -= ref;
    return out;
}
} // namespace mm
