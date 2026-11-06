// =============================================================================
//  AZ MUS — Direct Offline Processing (Cubase DOP kabi) va Render Selection
//  · Jarayonlar steki klipda saqlanadi (asl fayl o'zgarmaydi, istalgan payt qaytarish mumkin)
//  · Envelope: qo'lda nuqtalar + AUTO (vokal rider: balandlikni standart bo'yicha tenglash)
// =============================================================================
#pragma once
#include "Views.h"

namespace az
{
namespace dop
{
// ---------------------------------------------------------------- jarayon turlari
struct ProcDef { const char* type; const char* name; };
inline const ProcDef procDefs[] = {
    { "envelope",  "Envelope" },
    { "gain",      "Gain" },
    { "normalize", "Normalize" },
    { "fadein",    "Fade In" },
    { "fadeout",   "Fade Out" },
    { "invert",    "Invert Phase" },
    { "reverse",   "Reverse" },
    { "dc",        "Remove DC Offset" },
    { "silence",   "Silence" },
};
inline juce::String procName (const juce::String& type)
{
    for (auto& d : procDefs) if (type == d.type) return d.name;
    return type;
}

inline const juce::Identifier DOP ("AZDOP"), PROC ("PROC"), PT ("PT");

// Klipning DOP steki (yo'q bo'lsa yaratiladi)
inline juce::ValueTree stack (te::WaveAudioClip& c, bool create)
{
    auto v = c.state.getChildWithName (DOP);
    if (! v.isValid() && create)
    {
        v = juce::ValueTree (DOP);
        v.setProperty ("source", c.getSourceFileReference().getFile().getFullPathName(), nullptr);
        c.state.addChild (v, -1, nullptr);
    }
    return v;
}
inline juce::File originalFile (te::WaveAudioClip& c)
{
    auto v = stack (c, false);
    if (v.isValid() && juce::File (v["source"].toString()).existsAsFile()) return juce::File (v["source"].toString());
    return c.getSourceFileReference().getFile();
}

inline juce::ValueTree makeProc (const juce::String& type)
{
    juce::ValueTree p (PROC);
    p.setProperty ("type", type, nullptr); p.setProperty ("on", true, nullptr);
    if (type == "envelope")
    {
        p.setProperty ("curve", 0, nullptr); p.setProperty ("auto", false, nullptr);
        p.setProperty ("target", 0, nullptr); p.setProperty ("range", 9.0, nullptr); p.setProperty ("speed", 1, nullptr);
        for (double x : { 0.0, 1.0 }) { juce::ValueTree pt (PT); pt.setProperty ("x", x, nullptr); pt.setProperty ("db", 0.0, nullptr); p.addChild (pt, -1, nullptr); }
    }
    if (type == "gain")      p.setProperty ("db", 0.0, nullptr);
    if (type == "normalize") p.setProperty ("db", -1.0, nullptr);
    if (type == "fadein" || type == "fadeout") { p.setProperty ("len", 0.5, nullptr); p.setProperty ("curve", 1, nullptr); }
    return p;
}

// ---------------------------------------------------------------- envelope
struct EnvPoint { double x, db; };
inline std::vector<EnvPoint> points (const juce::ValueTree& p)
{
    std::vector<EnvPoint> r;
    for (auto c : p) if (c.hasType (PT)) r.push_back ({ (double) c["x"], (double) c["db"] });
    std::sort (r.begin(), r.end(), [] (auto& a, auto& b) { return a.x < b.x; });
    if (r.empty()) r = { { 0, 0 }, { 1, 0 } };
    return r;
}
inline void setPoints (juce::ValueTree p, const std::vector<EnvPoint>& pts, juce::UndoManager* um)
{
    for (int i = p.getNumChildren(); --i >= 0;) if (p.getChild (i).hasType (PT)) p.removeChild (i, um);
    for (auto& e : pts) { juce::ValueTree pt (PT); pt.setProperty ("x", e.x, nullptr); pt.setProperty ("db", e.db, nullptr); p.addChild (pt, -1, um); }
}
// curve: 0 = spline (Catmull-Rom), 1 = damped (silliq S), 2 = linear
inline double envDb (const std::vector<EnvPoint>& pts, double x, int curve)
{
    if (x <= pts.front().x) return pts.front().db;
    if (x >= pts.back().x) return pts.back().db;
    size_t i = 1; while (i < pts.size() && pts[i].x < x) ++i;
    const auto& a = pts[i - 1]; const auto& b = pts[i];
    const double t = (x - a.x) / juce::jmax (1e-12, b.x - a.x);
    if (curve == 2) return a.db + (b.db - a.db) * t;
    if (curve == 1) { const double s = t * t * (3 - 2 * t); return a.db + (b.db - a.db) * s; }
    const double p0 = i >= 2 ? pts[i - 2].db : a.db, p3 = i + 1 < pts.size() ? pts[i + 1].db : b.db;
    const double t2 = t * t, t3 = t2 * t;
    const double v = 0.5 * ((2 * a.db) + (-p0 + b.db) * t + (2 * p0 - 5 * a.db + 4 * b.db - p3) * t2 + (-p0 + 3 * a.db - 3 * b.db + p3) * t3);
    return juce::jlimit (juce::jmin (a.db, b.db) - 3.0, juce::jmax (a.db, b.db) + 3.0, v);
}

// ---------------------------------------------------------------- AUTO: vokal rider
// target: 0 = baland qismlarga tenglash (90-persentil), 1..4 = -20/-18/-16/-14 dBFS RMS
inline const char* autoTargetNames[] = { "Baland qismlarga tenglash (avto)", "-20 dBFS RMS", "-18 dBFS RMS (vokal standart)", "-16 dBFS RMS", "-14 dBFS RMS" };
inline std::vector<EnvPoint> autoEnvelope (const juce::AudioBuffer<float>& buf, double sr, juce::Range<double> region,
                                           int target, double rangeDb, int speed, double* outTargetDb = nullptr)
{
    const double win = speed == 0 ? 0.15 : speed == 2 ? 0.6 : 0.3, hop = 0.04;
    const int n0 = juce::jlimit (0, buf.getNumSamples(), (int) (region.getStart() * sr));
    const int n1 = juce::jlimit (n0, buf.getNumSamples(), (int) (region.getEnd() * sr));
    const int w = juce::jmax (16, (int) (win * sr)), h = juce::jmax (8, (int) (hop * sr));
    std::vector<double> centres, levels;
    for (int s = n0; s < n1; s += h)
    {
        const int a = juce::jmax (n0, s - w / 2), b = juce::jmin (n1, s + w / 2);
        double sum = 0; int cnt = 0;
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        { auto* d = buf.getReadPointer (ch); for (int i = a; i < b; ++i) { sum += (double) d[i] * d[i]; ++cnt; } }
        levels.push_back (10.0 * std::log10 (juce::jmax (1e-12, sum / juce::jmax (1, cnt))));
        centres.push_back ((double) s / sr);
    }
    if (levels.size() < 3) return { { 0, 0 }, { 1, 0 } };
    const double loudest = *std::max_element (levels.begin(), levels.end());
    const double gate = juce::jmax (-55.0, loudest - 36.0);
    std::vector<double> active; for (double l : levels) if (l > gate) active.push_back (l);
    if (active.empty()) return { { 0, 0 }, { 1, 0 } };
    std::sort (active.begin(), active.end());
    double tgt = active[(size_t) juce::jlimit (0, (int) active.size() - 1, (int) (active.size() * 0.9))];
    if (target >= 1 && target <= 4) tgt = -22.0 + 2.0 * target;
    if (outTargetDb) *outTargetDb = tgt;
    // gain egri chizig'i: sukunatda 0 dB (shovqinni ko'tarmaydi)
    // ibora chetlarida (kirish/so'nish) ko'tarmaymiz: qo'shni kadrlarning eng pasti bo'yicha
    std::vector<double> g (levels.size());
    for (size_t i = 0; i < levels.size(); ++i)
    {
        double lo = levels[i];
        for (int j = -3; j <= 3; ++j) { const int q = (int) i + j; if (q >= 0 && q < (int) levels.size()) lo = juce::jmin (lo, levels[(size_t) q]); }
        const double fade = juce::jlimit (0.0, 1.0, (lo - gate) / 12.0);
        const double want = juce::jlimit (-rangeDb, rangeDb, tgt - levels[i]);
        g[i] = want > 0 ? want * fade : want;          // pasaytirish har doim, ko'tarish faqat barqaror joyda
    }
    // silliqlash (attack/release)
    const int k = speed == 0 ? 1 : speed == 2 ? 5 : 3;
    std::vector<double> sm (g.size());
    for (size_t i = 0; i < g.size(); ++i)
    {
        double sum = 0; int cnt = 0;
        for (int j = -k; j <= k; ++j) { const int q = (int) i + j; if (q >= 0 && q < (int) g.size()) { sum += g[(size_t) q]; ++cnt; } }
        sm[i] = sum / cnt;
    }
    // klipga nisbatan 0..1 va RDP soddalashtirish (0.5 dB aniqlik)
    const double len = juce::jmax (1e-6, region.getLength());
    std::vector<EnvPoint> raw;
    raw.push_back ({ 0.0, sm.front() });
    for (size_t i = 0; i < sm.size(); ++i) raw.push_back ({ juce::jlimit (0.0, 1.0, (centres[i] - region.getStart()) / len), sm[i] });
    raw.push_back ({ 1.0, sm.back() });
    std::vector<bool> keep (raw.size(), false); keep.front() = keep.back() = true;
    std::function<void (size_t, size_t)> rdp = [&] (size_t a, size_t b)
    {
        double md = 0; size_t mi = a;
        for (size_t i = a + 1; i < b; ++i)
        {
            const double t = (raw[i].x - raw[a].x) / juce::jmax (1e-12, raw[b].x - raw[a].x);
            const double d = std::abs (raw[i].db - (raw[a].db + (raw[b].db - raw[a].db) * t));
            if (d > md) { md = d; mi = i; }
        }
        if (md > 0.5) { keep[mi] = true; rdp (a, mi); rdp (mi, b); }
    };
    rdp (0, raw.size() - 1);
    std::vector<EnvPoint> out;
    for (size_t i = 0; i < raw.size(); ++i) if (keep[i]) out.push_back ({ raw[i].x, std::round (raw[i].db * 10.0) / 10.0 });
    return out;
}

// ---------------------------------------------------------------- render
inline bool readFile (te::Engine& e, const juce::File& f, juce::AudioBuffer<float>& buf, double& sr)
{
    std::unique_ptr<juce::AudioFormatReader> r (e.getAudioFileFormatManager().readFormatManager.createReaderFor (f));
    if (! r) return false;
    buf.setSize ((int) juce::jmax (1u, r->numChannels), (int) r->lengthInSamples);
    r->read (&buf, 0, (int) r->lengthInSamples, 0, true, true);
    sr = r->sampleRate;
    return true;
}

// region: klip ko'rinadigan qismi (fayl ichidagi soniyalar)
inline void applyProc (const juce::ValueTree& p, juce::AudioBuffer<float>& b, double sr, juce::Range<double> region)
{
    if (! (bool) p["on"]) return;
    const juce::String t = p["type"];
    const int N = b.getNumSamples(), C = b.getNumChannels();
    const int r0 = juce::jlimit (0, N, (int) (region.getStart() * sr)), r1 = juce::jlimit (r0, N, (int) (region.getEnd() * sr));
    if (t == "gain") b.applyGain (r0, r1 - r0, juce::Decibels::decibelsToGain ((float) (double) p["db"]));
    else if (t == "normalize")
    {
        float pk = 0; for (int c = 0; c < C; ++c) pk = juce::jmax (pk, b.getMagnitude (c, r0, r1 - r0));
        if (pk > 1e-9f) b.applyGain (r0, r1 - r0, juce::Decibels::decibelsToGain ((float) (double) p["db"]) / pk);
    }
    else if (t == "invert") b.applyGain (r0, r1 - r0, -1.0f);
    else if (t == "reverse") b.reverse (r0, r1 - r0);
    else if (t == "silence") for (int c = 0; c < C; ++c) b.clear (c, r0, r1 - r0);
    else if (t == "dc")
        for (int c = 0; c < C; ++c)
        {
            double m = 0; auto* d = b.getWritePointer (c);
            for (int i = r0; i < r1; ++i) m += d[i];
            m /= juce::jmax (1, r1 - r0);
            for (int i = r0; i < r1; ++i) d[i] -= (float) m;
        }
    else if (t == "fadein" || t == "fadeout")
    {
        const int len = juce::jmin (r1 - r0, (int) ((double) p["len"] * sr));
        const int curve = p["curve"];
        for (int i = 0; i < len; ++i)
        {
            double x = (double) i / juce::jmax (1, len);
            double gn = curve == 0 ? x : curve == 1 ? std::sin (x * juce::MathConstants<double>::halfPi) : x * x;
            const int idx = t == "fadein" ? r0 + i : r1 - 1 - i;
            for (int c = 0; c < C; ++c) b.getWritePointer (c)[idx] *= (float) gn;
        }
    }
    else if (t == "envelope")
    {
        const auto pts = points (p); const int curve = p["curve"];
        const double len = juce::jmax (1e-9, region.getLength());
        double lastX = -1, lastG = 1;
        for (int i = 0; i < N; ++i)
        {
            const double x = juce::jlimit (0.0, 1.0, ((double) i / sr - region.getStart()) / len);
            if (std::abs (x - lastX) > 1e-5) { lastX = x; lastG = juce::Decibels::decibelsToGain (envDb (pts, x, curve), -120.0); }
            for (int c = 0; c < C; ++c) b.getWritePointer (c)[i] *= (float) lastG;
        }
    }
}

inline juce::Range<double> clipRegion (te::WaveAudioClip& c)
{
    const double off = c.getPosition().getOffset().inSeconds() * c.getSpeedRatio();
    return { off, off + c.getPosition().getLength().inSeconds() * c.getSpeedRatio() };
}

// AUTO envelope'larni shu klip uchun qayta hisoblash
inline void refreshAuto (te::WaveAudioClip& c, juce::ValueTree st, te::Engine& e, juce::UndoManager* um)
{
    juce::AudioBuffer<float> buf; double sr = 48000;
    if (! readFile (e, originalFile (c), buf, sr)) return;
    const auto region = clipRegion (c);
    for (auto p : st)
    {
        if (p["type"].toString() == "envelope" && (bool) p["auto"])
            setPoints (p, autoEnvelope (buf, sr, region, p["target"], p["range"], p["speed"]), um);
        applyProc (p, buf, sr, region);
    }
}

// stekni qo'llash: yangi fayl yoziladi va klip unga ulanadi (asl fayl saqlanadi)
inline bool render (te::WaveAudioClip& c, te::Engine& e, const juce::File& audioDir)
{
    auto st = stack (c, false);
    const auto orig = originalFile (c);
    bool any = false; if (st.isValid()) for (auto p : st) any |= (bool) p["on"];
    if (! st.isValid() || st.getNumChildren() == 0 || ! any)
    {
        if (c.getSourceFileReference().getFile() != orig) c.getSourceFileReference().setToFile (orig, te::SourceFileReference::PathStyle::chooseBest, false);
        return true;
    }
    juce::AudioBuffer<float> buf; double sr = 48000;
    if (! readFile (e, orig, buf, sr)) return false;
    const auto region = clipRegion (c);
    for (auto p : st) applyProc (p, buf, sr, region);
    audioDir.createDirectory();
    const auto hash = juce::String::toHexString ((juce::int64) st.toXmlString().hashCode64()).substring (0, 8);
    const auto out = audioDir.getChildFile (juce::File::createLegalFileName (orig.getFileNameWithoutExtension() + "_DOP_" + hash) + ".wav");
    if (! out.existsAsFile())
    {
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> os (out.createOutputStream().release());
        if (! os) return false;
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, (unsigned) buf.getNumChannels(), 32, {}, 0));
        if (! w) return false;
        os.release();
        w->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
    }
    c.getSourceFileReference().setToFile (out, te::SourceFileReference::PathStyle::chooseBest, false);
    return true;
}
} // namespace dop

// =============================================================================  Envelope muharriri
class EnvelopeEditor : public juce::Component
{
public:
    std::function<void()> onEdited;
    void setSource (const juce::AudioBuffer<float>* b, double s, juce::Range<double> reg) { buf = b; sr = s; region = reg; buildPeaks(); repaint(); }
    void setProc (juce::ValueTree p, juce::UndoManager* u) { proc = p; um = u; repaint(); }

    static constexpr double topDb = 12.0, botDb = -24.0;
    float dbToY (double db) const { auto r = area(); return r.getY() + (float) ((topDb - db) / (topDb - botDb)) * r.getHeight(); }
    double yToDb (float y) const { auto r = area(); return juce::jlimit (botDb, topDb, topDb - (double) (y - r.getY()) / r.getHeight() * (topDb - botDb)); }
    juce::Rectangle<float> area() const { return getLocalBounds().toFloat().reduced (8, 10); }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        auto r = area();
        g.setColour (Colour (0xff09080b)); g.fillRoundedRectangle (getLocalBounds().toFloat(), 8);
        g.setColour (Colours::white.withAlpha (0.08f)); g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 8, 1);
        for (double db : { 6.0, 0.0, -6.0, -12.0, -18.0 })
        {
            g.setColour (Colours::white.withAlpha (db == 0 ? 0.14f : 0.05f)); g.drawHorizontalLine ((int) dbToY (db), r.getX(), r.getRight());
            label (g, (db > 0 ? "+" : "") + String ((int) db), { r.getRight() - 26, dbToY (db) - 11, 24, 10 }, 8, dim2, Justification::centredRight, 400);
        }
        if (! proc.isValid()) return;
        const auto pts = dop::points (proc); const int curve = proc["curve"];
        // to'lqin: asl (xira) va envelope qo'llangan (yorqin)
        const float mid = r.getCentreY(), hh = r.getHeight() * 0.42f;
        for (int x = 0; x < (int) peaks.size(); ++x)
        {
            const float px = r.getX() + (float) x;
            const float pk = peaks[(size_t) x];
            g.setColour (Colours::white.withAlpha (0.10f)); g.fillRect (px, mid - pk * hh, 1.0f, pk * hh * 2);
            const float gn = jmin (2.5f, Decibels::decibelsToGain ((float) dop::envDb (pts, x / jmax (1.0, (double) peaks.size() - 1), curve)));
            const float v = jmin (1.0f, pk * gn);
            g.setColour (silver.withAlpha (0.55f)); g.fillRect (px, mid - v * hh, 1.0f, v * hh * 2);
        }
        Path p;
        for (int x = 0; x <= (int) r.getWidth(); x += 2)
        {
            const float y = dbToY (dop::envDb (pts, x / jmax (1.0f, r.getWidth()), curve));
            if (x == 0) p.startNewSubPath (r.getX(), y); else p.lineTo (r.getX() + (float) x, y);
        }
        g.setColour ((bool) proc["auto"] ? yellow : red); g.strokePath (p, PathStrokeType (1.6f));
        for (size_t i = 0; i < pts.size(); ++i)
        {
            const float x = r.getX() + (float) pts[i].x * r.getWidth(), y = dbToY (pts[i].db);
            g.setColour (Colour (0xff09080b)); g.fillEllipse (x - 4, y - 4, 8, 8);
            g.setColour ((int) i == hot ? Colours::white : ((bool) proc["auto"] ? yellow : red)); g.drawEllipse (x - 4, y - 4, 8, 8, 1.5f);
        }
        if (hot >= 0 && hot < (int) pts.size())
            label (g, String (pts[(size_t) hot].db, 1) + " dB", { r.getX() + 4, r.getY() + 2, 80, 12 }, 9, text, Justification::centredLeft, 600);
    }
    int pointAt (juce::Point<float> pos) const
    {
        if (! proc.isValid()) return -1;
        const auto pts = dop::points (proc); auto r = area();
        for (size_t i = 0; i < pts.size(); ++i)
            if (pos.getDistanceFrom ({ r.getX() + (float) pts[i].x * r.getWidth(), dbToY (pts[i].db) }) < 7) return (int) i;
        return -1;
    }
    void mouseMove (const juce::MouseEvent& e) override { const int h = pointAt (e.position); if (h != hot) { hot = h; repaint(); } }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! proc.isValid()) return;
        auto pts = dop::points (proc); auto r = area();
        dragIdx = pointAt (e.position);
        if (e.mods.isPopupMenu())
        {
            if (dragIdx > 0 && dragIdx < (int) pts.size() - 1) { pts.erase (pts.begin() + dragIdx); commit (pts); }
            dragIdx = -1; return;
        }
        if (dragIdx < 0)
        {
            const double x = juce::jlimit (0.0, 1.0, (double) (e.position.x - r.getX()) / r.getWidth());
            pts.push_back ({ x, yToDb (e.position.y) });
            std::sort (pts.begin(), pts.end(), [] (auto& a, auto& b) { return a.x < b.x; });
            for (size_t i = 0; i < pts.size(); ++i) if (std::abs (pts[i].x - x) < 1e-9) dragIdx = (int) i;
            commit (pts);
        }
        hot = dragIdx;
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! proc.isValid() || dragIdx < 0) return;
        auto pts = dop::points (proc); auto r = area();
        if (dragIdx >= (int) pts.size()) return;
        const bool edge = dragIdx == 0 || dragIdx == (int) pts.size() - 1;
        if (! edge)
        {
            const double lo = pts[(size_t) dragIdx - 1].x + 0.0005, hi = pts[(size_t) dragIdx + 1].x - 0.0005;
            pts[(size_t) dragIdx].x = juce::jlimit (lo, hi, (double) (e.position.x - r.getX()) / r.getWidth());
        }
        pts[(size_t) dragIdx].db = std::round (yToDb (e.position.y) * 10.0) / 10.0;
        commit (pts, false);
    }
    void mouseUp (const juce::MouseEvent&) override { if (dragIdx >= 0 && onEdited) onEdited(); dragIdx = -1; }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        auto pts = dop::points (proc); const int i = pointAt (e.position);
        if (i > 0 && i < (int) pts.size() - 1) { pts.erase (pts.begin() + i); commit (pts); }
    }
private:
    void commit (const std::vector<dop::EnvPoint>& pts, bool notify = true)
    {
        proc.setProperty ("auto", false, um);           // qo'lda tahrir = endi avto emas
        dop::setPoints (proc, pts, um); repaint();
        if (notify && onEdited) onEdited();
    }
    void buildPeaks()
    {
        peaks.clear();
        if (! buf) return;
        const int W = juce::jmax (10, (int) area().getWidth());
        const int n0 = (int) (region.getStart() * sr), n1 = juce::jmin (buf->getNumSamples(), (int) (region.getEnd() * sr));
        peaks.resize ((size_t) W, 0.0f);
        for (int x = 0; x < W; ++x)
        {
            const int a = n0 + (int) ((double) (n1 - n0) * x / W), b = n0 + (int) ((double) (n1 - n0) * (x + 1) / W);
            float m = 0; for (int c = 0; c < buf->getNumChannels(); ++c) if (b > a) m = juce::jmax (m, buf->getMagnitude (c, a, b - a));
            peaks[(size_t) x] = m;
        }
    }
    void resized() override { buildPeaks(); }
    const juce::AudioBuffer<float>* buf = nullptr; double sr = 48000; juce::Range<double> region;
    std::vector<float> peaks;
    juce::ValueTree proc; juce::UndoManager* um = nullptr; int dragIdx = -1, hot = -1;
};

// =============================================================================  DOP oynasi
class DopComponent : public juce::Component, private juce::Timer
{
public:
    std::function<void()> onApplied;
    DopComponent (AppEngine& a, juce::Array<te::WaveAudioClip*> cl) : app (a)
    {
        for (auto* c : cl) clips.add (c);
        for (auto* b : { &addB, &delB, &upB, &downB, &applyB, &resetB, &playB, &autoB, &c0, &c1, &c2 }) { addAndMakeVisible (b); b->getProperties().set ("radius", 5.0f); }
        addB.setButtonText (U ("+  Process")); delB.setButtonText (U ("O'chirish")); upB.setButtonText (juce::String::fromUTF8 ("\xe2\x96\xb2")); downB.setButtonText (juce::String::fromUTF8 ("\xe2\x96\xbc"));
        applyB.setButtonText (U ("Qo'llash")); resetB.setButtonText (U ("Asliga qaytarish")); playB.setButtonText (juce::String::fromUTF8 ("\xe2\x96\xb6"));
        playB.setTooltip (U ("Klipni loopda tinglash")); playB.setClickingTogglesState (true);
        autoB.setButtonText ("AUTO"); autoB.setTooltip (U ("Balandlikni avtomatik tenglash (vokal rider)"));
        autoB.setColour (juce::TextButton::buttonColourId, yellow.withAlpha (0.22f));
        c0.setButtonText ("Spline"); c1.setButtonText ("Damped"); c2.setButtonText ("Linear");
        for (auto* b : { &c0, &c1, &c2 }) { b->setClickingTogglesState (true); b->setRadioGroupId (77); }
        addAndMakeVisible (autoApply); autoApply.setButtonText ("Auto Apply"); autoApply.setToggleState (true, juce::dontSendNotification);
        addAndMakeVisible (target); for (int i = 0; i < 5; ++i) target.addItem (U (dop::autoTargetNames[i]), i + 1);
        addAndMakeVisible (speed); speed.addItem (U ("Tez"), 1); speed.addItem (U ("O'rta"), 2); speed.addItem (U ("Sekin"), 3);
        addAndMakeVisible (range); range.setSliderStyle (juce::Slider::LinearBar); range.setRange (1, 18, 0.5); range.setTextValueSuffix (U (" dB diapazon"));
        addAndMakeVisible (param); param.setSliderStyle (juce::Slider::LinearBar);
        addAndMakeVisible (curveBox);
        addAndMakeVisible (list); addAndMakeVisible (env);
        list.onSelect = [this] (int i) { sel = i; showProc(); };
        list.onToggle = [this] (int i) { auto p = st().getChild (i); p.setProperty ("on", ! (bool) p["on"], um()); changed(); };

        addB.onClick = [this] { addMenu(); };
        delB.onClick = [this] { if (sel >= 0) { st().removeChild (sel, um()); sel = juce::jmin (sel, st().getNumChildren() - 1); changed(); showProc(); } };
        upB.onClick = [this] { if (sel > 0) { st().moveChild (sel, sel - 1, um()); --sel; changed(); showProc(); } };
        downB.onClick = [this] { if (sel >= 0 && sel < st().getNumChildren() - 1) { st().moveChild (sel, sel + 1, um()); ++sel; changed(); showProc(); } };
        applyB.onClick = [this] { apply(); };
        resetB.onClick = [this] { st().removeAllChildren (um()); sel = -1; showProc(); apply(); };
        playB.onClick = [this] { preview (playB.getToggleState()); };
        autoB.onClick = [this] { runAuto(); };
        for (int i = 0; i < 3; ++i) { auto* b = i == 0 ? &c0 : i == 1 ? &c1 : &c2; b->onClick = [this, i] { if (auto p = cur(); p.isValid()) { p.setProperty ("curve", i, um()); env.repaint(); changed(); } }; }
        target.onChange = [this] { if (auto p = cur(); p.isValid()) { p.setProperty ("target", target.getSelectedId() - 1, um()); if ((bool) p["auto"]) runAuto(); } };
        speed.onChange = [this] { if (auto p = cur(); p.isValid()) { p.setProperty ("speed", speed.getSelectedId() - 1, um()); if ((bool) p["auto"]) runAuto(); } };
        range.onDragEnd = [this] { if (auto p = cur(); p.isValid()) { p.setProperty ("range", range.getValue(), um()); if ((bool) p["auto"]) runAuto(); } };
        param.onValueChange = [this] { if (auto p = cur(); p.isValid()) { p.setProperty (paramId, param.getValue(), um()); changed(); } };
        curveBox.onChange = [this] { if (auto p = cur(); p.isValid()) { p.setProperty ("curve", curveBox.getSelectedId() - 1, um()); changed(); } };
        env.onEdited = [this] { changed(); };

        if (auto* c = first())
        {
            dop::readFile (app.engine, dop::originalFile (*c), source, sr);
            env.setSource (&source, sr, dop::clipRegion (*c));
            auto s = dop::stack (*c, true);
            if (s.getNumChildren() == 0) { s.addChild (dop::makeProc ("envelope"), -1, nullptr); }
            sel = 0;
        }
        showProc();
        setSize (860, 470);
    }
    ~DopComponent() override { stopTimer(); if (previewing) preview (false); }

    te::WaveAudioClip* first() const { return clips.isEmpty() ? nullptr : dynamic_cast<te::WaveAudioClip*> (clips[0].get()); }
    juce::UndoManager* um() const { return &app.edit->getUndoManager(); }
    juce::ValueTree st() const { auto* c = first(); return c ? dop::stack (*c, true) : juce::ValueTree(); }
    juce::ValueTree cur() const { return sel >= 0 ? st().getChild (sel) : juce::ValueTree(); }

    void addMenu()
    {
        juce::PopupMenu m;
        for (int i = 0; i < (int) std::size (dop::procDefs); ++i) m.addItem (i + 1, dop::procDefs[i].name);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addB), [this] (int r)
        {
            if (r <= 0) return;
            st().addChild (dop::makeProc (dop::procDefs[r - 1].type), -1, um());
            sel = st().getNumChildren() - 1; showProc(); changed();
        });
    }
    void runAuto()
    {
        auto p = cur(); auto* c = first();
        if (! p.isValid() || c == nullptr || p["type"].toString() != "envelope") return;
        app.edit->getUndoManager().beginNewTransaction ("Envelope AUTO");
        p.setProperty ("auto", true, um());
        double tgt = 0;
        dop::setPoints (p, dop::autoEnvelope (source, sr, dop::clipRegion (*c), p["target"], p["range"], p["speed"], &tgt), um());
        autoInfo = U ("AUTO: maqsad ") + juce::String (tgt, 1) + juce::String::fromUTF8 (" dBFS RMS \xc2\xb7 ") + juce::String ((int) dop::points (p).size()) + U (" nuqta. Qo'lda to'g'rilash mumkin.");
        env.repaint(); changed(); repaint();
    }
    void changed()
    {
        list.setItems (st()); env.repaint();
        if (autoApply.getToggleState()) startTimer (350);
    }
    void timerCallback() override { stopTimer(); apply(); }
    void apply()
    {
        if (! app.edit) return;
        auto audioDir = app.editFile.getParentDirectory().getChildFile ("Audio");
        auto master = st();
        int ok = 0;
        for (auto& cp : clips)
        {
            auto* c = dynamic_cast<te::WaveAudioClip*> (cp.get()); if (! c) continue;
            if (c != first())
            {
                // bir xil stek boshqa tanlangan kliplarga ham (AUTO har bir klip uchun alohida hisoblanadi)
                auto s = dop::stack (*c, true); s.removeAllChildren (nullptr);
                for (auto p : master) s.addChild (p.createCopy(), -1, nullptr);
                dop::refreshAuto (*c, s, app.engine, nullptr);
            }
            ok += dop::render (*c, app.engine, audioDir) ? 1 : 0;
        }
        lastResult = juce::String (ok) + U (" klip qayta ishlandi");
        if (onApplied) onApplied();
        repaint();
    }
    void preview (bool on)
    {
        auto* c = first(); if (! c || ! app.edit) return;
        auto& tr = app.edit->getTransport();
        if (on)
        {
            oldLoop = tr.getLoopRange(); oldLooping = tr.looping;
            tr.setLoopRange (c->getPosition().time); tr.looping = true; tr.setPosition (c->getPosition().getStart()); tr.play (false);
        }
        else if (previewing) { tr.stop (false, false); tr.setLoopRange (oldLoop); tr.looping = oldLooping; }
        previewing = on;
    }

    void showProc()
    {
        list.setItems (st()); list.selected = sel; list.repaint();
        auto p = cur();
        const juce::String t = p.isValid() ? p["type"].toString() : juce::String();
        const bool isEnv = t == "envelope";
        env.setVisible (isEnv); env.setProc (isEnv ? p : juce::ValueTree(), um());
        for (auto* w : std::initializer_list<juce::Component*> { &autoB, &target, &speed, &range, &c0, &c1, &c2 }) w->setVisible (isEnv);
        if (isEnv)
        {
            const int cv = p["curve"]; (cv == 0 ? c0 : cv == 1 ? c1 : c2).setToggleState (true, juce::dontSendNotification);
            target.setSelectedId ((int) p["target"] + 1, juce::dontSendNotification);
            speed.setSelectedId ((int) p["speed"] + 1, juce::dontSendNotification);
            range.setValue (p["range"], juce::dontSendNotification);
        }
        const bool hasParam = t == "gain" || t == "normalize" || t == "fadein" || t == "fadeout";
        param.setVisible (hasParam); curveBox.setVisible (t == "fadein" || t == "fadeout");
        if (t == "gain")      { paramId = "db"; param.setRange (-24, 24, 0.1); param.setTextValueSuffix (" dB"); }
        if (t == "normalize") { paramId = "db"; param.setRange (-24, 0, 0.1); param.setTextValueSuffix (U (" dBFS cho'qqi")); }
        if (t == "fadein" || t == "fadeout")
        {
            paramId = "len"; param.setRange (0.01, 20, 0.01); param.setTextValueSuffix (U (" s")); param.setSkewFactorFromMidPoint (1.0);
            curveBox.clear (juce::dontSendNotification); curveBox.addItem ("Linear", 1); curveBox.addItem ("Sine", 2); curveBox.addItem ("Exponential", 3);
            curveBox.setSelectedId ((int) p["curve"] + 1, juce::dontSendNotification);
        }
        if (hasParam) param.setValue (p[paramId], juce::dontSendNotification);
        resized(); repaint();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10);
        auto top = r.removeFromTop (30);
        addB.setBounds (top.removeFromLeft (110)); top.removeFromLeft (14);
        playB.setBounds (top.removeFromLeft (34)); top.removeFromLeft (14);
        autoApply.setBounds (top.removeFromLeft (110));
        resetB.setBounds (top.removeFromRight (140)); top.removeFromRight (6);
        applyB.setBounds (top.removeFromRight (100));
        r.removeFromTop (8);
        auto left = r.removeFromLeft (260);
        auto lb = left.removeFromBottom (28);
        delB.setBounds (lb.removeFromLeft (100)); lb.removeFromLeft (6); upB.setBounds (lb.removeFromLeft (30)); lb.removeFromLeft (4); downB.setBounds (lb.removeFromLeft (30));
        left.removeFromBottom (6);
        list.setBounds (left);
        r.removeFromLeft (10);
        editorArea = r;
        auto er = r.withTrimmedTop (24);
        auto row = er.removeFromTop (26);
        c0.setBounds (row.removeFromLeft (66)); row.removeFromLeft (3); c1.setBounds (row.removeFromLeft (70)); row.removeFromLeft (3); c2.setBounds (row.removeFromLeft (62));
        row.removeFromLeft (14);
        autoB.setBounds (row.removeFromLeft (64)); row.removeFromLeft (6);
        target.setBounds (row.removeFromLeft (juce::jmin (220, row.getWidth())));
        er.removeFromTop (6);
        auto row2 = er.removeFromTop (22);
        range.setBounds (row2.removeFromLeft (200)); row2.removeFromLeft (8); speed.setBounds (row2.removeFromLeft (100));
        er.removeFromTop (8);
        er.removeFromBottom (20);
        env.setBounds (er);
        auto pr = r.withTrimmedTop (34);
        param.setBounds (pr.removeFromTop (26).withWidth (300));
        pr.removeFromTop (8); curveBox.setBounds (pr.removeFromTop (24).withWidth (180));
    }
    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        g.fillAll (bg1);
        auto p = cur();
        caps (g, p.isValid() ? dop::procName (p["type"]) : U ("Jarayon tanlanmagan"), editorArea.toFloat().withHeight (18), 10, redSoft);
        if (auto* c = first())
            label (g, c->getName() + (clips.size() > 1 ? "  (+" + String (clips.size() - 1) + U (" klip)") : String()),
                   editorArea.toFloat().withHeight (18).withTrimmedLeft (180), 10, dim, Justification::centredRight, 500);
        const String info = autoInfo.isNotEmpty() && p.isValid() && (bool) p["auto"] ? autoInfo : lastResult;
        label (g, info, editorArea.toFloat().removeFromBottom (18), 9.5f, dim, Justification::centredLeft, 400);
    }

    // ---------------------------------------------------------------- jarayonlar ro'yxati
    struct ProcList : public juce::Component
    {
        std::function<void (int)> onSelect, onToggle;
        int selected = -1;
        juce::ValueTree items;
        void setItems (juce::ValueTree v) { items = v; repaint(); }
        void paint (juce::Graphics& g) override
        {
            using namespace juce;
            g.setColour (Colour (0xff09080b)); g.fillRoundedRectangle (getLocalBounds().toFloat(), 8);
            caps (g, "NAME", { 30, 6, 120, 12 }, 8.5f); caps (g, "STATUS", { (float) getWidth() - 60, 6, 54, 12 }, 8.5f);
            if (! items.isValid()) return;
            for (int i = 0; i < items.getNumChildren(); ++i)
            {
                auto p = items.getChild (i);
                auto r = Rectangle<float> (4, 24.0f + (float) i * 24, (float) getWidth() - 8, 22);
                if (i == selected) { g.setColour (green.withAlpha (0.22f)); g.fillRoundedRectangle (r, 5); }
                label (g, String (i + 1), r.withWidth (22), 9, dim2, Justification::centred, 500);
                label (g, dop::procName (p["type"]) + ((bool) p["auto"] ? "  (AUTO)" : ""), r.withTrimmedLeft (26), 10.5f, (bool) p["on"] ? text : dim2, Justification::centredLeft, 500);
                label (g, (bool) p["on"] ? String::fromUTF8 ("\xe2\x9c\x93") : String::fromUTF8 ("\xe2\x80\x94"), r.withTrimmedLeft (r.getWidth() - 44), 13, (bool) p["on"] ? green : dim2, Justification::centred, 700);
            }
        }
        void mouseDown (const juce::MouseEvent& e) override
        {
            const int i = (int) ((e.position.y - 24) / 24);
            if (! items.isValid() || i < 0 || i >= items.getNumChildren()) return;
            if (e.position.x > getWidth() - 50) { if (onToggle) onToggle (i); }
            selected = i; if (onSelect) onSelect (i); repaint();
        }
    };

private:
    AppEngine& app;
    juce::Array<te::Clip::Ptr> clips;
    juce::AudioBuffer<float> source; double sr = 48000;
    juce::TextButton addB, delB, upB, downB, applyB, resetB, playB, autoB, c0, c1, c2;
    juce::ToggleButton autoApply;
    juce::ComboBox target, speed, curveBox; juce::Slider range, param; juce::Identifier paramId { "db" };
    ProcList list; EnvelopeEditor env;
    juce::Rectangle<int> editorArea;
    int sel = -1; juce::String autoInfo, lastResult;
    bool previewing = false, oldLooping = false; te::TimeRange oldLoop;
};

class DopWindow : public juce::DocumentWindow
{
public:
    DopWindow (AppEngine& a, juce::Array<te::WaveAudioClip*> clips, std::function<void()> applied)
        : juce::DocumentWindow ("Direct Offline Processing", bg1, juce::DocumentWindow::closeButton)
    {
        auto* c = new DopComponent (a, clips); c->onApplied = applied;
        if (! clips.isEmpty()) setName ("Direct Offline Processing: " + clips[0]->getName());
        setUsingNativeTitleBar (true); setContentOwned (c, true); setResizable (true, false);
        setResizeLimits (720, 400, 1600, 1000);
        centreWithSize (getWidth(), getHeight()); setAlwaysOnTop (true); setVisible (true);
    }
    std::function<void()> onClose;
    void closeButtonPressed() override { if (onClose) onClose(); }
};

// =============================================================================  Render Selection (Cubase kabi)
struct RenderOptions
{
    bool separate = true;          // As Separate Events / As One Event
    int processing = 1;            // 0 Dry · 1 Channel Settings · 2 Complete Signal Path · 3 + Master FX
    bool tail = false; double tailSec = 2.0;
    int bitDepth = 32;             // 16 · 24 · 32 (float)
    bool customName = false; juce::String name;
    juce::File folder;
    int source = 0;                // 0 Mute · 1 Keep · 2 Remove
};

inline juce::String renderSelection (AppEngine& app, const juce::Array<te::Clip*>& clips, const RenderOptions& o)
{
    auto* edit = app.edit.get();
    if (edit == nullptr || clips.isEmpty()) return U ("Avval klip(lar)ni tanlang.");
    auto dir = o.folder.isDirectory() ? o.folder : app.editFile.getParentDirectory().getChildFile ("Audio");
    dir.createDirectory();
    auto& dm = app.engine.getDeviceManager();
    std::map<te::AudioTrack*, juce::Array<te::Clip*>> byTrack;
    for (auto* c : clips) if (auto* t = dynamic_cast<te::AudioTrack*> (c->getTrack())) byTrack[t].add (c);
    edit->getUndoManager().beginNewTransaction ("Render Selection");
    Routing rt (app);
    int made = 0;
    for (auto& [t, cl] : byTrack)
    {
        // guruhlar: alohida (har bir klip) yoki bitta (trek bo'yicha)
        juce::Array<juce::Array<te::Clip*>> groups;
        if (o.separate) for (auto* c : cl) groups.add ({ c }); else groups.add (cl);
        te::AudioTrack* dest = nullptr;
        if (o.processing > 0 || o.source == 1)
        {
            dest = edit->insertNewAudioTrack (te::TrackInsertPoint (nullptr, t), nullptr).get();
            if (dest) { dest->setName (t->getName() + " (Render)"); dest->setColour (t->getColour()); }
        }
        int idx = 0;
        for (auto& g : groups)
        {
            auto a = g[0]->getPosition().getStart(), b = g[0]->getPosition().getEnd();
            for (auto* c : g) { a = juce::jmin (a, c->getPosition().getStart()); b = juce::jmax (b, c->getPosition().getEnd()); }
            if (o.tail) b = b + te::TimeDuration::fromSeconds (o.tailSec);
            juce::String nm = o.customName && o.name.isNotEmpty() ? o.name + (groups.size() > 1 ? "-" + juce::String (++idx).paddedLeft ('0', 2) : juce::String())
                                                                  : (g.size() == 1 ? g[0]->getName() : t->getName()) + "-render";
            const auto file = dir.getNonexistentChildFile (juce::File::createLegalFileName (nm), ".wav");
            te::Renderer::Parameters r (*edit);
            r.destFile = file; r.audioFormat = app.engine.getAudioFileFormatManager().getWavFormat();
            r.bitDepth = o.bitDepth; r.ditheringEnabled = o.bitDepth < 32;
            r.sampleRateForAudio = dm.getSampleRate() > 7000 ? dm.getSampleRate() : 48000.0;
            r.blockSizeForAudio  = dm.getBlockSize() > 0 ? dm.getBlockSize() : 512;
            r.time = { a, b };
            auto all = te::getAllTracks (*edit);
            juce::BigInteger bits; bits.setBit (all.indexOf (t));
            if (o.processing >= 2) for (auto* fx : rt.fxTracks()) bits.setBit (all.indexOf (fx));
            r.tracksToDo = bits;
            r.allowedClips = g;
            r.usePlugins = o.processing > 0 || dynamic_cast<te::MidiClip*> (g[0]) != nullptr;
            r.useMasterPlugins = o.processing == 3;
            {
                ScopedControlRoomBypass crb (*edit);
                te::Renderer::renderToFile ("Render Selection", r);
            }
            if (! file.existsAsFile()) continue;
            te::AudioFile af (app.engine, file);
            const auto len = te::TimeDuration::fromSeconds (af.getLength());
            auto* target = dest != nullptr ? dest : t;
            if (dest == nullptr && o.source != 1) for (auto* c : g) c->removeFromParent();   // Dry + Mute/Remove: o'rniga qo'yiladi
            target->insertWaveClip (nm, file, { { a, a + len }, {} }, false);
            if (dest != nullptr) for (auto* c : g) { if (o.source == 0) c->setMuted (true); if (o.source == 2) c->removeFromParent(); }
            ++made;
        }
    }
    return made > 0 ? U ("Render Selection: ") + juce::String (made) + U (" ta audio yaratildi") : U ("Render bajarilmadi");
}

class RenderDialog : public juce::Component
{
public:
    std::function<void (const RenderOptions&)> onRender;
    std::function<void()> onClose;
    explicit RenderDialog (AppEngine& a) : app (a)
    {
        auto& pf = a.engine.getPropertyStorage().getPropertiesFile();
        for (auto* c : std::initializer_list<juce::Component*> { &mode, &tailMode, &tail, &bits, &custom, &name, &folderBox, &folderB, &sourceBox, &renderB, &closeB }) addAndMakeVisible (c);
        mode.addItem ("As Separate Events", 1); mode.addItem ("As One Event", 2); mode.setSelectedId (pf.getIntValue ("azRenMode", 1));
        const char* pn[] = { "Dry (Transfer Channel Settings)", "Channel Settings", "Complete Signal Path", "Complete Signal Path + Master FX" };
        for (int i = 0; i < 4; ++i)
        {
            auto* b = proc.add (new juce::ToggleButton (pn[i])); b->setRadioGroupId (91); addAndMakeVisible (b);
        }
        proc[juce::jlimit (0, 3, pf.getIntValue ("azRenProc", 1))]->setToggleState (true, juce::dontSendNotification);
        tailMode.addItem ("Off", 1); tailMode.addItem ("On", 2); tailMode.setSelectedId (pf.getBoolValue ("azRenTail", false) ? 2 : 1);
        tail.setText (juce::String (pf.getDoubleValue ("azRenTailSec", 2.0), 3)); tail.setJustification (juce::Justification::centredRight);
        tailMode.onChange = [this] { tail.setEnabled (tailMode.getSelectedId() == 2); }; tail.setEnabled (tailMode.getSelectedId() == 2);
        bits.addItem ("16 bit", 16); bits.addItem ("24 bit", 24); bits.addItem ("32 bit float", 32); bits.setSelectedId (pf.getIntValue ("azRenBits", 32));
        custom.setButtonText ("Use Custom Name"); name.setTextToShowWhenEmpty ("Azimxodjayev Music - Render", dim2);
        custom.onClick = [this] { name.setEnabled (custom.getToggleState()); }; name.setEnabled (false);
        folder = juce::File (pf.getValue ("azRenFolder"));
        if (! folder.isDirectory()) folder = a.editFile.getParentDirectory().getChildFile ("Audio");
        folderBox.setText (folder.getFullPathName()); folderBox.setReadOnly (true);
        folderB.setButtonText ("..."); folderB.onClick = [this] { chooseFolder(); };
        sourceBox.addItem ("Mute Source Events", 1); sourceBox.addItem ("Keep Source Events", 2); sourceBox.addItem ("Remove Source Events", 3);
        sourceBox.setSelectedId (pf.getIntValue ("azRenSrc", 1));
        renderB.setButtonText ("Render"); closeB.setButtonText (U ("Yopish"));
        renderB.onClick = [this] { save(); if (onRender) onRender (options()); };
        closeB.onClick = [this] { if (onClose) onClose(); };
        setSize (380, 600);
    }
    RenderOptions options() const
    {
        RenderOptions o;
        o.separate = mode.getSelectedId() == 1;
        for (int i = 0; i < 4; ++i) if (proc[i]->getToggleState()) o.processing = i;
        o.tail = tailMode.getSelectedId() == 2; o.tailSec = juce::jlimit (0.0, 60.0, tail.getText().getDoubleValue());
        o.bitDepth = bits.getSelectedId(); o.customName = custom.getToggleState(); o.name = name.getText();
        o.folder = folder; o.source = sourceBox.getSelectedId() - 1;
        return o;
    }
    void save()
    {
        auto& pf = app.engine.getPropertyStorage().getPropertiesFile(); auto o = options();
        pf.setValue ("azRenMode", mode.getSelectedId()); pf.setValue ("azRenProc", o.processing); pf.setValue ("azRenTail", o.tail);
        pf.setValue ("azRenTailSec", o.tailSec); pf.setValue ("azRenBits", o.bitDepth); pf.setValue ("azRenFolder", o.folder.getFullPathName()); pf.setValue ("azRenSrc", sourceBox.getSelectedId());
    }
    void chooseFolder()
    {
        chooser = std::make_unique<juce::FileChooser> ("File Location", folder);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this] (const juce::FileChooser& fc)
        { if (fc.getResult().isDirectory()) { folder = fc.getResult(); folderBox.setText (folder.getFullPathName()); } });
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (16, 12);
        auto row = [&] (int h) { auto x = r.removeFromTop (h); r.removeFromTop (6); return x; };
        auto rl = [&] (juce::Component& c, int h = 24) { auto x = row (h); x.removeFromLeft (150); c.setBounds (x); };
        rl (mode); row (22);
        for (auto* b : proc) b->setBounds (row (22).withTrimmedLeft (6));
        row (22);
        rl (tailMode); rl (tail); rl (bits); row (22);
        custom.setBounds (row (22).withTrimmedLeft (6)); name.setBounds (row (24).withTrimmedLeft (6)); row (22);
        auto fr = row (24); folderB.setBounds (fr.removeFromRight (30)); fr.removeFromRight (4); folderBox.setBounds (fr); row (22);
        rl (sourceBox);
        auto br = getLocalBounds().reduced (16, 12).removeFromBottom (28);
        closeB.setBounds (br.removeFromRight (90)); br.removeFromRight (8); renderB.setBounds (br.removeFromRight (90));
    }
    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        g.fillAll (bg1);
        auto r = getLocalBounds().reduced (16, 12).toFloat();
        auto lab = [&] (const String& s, float y, bool head = false)
        {
            if (head) { label (g, s, { r.getX(), y, r.getWidth(), 22 }, 11, text, Justification::centredLeft, 700);
                        g.setColour (Colours::white.withAlpha (0.08f)); g.drawHorizontalLine ((int) (y + 21), r.getX() + 90, r.getRight()); }
            else label (g, s, { r.getX() + 6, y, 140, 24 }, 10.5f, silver, Justification::centredLeft, 500);
        };
        lab ("Mode", (float) mode.getY());
        lab ("Processing", (float) proc[0]->getY() - 28, true);
        lab ("Properties", (float) tailMode.getY() - 28, true);
        lab ("Tail Mode", (float) tailMode.getY()); lab ("Tail Size (s)", (float) tail.getY()); lab ("Bit Depth", (float) bits.getY());
        lab ("File Name Settings", (float) custom.getY() - 28, true);
        lab ("File Location", (float) folderBox.getY() - 28, true);
        lab ("Source Event Settings", (float) sourceBox.getY() - 28, true);
        lab ("Source Events", (float) sourceBox.getY());
    }
private:
    AppEngine& app;
    juce::ComboBox mode, tailMode, bits, sourceBox; juce::TextEditor tail, name, folderBox;
    juce::OwnedArray<juce::ToggleButton> proc; juce::ToggleButton custom; juce::TextButton folderB, renderB, closeB;
    juce::File folder; std::unique_ptr<juce::FileChooser> chooser;
};

class RenderWindow : public juce::DocumentWindow
{
public:
    RenderWindow (AppEngine& a, std::function<void (const RenderOptions&)> doRender, std::function<void()> closed)
        : juce::DocumentWindow ("Render Selection", bg1, juce::DocumentWindow::closeButton), onClosed (closed)
    {
        auto* c = new RenderDialog (a);
        c->onRender = std::move (doRender); c->onClose = [this] { closeButtonPressed(); };
        setUsingNativeTitleBar (true); setContentOwned (c, true); centreWithSize (getWidth(), getHeight());
        setAlwaysOnTop (true); setVisible (true);
    }
    void closeButtonPressed() override { if (onClosed) juce::MessageManager::callAsync (onClosed); }
    std::function<void()> onClosed;
};
}
