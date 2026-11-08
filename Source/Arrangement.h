// =============================================================================
//  AZ MUS — aranjirovka: vaqt chizg'ichi, trek sarlavhalari, kliplar, kursor
// =============================================================================
#pragma once
#include "AppEngine.h"
#include <set>

namespace az
{
// Umumiy ko'rinish holati (zoom, scroll, tanlangan trek)
struct ViewState : public juce::ChangeBroadcaster
{
    double pxPerSec = 40.0, startSec = 0.0;
    int trackH = 72, scrollY = 0;
    te::EditItemID selectedTrack;
    bool snap = true;
    int tool = 0;     // 0 tanlash, 1 qaychi, 2 o'chirg'ich, 3 mute

    // ---- Grid (Logic X tuzilmasi: Smart · Bar · Beat · Division) + Cubase bo'linmalari
    enum GridMode { gridSmart = 0, gridBar, gridBeat, gridDivision };
    int gridMode = gridDivision;
    double division = 0.5;                 // choraklarda (beat): 1/8 = 0.5, 1/8T = 1/3, 1/8. = 0.75
    // ---- Snap turi (Cubase "Bar" qutisi / Logic "Snap Regions to Relative Value")
    enum SnapType { snapGrid = 0, snapGridRelative, snapEvents, snapCursor };
    int snapType = snapGrid;
    // ---- vaqt formati (Bars+Beats · Seconds · Timecode · Samples)
    enum TimeFormat { fmtBars = 0, fmtSeconds, fmtTimecode, fmtSamples };
    int timeFormat = fmtBars; int fps = 25;
    // ---- Quantize
    double quantize = 0.5; bool linkQ = true;
    bool transientMagnet = true;        // qaychi: audiodagi zarba boshiga yopishish

    float secToX (double s) const { return (float) ((s - startSec) * pxPerSec); }
    double xToSec (float x) const { return startSec + (double) x / pxPerSec; }
    void changed() { sendChangeMessage(); }

    static juce::String divisionName (double beats)
    {
        struct D { double b; const char* n; };
        static const D ds[] = { {4,"1/1"},{2,"1/2"},{1,"1/4"},{0.5,"1/8"},{0.25,"1/16"},{0.125,"1/32"},{0.0625,"1/64"},{0.03125,"1/128"} };
        for (auto& d : ds)
        {
            if (std::abs (beats - d.b) < 1e-6) return d.n;
            if (std::abs (beats - d.b * 2.0 / 3.0) < 1e-6) return juce::String (d.n) + "T";
            if (std::abs (beats - d.b * 1.5) < 1e-6) return juce::String (d.n) + ".";
        }
        return juce::String (beats, 3);
    }
    juce::String gridName() const
    {
        if (gridMode == gridSmart) return "Smart";
        if (gridMode == gridBar) return "Bar";
        if (gridMode == gridBeat) return "Beat";
        return divisionName (division);
    }
    // joriy grid qadami (chorak = 1 beat)
    double gridBeats (te::Edit& e) const
    {
        auto& ts = e.tempoSequence;
        const auto* sig = ts.getTimeSig (0);
        const double barBeats = sig ? sig->numerator * 4.0 / sig->denominator : 4.0;
        switch (gridMode)
        {
            case gridBar:  return barBeats;
            case gridBeat: return sig ? 4.0 / sig->denominator : 1.0;
            case gridDivision: return division;
            default: break;
        }
        // Smart: zoomga qarab eng kichik o'qiladigan qadam (>= 14 px)
        const double beatPx = pxPerSec * 60.0 / ts.getTempo (0)->getBpm();
        for (double g : { 0.0625, 0.125, 0.25, 0.5, 1.0 }) if (beatPx * g >= 14.0) return g;
        return beatPx * barBeats >= 14.0 ? barBeats : barBeats * 4.0;
    }
};

inline te::AudioTrack* findTrack (te::Edit& e, te::EditItemID id)
{
    for (auto* t : te::getAudioTracks (e)) if (t->itemID == id) return t;
    return nullptr;
}

inline te::TimePosition snapToGrid (te::Edit& e, double sec, double gridBeats)
{
    auto& ts = e.tempoSequence;
    const double b = ts.toBeats (te::TimePosition::fromSeconds (juce::jmax (0.0, sec))).inBeats();
    return ts.toTime (te::BeatPosition::fromBeats (juce::jmax (0.0, std::round (b / gridBeats) * gridBeats)));
}

// exclude: sudralayotgan klip (o'z chetiga yopishmasin)
inline te::TimePosition snapTime (te::Edit& e, double sec, const ViewState& vs, bool force = false, const te::Clip* exclude = nullptr)
{
    if (! vs.snap && ! force) return te::TimePosition::fromSeconds (juce::jmax (0.0, sec));
    const double tolSec = 10.0 / vs.pxPerSec;
    if (vs.snapType == ViewState::snapEvents)
    {
        double best = -1, bd = tolSec;
        for (auto* t : te::getAudioTracks (e))
            for (auto* c : t->getClips())
            {
                if (c == exclude) continue;
                for (double edge : { c->getPosition().getStart().inSeconds(), c->getPosition().getEnd().inSeconds() })
                    if (std::abs (edge - sec) < bd) { bd = std::abs (edge - sec); best = edge; }
            }
        if (best >= 0) return te::TimePosition::fromSeconds (best);
    }
    if (vs.snapType == ViewState::snapCursor)
    {
        const double p = e.getTransport().getPosition().inSeconds();
        if (std::abs (p - sec) < tolSec * 1.5) return te::TimePosition::fromSeconds (p);
    }
    return snapToGrid (e, sec, vs.gridBeats (e));
}

// Grid Relative: klipning gridga nisbatan joyi saqlanadi, siljish grid qadamiga yaxlitlanadi
inline te::TimePosition snapMove (te::Edit& e, te::TimePosition origStart, double dtSec, const ViewState& vs, const te::Clip* exclude = nullptr)
{
    if (vs.snap && vs.snapType == ViewState::snapGridRelative)
    {
        auto& ts = e.tempoSequence;
        const double b0 = ts.toBeats (origStart).inBeats();
        const double b1 = ts.toBeats (te::TimePosition::fromSeconds (juce::jmax (0.0, origStart.inSeconds() + dtSec))).inBeats();
        const double g = vs.gridBeats (e);
        return ts.toTime (te::BeatPosition::fromBeats (juce::jmax (0.0, b0 + std::round ((b1 - b0) / g) * g)));
    }
    return snapTime (e, origStart.inSeconds() + dtSec, vs, false, exclude);
}

// vaqtni tanlangan formatda yozish (LCD, ruler)
inline juce::String formatTime (te::Edit& e, te::TimePosition pos, const ViewState& vs, double sampleRate = 48000.0)
{
    const double s = pos.inSeconds();
    switch (vs.timeFormat)
    {
        case ViewState::fmtSeconds:  return juce::String::formatted ("%d:%02d.%03d", (int) s / 60, (int) s % 60, (int) ((s - std::floor (s)) * 1000));
        case ViewState::fmtTimecode: { const int t = (int) s; return juce::String::formatted ("%02d:%02d:%02d:%02d", t / 3600, (t / 60) % 60, t % 60, (int) ((s - t) * vs.fps)); }
        case ViewState::fmtSamples:  return juce::String ((juce::int64) std::llround (s * sampleRate));
        default: break;
    }
    auto& ts = e.tempoSequence;
    const auto bb = ts.toBarsAndBeats (pos);
    const double frac = bb.getFractionalBeats().inBeats();
    const int six = (int) (frac * 4.0) + 1, tick = (int) ((frac * 4.0 - (six - 1)) * 240.0);
    return juce::String::formatted ("%03d.%d.%d.%03d", bb.bars + 1, bb.getWholeBeats() + 1, six, tick);
}

// Cubase kabi: Alt bosib turilganda kichik oq glyansli qaychi kursori
inline juce::Image makeScissorsImage()
{
    {
        const int S = 28;
        juce::Image img (juce::Image::ARGB, S, S, true);
        juce::Graphics g (img);
        auto blade = [&] (bool left)
        {
            juce::Path b;
            const float sx = left ? 9.0f : 19.0f, ex = left ? 17.5f : 10.5f;
            b.startNewSubPath (ex, 2.0f);
            b.quadraticTo ((sx + ex) * 0.5f + (left ? -1.5f : 1.5f), 9.0f, sx, 15.5f);
            b.lineTo (sx + (left ? 2.4f : -2.4f), 16.5f);
            b.quadraticTo ((sx + ex) * 0.5f + (left ? 1.0f : -1.0f), 9.5f, ex + (left ? -0.6f : 0.6f), 2.0f);
            b.closeSubPath();
            return b;
        };
        juce::Path blades; blades.addPath (blade (true)); blades.addPath (blade (false));
        juce::Path rings;
        rings.addEllipse (3.5f, 16.5f, 8.5f, 8.5f); rings.addEllipse (6.0f, 19.0f, 3.5f, 3.5f);
        rings.addEllipse (16.0f, 16.5f, 8.5f, 8.5f); rings.addEllipse (18.5f, 19.0f, 3.5f, 3.5f);
        rings.setUsingNonZeroWinding (false);   // ichki doiralar teshik bo'ladi
        // qora kontur (har qanday fonda ko'rinadi)
        g.setColour (juce::Colours::black.withAlpha (0.85f));
        g.strokePath (blades, juce::PathStrokeType (2.2f)); g.strokePath (rings, juce::PathStrokeType (2.0f));
        // oq glyans
        g.setGradientFill (juce::ColourGradient (juce::Colours::white, 10, 2, juce::Colour (0xffc9ced6), 18, 26, false));
        g.fillPath (blades); g.fillPath (rings);
        g.setColour (juce::Colours::white.withAlpha (0.9f)); g.fillEllipse (13.0f, 14.5f, 2.2f, 2.2f);       // vint
        g.setColour (juce::Colours::white.withAlpha (0.55f)); g.drawLine (12.5f, 4.0f, 10.5f, 12.0f, 0.8f); // yaltirash
        return img;
    }
}
inline juce::MouseCursor makeScissorsCursor() { return juce::MouseCursor (makeScissorsImage(), 14, 2); }

// audiodagi zarba (transient) boshlanishlari, soniyalarda (fayl ichida)
inline std::vector<double> detectTransients (te::Engine& eng, const juce::File& f)
{
    std::vector<double> out;
    std::unique_ptr<juce::AudioFormatReader> r (eng.getAudioFileFormatManager().readFormatManager.createReaderFor (f));
    if (! r) return out;
    const double sr = r->sampleRate;
    const int hop = juce::jmax (32, (int) (sr * 0.004));
    const juce::int64 total = std::min<juce::int64> (r->lengthInSamples, (juce::int64) (sr * 1200));
    juce::AudioBuffer<float> b ((int) juce::jmax (1u, r->numChannels), (int) std::min<juce::int64> (total, (juce::int64) sr * 20));
    std::vector<float> env; env.reserve ((size_t) (total / hop + 1));
    std::vector<float> mono; mono.reserve ((size_t) total);
    for (juce::int64 pos = 0; pos < total; pos += b.getNumSamples())
    {
        const int n = (int) std::min<juce::int64> (b.getNumSamples(), total - pos);
        r->read (&b, 0, n, pos, true, true);
        for (int i = 0; i < n; ++i) { float v = 0; for (int c = 0; c < b.getNumChannels(); ++c) v += std::abs (b.getSample (c, i)); mono.push_back (v / b.getNumChannels()); }
    }
    for (size_t s = 0; s + (size_t) hop <= mono.size(); s += (size_t) hop)
    {
        double sum = 0; for (int i = 0; i < hop; ++i) sum += (double) mono[s + (size_t) i] * mono[s + (size_t) i];
        env.push_back ((float) (10.0 * std::log10 (juce::jmax (1e-12, sum / hop))));
    }
    const int minGap = (int) (0.06 * sr / hop);
    int last = -minGap;
    for (int i = 6; i < (int) env.size(); ++i)
    {
        float before = -200; for (int k = 2; k <= 6; ++k) before = juce::jmax (before, env[(size_t) (i - k)]);
        if (env[(size_t) i] > -48.0f && env[(size_t) i] - before > 6.0f && i - last > minGap)
        {
            // aniq boshlanish: shu kadrdan oldin signal 15% cho'qqidan oshgan birinchi namuna
            const size_t a = (size_t) juce::jmax (0, (i - 2) * hop), z = juce::jmin (mono.size(), (size_t) (i + 3) * (size_t) hop);
            float pk = 0; for (size_t q = a; q < z; ++q) pk = juce::jmax (pk, mono[q]);
            size_t at = (size_t) i * (size_t) hop;
            for (size_t q = a; q < z; ++q) if (mono[q] > pk * 0.15f) { at = q; break; }
            out.push_back ((double) at / sr); last = i;
        }
    }
    return out;
}

// =============================================================================  Ruler
class TimeRuler : public juce::Component
{
public:
    TimeRuler (AppEngine& a, ViewState& v) : app (a), vs (v) {}

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        g.fillAll (bg2);
        if (! app.edit) return;
        auto& e = *app.edit; auto& ts = e.tempoSequence;
        const float W = (float) getWidth(), H = (float) getHeight();
        // markerlar (qo'shiq bo'limlari)
        auto markers = e.getMarkerManager().getMarkers();
        for (auto* m : markers)
        {
            const auto r = m->getEditTimeRange();
            const float x1 = vs.secToX (r.getStart().inSeconds()), x2 = vs.secToX (r.getEnd().inSeconds());
            if (x2 < 0 || x1 > W) continue;
            const auto c = m->getColour() == Colour() ? blue : m->getColour();
            g.setColour (c.withAlpha (0.2f)); g.fillRect (x1 + 1, 4.0f, x2 - x1 - 2, 18.0f);
            g.setColour (c); g.fillRect (x1 + 1, 4.0f, 3.0f, 18.0f);
            label (g, m->getName().toUpperCase(), { x1 + 9, 4, x2 - x1 - 12, 18 }, 10, text, Justification::centredLeft, 600);
        }
        // taktlar
        const double beatPx = vs.pxPerSec * 60.0 / ts.getTempo (0)->getBpm();
        const int barStep = beatPx * 4 > 70 ? 1 : beatPx * 4 > 30 ? 2 : beatPx * 4 > 14 ? 4 : 8;
        const int firstBar = jmax (0, ts.toBarsAndBeats (te::TimePosition::fromSeconds (jmax (0.0, vs.startSec))).bars);
        for (int bar = firstBar; ; ++bar)
        {
            const float x = vs.secToX (ts.toTime (te::tempo::BarsAndBeats { bar, {} }).inSeconds());
            if (x > W) break;
            const bool major = bar % barStep == 0;
            g.setColour (Colours::white.withAlpha (major ? 0.22f : 0.1f));
            g.fillRect (x, major ? 32.0f : 42.0f, 1.0f, major ? H - 32 : H - 42);
            if (major) label (g, vs.timeFormat == ViewState::fmtBars ? String (bar + 1)
                                 : formatTime (e, ts.toTime (te::tempo::BarsAndBeats { bar, {} }), vs, app.engine.getDeviceManager().getSampleRate() > 0 ? app.engine.getDeviceManager().getSampleRate() : 48000.0),
                              { x + 4, 30, vs.timeFormat == ViewState::fmtBars ? 40.0f : 90.0f, 14 }, 10, dim, Justification::centredLeft, 400);
            if (beatPx > 22)
                for (int b = 1; b < 4; ++b)
                {
                    const float bx = vs.secToX (ts.toTime (te::tempo::BarsAndBeats { bar, te::BeatDuration::fromBeats (b) }).inSeconds());
                    g.setColour (Colours::white.withAlpha (0.07f)); g.fillRect (bx, 48.0f, 1.0f, H - 48);
                }
        }
        // loop
        auto& tr = e.getTransport();
        const auto lr = tr.getLoopRange();
        if (lr.getLength() > 0s)
        {
            const float x1 = vs.secToX (lr.getStart().inSeconds()), x2 = vs.secToX (lr.getEnd().inSeconds());
            g.setColour (red.withAlpha (tr.looping ? 0.22f : 0.08f)); g.fillRect (x1, H - 9, x2 - x1, 8.0f);
            g.setColour (tr.looping ? red : dim2); g.fillRect (x1, H - 9, 2.0f, 8.0f); g.fillRect (x2 - 2, H - 9, 2.0f, 8.0f);
        }
        // kursor
        const float px = vs.secToX (tr.getPosition().inSeconds());
        Path p; p.addTriangle (px - 6, H - 9, px + 6, H - 9, px, H - 1);
        g.setColour (Colours::white); g.fillPath (p);
        g.setColour (line); g.drawHorizontalLine (getHeight() - 1, 0, W);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! app.edit) return;
        dragStart = snapTime (*app.edit, vs.xToSec (e.position.x), vs);
        if (! e.mods.isShiftDown() && e.y < getHeight() - 12) app.edit->getTransport().setPosition (dragStart);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! app.edit) return;
        // sudrash = loop zonasi
        auto t2 = snapTime (*app.edit, vs.xToSec (e.position.x), vs);
        auto a = juce::jmin (dragStart, t2), b = juce::jmax (dragStart, t2);
        if (b - a > 0.05s) { app.edit->getTransport().setLoopRange ({ a, b }); app.edit->getTransport().looping = true; }
        repaint();
    }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (! app.edit) return;
        // ikki marta bosish = marker (qo'shiq bo'limi) qo'shish
        auto& ts = app.edit->tempoSequence;
        auto t = snapTime (*app.edit, vs.xToSec (e.position.x), vs, true);
        const auto bar = ts.toBarsAndBeats (t).bars;
        auto s = ts.toTime (te::tempo::BarsAndBeats { bar, {} }), en = ts.toTime (te::tempo::BarsAndBeats { bar + 8, {} });
        if (auto m = app.edit->getMarkerManager().createMarker (-1, s, en - s, nullptr))
        {
            static const char* names[] = { "Intro", "Kuplet", "Naqarot", "Bridge", "Outro" };
            const int n = app.edit->getMarkerManager().getMarkers().size() - 1;
            m->setName (names[juce::jmin (4, n)]);
            m->setColour (trackColours[n % 8]);
        }
        repaint();
    }

private:
    AppEngine& app; ViewState& vs; te::TimePosition dragStart;
};

// klip ko'rinadigan qismining to'lqin fayli ichidagi vaqt oralig'i.
// Diqqat: temp/pitch o'zgargan (autoTempo, pitch, speed) kliplar uchun TE "proxy" fayl ijro etadi —
// u allaqachon klip vaqtida (0 = klip boshi, offset qo'llangan). Oddiy kliplarda — asl fayl (offset + speed).
inline std::pair<te::TimePosition, te::TimePosition> thumbRange (te::WaveAudioClip& c, double vis1, double vis2)
{
    const auto pos = c.getPosition();
    if (c.usesTimeStretchedProxy())
        return { te::TimePosition::fromSeconds (vis1 - pos.getStart().inSeconds()), te::TimePosition::fromSeconds (vis2 - pos.getStart().inSeconds()) };
    const double speed = c.getSpeedRatio();
    return { te::TimePosition::fromSeconds ((vis1 - pos.getStart().inSeconds() + pos.getOffset().inSeconds()) * speed),
             te::TimePosition::fromSeconds ((vis2 - pos.getStart().inSeconds() + pos.getOffset().inSeconds()) * speed) };
}
// klip vaqti (edit) -> asl fayl vaqti va aksincha (autoTempo'ni ham hisobga oladi)
inline double editToSourceTime (te::WaveAudioClip& c, double editSec)
{
    const auto pos = c.getPosition();
    if (c.getAutoTempo())
    {
        auto& ts = c.edit.tempoSequence;
        const double beats = ts.toBeats (te::TimePosition::fromSeconds (editSec)).inBeats() - ts.toBeats (pos.getStart()).inBeats() + c.getOffsetInBeats().inBeats();
        const double bps = c.getLoopInfo().getBeatsPerSecond (c.getAudioFile().getInfo());
        return bps > 0 ? beats / bps : 0.0;
    }
    return (editSec - pos.getStart().inSeconds() + pos.getOffset().inSeconds()) * c.getSpeedRatio();
}
inline double sourceToEditTime (te::WaveAudioClip& c, double srcSec)
{
    // monoton bog'lanish: ikki nuqta orasida chiziqli (doimiy temp)
    const double cs = c.getPosition().getStart().inSeconds(), ce = c.getPosition().getEnd().inSeconds();
    const double a = editToSourceTime (c, cs), b = editToSourceTime (c, ce);
    if (std::abs (b - a) < 1e-9) return cs;
    return cs + (srcSec - a) / (b - a) * (ce - cs);
}

// =============================================================================  Lanes (kliplar)
class Lanes : public juce::Component, public juce::FileDragAndDropTarget, public juce::DragAndDropTarget
{
public:
    std::function<void (te::MidiClip&)> onOpenPianoRoll;
    std::function<void()> onSelectionChanged;

    Lanes (AppEngine& a, ViewState& v) : app (a), vs (v) {}

    te::AudioTrack* trackAtY (int y) const
    {
        if (! app.edit) return nullptr;
        auto ts = te::getAudioTracks (*app.edit);
        const int i = (y + vs.scrollY) / vs.trackH;
        return juce::isPositiveAndBelow (i, ts.size()) ? ts[i] : nullptr;
    }
    int trackY (te::AudioTrack& t) const
    {
        return te::getAudioTracks (*app.edit).indexOf (&t) * vs.trackH - vs.scrollY;
    }
    juce::Rectangle<float> clipRect (te::Clip& c) const
    {
        auto* t = dynamic_cast<te::AudioTrack*> (c.getTrack());
        if (t == nullptr) return {};
        const auto r = c.getEditTimeRange();
        const float x1 = vs.secToX (r.getStart().inSeconds()), x2 = vs.secToX (r.getEnd().inSeconds());
        return { x1 + 1, (float) trackY (*t) + 5, juce::jmax (4.0f, x2 - x1 - 2), (float) vs.trackH - 10 };
    }
    te::Clip* clipAt (juce::Point<float> p) const
    {
        if (auto* t = trackAtY ((int) p.y))
            for (auto* c : t->getClips())
                if (clipRect (*c).contains (p)) return c;
        return nullptr;
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        g.fillAll (bg0);
        if (! app.edit) return;
        auto& e = *app.edit; auto& ts = e.tempoSequence;
        const float W = (float) getWidth(), H = (float) getHeight();
        // grid
        const double beatPx = vs.pxPerSec * 60.0 / ts.getTempo (0)->getBpm();
        const int firstBar = jmax (0, ts.toBarsAndBeats (te::TimePosition::fromSeconds (jmax (0.0, vs.startSec))).bars);
        for (int bar = firstBar; ; ++bar)
        {
            const float x = vs.secToX (ts.toTime (te::tempo::BarsAndBeats { bar, {} }).inSeconds());
            if (x > W) break;
            g.setColour (Colours::white.withAlpha (0.06f)); g.fillRect (x, 0.0f, 1.0f, H);
            // grid chiziqlari (tanlangan bo'linma bo'yicha)
            const double gb = vs.gridBeats (e);
            const auto* sg = ts.getTimeSig (0);
            const double barBeats = sg ? sg->numerator * 4.0 / sg->denominator : 4.0;
            if (beatPx * gb >= 7.0 && gb < barBeats)
            {
                const double b0 = ts.toBeats (ts.toTime (te::tempo::BarsAndBeats { bar, {} })).inBeats();
                for (double k = gb; k < barBeats - 1e-6; k += gb)
                {
                    const float bx = vs.secToX (ts.toTime (te::BeatPosition::fromBeats (b0 + k)).inSeconds());
                    const bool beat = std::abs (k - std::round (k)) < 1e-6;
                    g.setColour (Colours::white.withAlpha (beat ? 0.035f : 0.018f)); g.fillRect (bx, 0.0f, 1.0f, H);
                }
            }
        }
        // loop zonasi
        auto& tr = e.getTransport();
        if (tr.looping && tr.getLoopRange().getLength() > 0s)
        {
            const float x1 = vs.secToX (tr.getLoopRange().getStart().inSeconds()), x2 = vs.secToX (tr.getLoopRange().getEnd().inSeconds());
            g.setColour (red.withAlpha (0.05f)); g.fillRect (x1, 0.0f, x2 - x1, H);
        }
        // treklar
        auto tracks = te::getAudioTracks (e);
        for (int i = 0; i < tracks.size(); ++i)
        {
            auto* t = tracks[i];
            const float y = (float) (i * vs.trackH - vs.scrollY);
            if (y > H || y + vs.trackH < 0) continue;
            if (t->itemID == vs.selectedTrack) { g.setColour (red.withAlpha (0.035f)); g.fillRect (0.0f, y, W, (float) vs.trackH); }
            g.setColour (Colours::white.withAlpha (0.055f)); g.fillRect (0.0f, y + vs.trackH - 1, W, 1.0f);
            for (auto* c : t->getClips()) drawClip (g, *c, *t);
            if (e.getTransport().isRecording() && app.isArmed (*t)) drawRecording (g, *t, y);
        }
        // kursor
        const float px = vs.secToX (tr.getPosition().inSeconds());
        g.setColour (Colours::white.withAlpha (0.08f)); g.fillRect (px - 6, 0.0f, 6.0f, H);
        g.setColour (Colours::white); g.fillRect (px, 0.0f, 1.5f, H);
        if (dropHint) { g.setColour (red.withAlpha (0.6f)); g.drawRect (getLocalBounds(), 2); }
        if (marquee) { g.setColour (red.withAlpha (0.12f)); g.fillRect (*marquee); g.setColour (red.withAlpha (0.7f)); g.drawRect (*marquee, 1.0f); }
    }

    void drawClip (juce::Graphics& g, te::Clip& c, te::AudioTrack& t)
    {
        using namespace juce;
        auto r = clipRect (c);
        if (r.getRight() < 0 || r.getX() > (float) getWidth()) return;
        const bool sel = app.selection.isSelected (&c);
        auto col = t.getColour();
        const bool muted = c.isMuted() || t.isMuted (true);
        g.setColour (col.withAlpha (muted ? 0.12f : 0.22f)); g.fillRoundedRectangle (r, 6);
        g.setColour (col.withAlpha (sel ? 0.95f : 0.5f)); g.drawRoundedRectangle (r, 6, sel ? 1.6f : 1.0f);
        auto head = r.withHeight (15);
        { Graphics::ScopedSaveState s (g); g.reduceClipRegion (head.toNearestInt()); g.setColour (col.withAlpha (0.38f)); g.fillRoundedRectangle (r, 6); }
        label (g, c.getName(), head.reduced (6, 0), 9.5f, Colours::white, Justification::centredLeft, 600);
        if (auto* wc0 = dynamic_cast<te::WaveAudioClip*> (&c))
        {   // fayl sample rate'i DAW'nikidan farq qilsa: sariq belgi
            te::AudioFile af (app.engine, wc0->getSourceFileReference().getFile());
            const double dr = app.engine.getDeviceManager().getSampleRate();
            if (AppEngine::isAltered (*wc0) && r.getWidth() > 130)
            {   // tempo / ton o'zgartirilgan: ko'k belgi (o'ng tugma -> Asl holiga qaytarish)
                String t = "SYNC";
                if (std::abs (wc0->getPitchChange()) > 0.001f) t << " " << (wc0->getPitchChange() > 0 ? "+" : "") << roundToInt (wc0->getPitchChange());
                auto bd = Rectangle<float> (head.getRight() - 38 - 54, head.getY() + 2, 50, 11);
                g.setColour (Colour (0xff4ad6ff).withAlpha (0.9f)); g.fillRoundedRectangle (bd, 3);
                label (g, t, bd, 7.5f, Colours::black, Justification::centred, 800);
            }
            if (af.isValid() && dr > 0 && std::abs (af.getSampleRate() - dr) > 0.5 && r.getWidth() > 90)
            {
                const String t = String (af.getSampleRate() / 1000.0, 1).replace (".0", "") + "k";
                auto bd = Rectangle<float> (head.getRight() - 34, head.getY() + 2, 30, 11);
                g.setColour (yellow.withAlpha (0.9f)); g.fillRoundedRectangle (bd, 3);
                label (g, t, bd, 8, Colours::black, Justification::centred, 800);
            }
        }
        auto body = r.withTrimmedTop (16).reduced (2, 2);

        if (auto* wc = dynamic_cast<te::WaveAudioClip*> (&c))
        {
            auto& th = thumbFor (*wc);
            if (th.isGeneratingProxy() || th.isOutOfDate()) { label (g, U ("To'lqin tayyorlanmoqda\xe2\x80\xa6"), body, 9, dim); return; }
            Graphics::ScopedSaveState s (g); g.reduceClipRegion (body.toNearestInt());
            const auto pos = wc->getPosition();
            const double vis1 = jmax (vs.startSec, pos.getStart().inSeconds()), vis2 = jmin (vs.xToSec ((float) getWidth()), pos.getEnd().inSeconds());
            if (vis2 <= vis1) return;
            const auto [t1, t2] = thumbRange (*wc, vis1, vis2);
            auto area = Rectangle<float> (vs.secToX (vis1), body.getY(), vs.secToX (vis2) - vs.secToX (vis1), body.getHeight()).toNearestInt();
            g.setColour (Colours::white.withAlpha (muted ? 0.25f : 0.85f));
            const float gain = wc->getGain();
            if (th.getNumChannels() > 1) { th.drawChannel (g, area.removeFromTop (area.getHeight() / 2), { t1, t2 }, 0, gain); th.drawChannel (g, area, { t1, t2 }, 1, gain); }
            else th.drawChannel (g, area, { t1, t2 }, 0, gain);
        }
        else if (auto* mc = dynamic_cast<te::MidiClip*> (&c))
        {
            auto& seq = mc->getSequence();
            int lo = 127, hi = 0;
            for (auto* n : seq.getNotes()) { lo = jmin (lo, n->getNoteNumber()); hi = jmax (hi, n->getNoteNumber()); }
            if (hi < lo) { label (g, U ("Bo'sh \xc2\xb7 2x bosing"), body, 9, dim); return; }
            lo -= 2; hi += 2;
            Graphics::ScopedSaveState s (g); g.reduceClipRegion (body.toNearestInt());
            auto& ts = app.edit->tempoSequence;
            for (auto* n : seq.getNotes())
            {
                const auto sb = mc->getStartBeat() + toDuration (n->getStartBeat()) - mc->getOffsetInBeats(); const auto eb = sb + n->getLengthBeats();
                const float x1 = vs.secToX (ts.toTime (sb).inSeconds()), x2 = vs.secToX (ts.toTime (eb).inSeconds());
                const float y = body.getBottom() - (float) (n->getNoteNumber() - lo) / (float) (hi - lo) * body.getHeight();
                g.setColour (Colours::white.withAlpha (0.5f + 0.5f * (float) n->getVelocity() / 127.0f));
                g.fillRoundedRectangle (x1, y - 2, jmax (2.0f, x2 - x1 - 1), 4, 1.5f);
            }
        }
    }

    void drawRecording (juce::Graphics& g, te::AudioTrack&, float y)
    {
        auto& tr = app.edit->getTransport();
        const double start = recStart.value_or (tr.getPosition().inSeconds());
        if (! recStart) recStart = start;
        const float x1 = vs.secToX (start), x2 = vs.secToX (tr.getPosition().inSeconds());
        auto r = juce::Rectangle<float> (x1, y + 5, juce::jmax (2.0f, x2 - x1), (float) vs.trackH - 10);
        g.setColour (red.withAlpha (0.25f)); g.fillRoundedRectangle (r, 6);
        g.setColour (red); g.drawRoundedRectangle (r, 6, 1.2f);
        label (g, U ("\xe2\x97\x8f Yozilmoqda"), r.withHeight (15).reduced (6, 0), 9.5f, juce::Colours::white, juce::Justification::centredLeft, 600);
    }
    std::optional<double> recStart;

    // ---------------------------------------------------------------- sichqoncha
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! app.edit) return;
        grabKeyboardFocus();
        auto* t = trackAtY (e.y);
        if (t) { vs.selectedTrack = t->itemID; app.routeMidiTo (t); }
        drag = {};
        if (auto* c = clipAt (e.position); c != nullptr && vs.tool != 0 && ! e.mods.isPopupMenu())
        {
            te::Clip::Ptr cp (c);
            if (vs.tool == 1)
            {
                app.edit->getUndoManager().beginNewTransaction (U ("Bo'lish"));
                if (auto* ct = dynamic_cast<te::ClipTrack*> (c->getTrack())) ct->splitClip (*c, cutPosition (*c, e.position.x).first);
            }
            if (vs.tool == 2) { app.edit->getUndoManager().beginNewTransaction (U ("O'chirish")); app.selection.deselectAll(); c->removeFromParent(); }
            if (vs.tool == 3) { app.edit->getUndoManager().beginNewTransaction ("Mute"); c->setMuted (! c->isMuted()); }
            repaint(); return;
        }
        marquee.reset();
        if (auto* c = clipAt (e.position))
        {
            if (e.mods.isShiftDown() || e.mods.isCommandDown()) app.selection.addToSelection (c);
            else if (! app.selection.isSelected (c)) app.selection.selectOnly (c);
            drag.clip = c; drag.startTime = c->getPosition().getStart(); drag.origTrack = dynamic_cast<te::AudioTrack*> (c->getTrack());
            const auto cr = clipRect (*c);
            drag.resizeEnd = e.position.x > cr.getRight() - 7;
            drag.resizeStart = ! drag.resizeEnd && e.position.x < cr.getX() + 7 && cr.getWidth() > 18;
            drag.origLen = c->getPosition().getLength();
            drag.origOffset = c->getPosition().getOffset();
            drag.copy = e.mods.isAltDown() && ! drag.resizeEnd && ! drag.resizeStart;
            drag.copied = false;
            drag.group.clear(); drag.groupStarts.clear();
            for (auto* sc : app.selection.getItemsOfType<te::Clip>()) { drag.group.add (sc); drag.groupStarts.add (sc->getPosition().getStart()); }
            app.edit->getUndoManager().beginNewTransaction (drag.copy ? U ("Nusxa") : (drag.resizeEnd || drag.resizeStart) ? U ("Uzunlik") : U ("Ko'chirish"));
            if (e.mods.isPopupMenu()) showClipMenu (*c);
        }
        else
        {
            if (! (e.mods.isShiftDown() || e.mods.isCommandDown())) app.selection.deselectAll();
            if (! e.mods.isPopupMenu()) marquee = juce::Rectangle<float> (e.position, e.position);
        }
        if (onSelectionChanged) onSelectionChanged();
        vs.changed(); repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! app.edit) return;
        if (marquee)   // to'rtburchak bilan belgilash
        {
            marquee = juce::Rectangle<float> (e.mouseDownPosition, e.position);
            app.selection.deselectAll();
            for (auto* t : te::getAudioTracks (*app.edit))
                for (auto* c : t->getClips())
                    if (clipRect (*c).intersects (*marquee)) app.selection.addToSelection (c);
            repaint(); return;
        }
        if (drag.clip == nullptr) return;
        if (e.getDistanceFromDragStart() < 3 && ! drag.copied) return;
        // Alt + sudrash = nusxa: asl kliplar joyida qoladi, nusxalari sudraladi
        if (drag.copy && ! drag.copied)
        {
            drag.copied = true;
            juce::Array<te::Clip*> copies;
            for (auto* oc : drag.group) if (auto nc = cloneInPlace (*oc)) copies.add (nc.get());
            const int mainIdx = drag.group.indexOf (drag.clip.get());
            drag.group = copies;
            if (juce::isPositiveAndBelow (mainIdx, copies.size())) drag.clip = copies[mainIdx];
            app.selection.deselectAll();
            for (auto* c2 : copies) app.selection.addToSelection (c2);
        }
        auto c = drag.clip;
        const double dt = (double) e.getDistanceFromDragStartX() / vs.pxPerSec;
        if (drag.resizeEnd)
        {
            const auto end = snapTime (*app.edit, (drag.startTime + drag.origLen).inSeconds() + dt, vs, false, c.get());
            if (end > drag.startTime + 0.05s) c->setEnd (end, true);
        }
        else if (drag.resizeStart)
        {
            // chap chetdan qirqish: kontent joyida qoladi (offset o'zgaradi), oxiri qimirlamaydi
            auto ns = snapTime (*app.edit, drag.startTime.inSeconds() + dt, vs, false, c.get());
            const auto endT = drag.startTime + drag.origLen;
            auto minStart = te::TimePosition();
            if (dynamic_cast<te::WaveAudioClip*> (c.get()) != nullptr)
                minStart = te::TimePosition::fromSeconds (juce::jmax (0.0, drag.startTime.inSeconds() - drag.origOffset.inSeconds() / juce::jmax (0.01, c->getSpeedRatio())));
            ns = juce::jlimit (minStart, endT - 0.05s, ns);
            c->setStart (ns, true, false);
            c->setEnd (endT, false);
        }
        else if (drag.group.size() > 1)
        {
            // bir nechta klip birga: yetakchi klip snap qilinadi, qolganlari bir xil siljiydi
            const auto lead = snapMove (*app.edit, drag.startTime, dt, vs, drag.clip.get());
            const auto shift = lead - drag.startTime;
            auto earliest = drag.groupStarts[0]; for (auto s : drag.groupStarts) earliest = juce::jmin (earliest, s);
            const auto safeShift = earliest + shift < te::TimePosition() ? te::TimePosition() - earliest : shift;
            for (int i = 0; i < drag.group.size(); ++i) drag.group[i]->setStart (drag.groupStarts[i] + safeShift, false, true);
        }
        else
        {
            c->setStart (snapMove (*app.edit, drag.startTime, dt, vs, c.get()), false, true);
            if (auto* nt = trackAtY (e.y))
                if (nt != c->getTrack()) c->moveTo (*nt);
        }
        repaint();
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (marquee && app.edit)
        {
            // sudralmagan bo'lsa — oddiy bosish: kursorni qo'yish
            if (! e.mouseWasDraggedSinceMouseDown()) app.edit->getTransport().setPosition (snapTime (*app.edit, vs.xToSec (e.position.x), vs));
            marquee.reset(); repaint();
            if (onSelectionChanged) onSelectionChanged();
        }
        // Alt + bosish (sudramasdan) = qaychi: shu joyda bo'lish (Cubase)
        if (drag.clip != nullptr && drag.copy && ! drag.copied && ! e.mouseWasDraggedSinceMouseDown() && app.edit)
        {
            app.edit->getUndoManager().undoCurrentTransactionOnly();
            app.edit->getUndoManager().beginNewTransaction (U ("Bo'lish"));
            const auto at = cutPosition (*drag.clip, e.mouseDownPosition.x).first;
            if (auto* ct = dynamic_cast<te::ClipTrack*> (drag.clip->getTrack()))
                if (at > drag.clip->getPosition().getStart() && at < drag.clip->getPosition().getEnd()) ct->splitClip (*drag.clip, at);
            repaint();
        }
        drag = {};
        updateCursor (e.position, e.mods);
    }

    // ---------------------------------------------------------------- qaychi: zarba boshiga magnit
    std::map<juce::String, std::vector<double>> onsetCache; std::set<juce::String> onsetPending;
    const std::vector<double>* onsetsFor (te::WaveAudioClip& c)
    {
        const auto f = c.getAudioFile().getFile(); const auto key = f.getFullPathName();
        if (auto it = onsetCache.find (key); it != onsetCache.end()) return &it->second;
        if (onsetPending.count (key) == 0 && f.existsAsFile())
        {
            onsetPending.insert (key);
            juce::Component::SafePointer<Lanes> sp (this);
            auto* eng = &app.engine;
            juce::Thread::launch ([sp, eng, f, key]
            {
                auto v = detectTransients (*eng, f);
                juce::MessageManager::callAsync ([sp, key, v = std::move (v)]() mutable { if (sp) { sp->onsetCache[key] = std::move (v); sp->onsetPending.erase (key); sp->repaint(); } });
            });
        }
        return nullptr;
    }
    // kesish nuqtasi: magnit yoqilgan bo'lsa yaqin zarba boshiga, aks holda gridga
    std::pair<te::TimePosition, bool> cutPosition (te::Clip& c, float x)
    {
        const double t = vs.xToSec (x);
        if (vs.transientMagnet)
            if (auto* wc = dynamic_cast<te::WaveAudioClip*> (&c))
                if (auto* ons = onsetsFor (*wc))
                {
                    const double cs = c.getPosition().getStart().inSeconds();
                    const double ft = editToSourceTime (*wc, t);
                    const double tol = std::abs (editToSourceTime (*wc, t + 16.0 / vs.pxPerSec) - ft);
                    double best = -1, bd = tol;
                    for (double o : *ons) if (std::abs (o - ft) < bd) { bd = std::abs (o - ft); best = o; }
                    if (best >= 0)
                    {
                        const double et = sourceToEditTime (*wc, best);
                        if (et > cs && et < c.getPosition().getEnd().inSeconds()) return { te::TimePosition::fromSeconds (et), true };
                    }
                }
        return { snapTime (*app.edit, t, vs, false, &c), false };
    }
    struct CutHover { te::Clip::Ptr clip; float x = 0; bool magnet = false; };
    std::optional<CutHover> cutHover;
    void updateCutHover (te::Clip* c, float x)
    {
        if (c == nullptr || ! app.edit) { if (cutHover) { cutHover.reset(); repaint(); } return; }
        auto [tp, mag] = cutPosition (*c, x);
        cutHover = CutHover { c, vs.secToX (tp.inSeconds()), mag };
        repaint();
    }
    void paintOverChildren (juce::Graphics& g) override
    {
        if (! cutHover || cutHover->clip == nullptr) return;
        auto r = clipRect (*cutHover->clip);
        const float x = cutHover->x;
        const auto col = cutHover->magnet ? juce::Colour (0xff4ad6ff) : juce::Colours::white;
        g.setColour (col.withAlpha (0.9f)); g.fillRect (x - 0.75f, r.getY() - 2, 1.5f, r.getHeight() + 4);
        if (cutHover->magnet)
        {
            // magnit belgisi
            juce::Path m; const float mx = x - 6, my = r.getY() - 1;
            m.startNewSubPath (mx, my); m.lineTo (mx, my + 6); m.quadraticTo (mx, my + 11, mx + 6, my + 11); m.quadraticTo (mx + 12, my + 11, mx + 12, my + 6); m.lineTo (mx + 12, my);
            g.setColour (juce::Colour (0xff09080b).withAlpha (0.85f)); g.fillRoundedRectangle (mx - 3, my - 2, 18, 15, 4);
            g.setColour (col); g.strokePath (m, juce::PathStrokeType (2.4f));
            g.setColour (red); g.fillRect (mx - 1.2f, my, 2.4f, 2.2f); g.fillRect (mx + 10.8f, my, 2.4f, 2.2f);
        }
    }
    void mouseExit (const juce::MouseEvent&) override { if (cutHover) { cutHover.reset(); repaint(); } }

    te::Clip::Ptr cloneInPlace (te::Clip& c)
    {
        auto* t = dynamic_cast<te::ClipTrack*> (c.getTrack());
        if (t == nullptr) return {};
        auto st = c.state.createCopy();
        te::EditItemID::remapIDs (st, nullptr, *app.edit);
        return t->insertClipWithState (st);
    }
    std::optional<juce::Rectangle<float>> marquee;
    juce::MouseCursor scissors = makeScissorsCursor();

    void modifierKeysChanged (const juce::ModifierKeys& m) override
    {
        if (! isMouseOverOrDragging()) return;
        updateCursor (getMouseXYRelative().toFloat(), m);
    }
    void updateCursor (juce::Point<float> pos, const juce::ModifierKeys& m)
    {
        auto* c = clipAt (pos);
        const bool cutMode = vs.tool == 1 || (vs.tool == 0 && m.isAltDown() && ! m.isAnyMouseButtonDown());
        updateCutHover (cutMode ? c : nullptr, pos.x);
        if (cutMode) { setMouseCursor (scissors); return; }
        if (vs.tool != 0) { setMouseCursor (c ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor); return; }
        const bool edge = c && (pos.x > clipRect (*c).getRight() - 7 || (pos.x < clipRect (*c).getX() + 7 && clipRect (*c).getWidth() > 18));
        setMouseCursor (edge ? juce::MouseCursor::LeftRightResizeCursor : c ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
    }
    void mouseMove (const juce::MouseEvent& e) override
    {
        updateCursor (e.position, e.mods);
        return;
        auto* c = clipAt (e.position);
        if (vs.tool != 0) { setMouseCursor (c ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor); return; }
        setMouseCursor (c && e.position.x > clipRect (*c).getRight() - 6 ? juce::MouseCursor::LeftRightResizeCursor
                                                                         : c ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (! app.edit) return;
        if (auto* c = clipAt (e.position))
        {
            if (auto* mc = dynamic_cast<te::MidiClip*> (c)) if (onOpenPianoRoll) onOpenPianoRoll (*mc);
            if (auto* wc = dynamic_cast<te::WaveAudioClip*> (c)) if (onOpenAudioEditor) onOpenAudioEditor (*wc);
            return;
        }
        if (auto* t = trackAtY (e.y))
        {
            // bo'sh joyga 2x bosish: instrument trekda 1 taktli MIDI klip
            auto& ts = app.edit->tempoSequence;
            const auto bar = ts.toBarsAndBeats (te::TimePosition::fromSeconds (jmaxD (vs.xToSec (e.position.x)))).bars;
            auto s = ts.toTime (te::tempo::BarsAndBeats { bar, {} }), en = ts.toTime (te::tempo::BarsAndBeats { bar + 1, {} });
            app.edit->getUndoManager().beginNewTransaction ("MIDI klip");
            if (auto mc = t->insertMIDIClip ("MIDI", { s, en }, nullptr))
            {
                app.selection.selectOnly (mc.get());
                if (onOpenPianoRoll) onOpenPianoRoll (*mc);
            }
            repaint();
        }
    }
    static double jmaxD (double v) { return juce::jmax (0.0, v); }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (e.mods.isCommandDown() || e.mods.isAltDown())
        {
            const double anchor = vs.xToSec (e.position.x);
            vs.pxPerSec = juce::jlimit (2.0, 2000.0, vs.pxPerSec * (w.deltaY > 0 ? 1.18 : 1.0 / 1.18));
            vs.startSec = juce::jmax (0.0, anchor - (double) e.position.x / vs.pxPerSec);
        }
        else if (e.mods.isShiftDown() || std::abs (w.deltaX) > std::abs (w.deltaY))
            vs.startSec = juce::jmax (0.0, vs.startSec - (double) (w.deltaX != 0 ? w.deltaX : w.deltaY) * 300.0 / vs.pxPerSec);
        else
        {
            const int maxY = juce::jmax (0, te::getAudioTracks (*app.edit).size() * vs.trackH - getHeight() + 40);
            vs.scrollY = juce::jlimit (0, maxY, vs.scrollY - (int) (w.deltaY * 200));
        }
        vs.changed();
    }

    std::function<void()> onDop, onRenderSel, onBounce, onQuantize;
    std::function<void (te::WaveAudioClip&, double)> onConvertRate;
    std::function<void (te::WaveAudioClip&)> onRestoreOriginal;   // sample rate konvertatsiyasi (0 = DAW formatiga)
    std::function<void (te::WaveAudioClip&)> onOpenAudioEditor;
    void showClipMenu (te::Clip& c)
    {
        juce::PopupMenu m;
        m.addSectionHeader (c.getName());
        const bool audio = dynamic_cast<te::WaveAudioClip*> (&c) != nullptr;
        if (audio) { juce::PopupMenu::Item it ("Direct Offline Processing"); it.itemID = 10; it.shortcutKeyDescription = "F7"; m.addItem (std::move (it)); }
        { juce::PopupMenu::Item it ("Bounce Selection"); it.itemID = 11; it.shortcutKeyDescription = "B"; m.addItem (std::move (it)); }
        m.addItem (12, "Render Selection...");
        { juce::PopupMenu::Item it ("Quantize"); it.itemID = 13; it.shortcutKeyDescription = "Q"; m.addItem (std::move (it)); }
        m.addSeparator();
        m.addItem (1, U ("Kursor joyida bo'lish  (Ctrl+E)"));
        m.addItem (2, c.isMuted() ? U ("Ovozini yoqish") : U ("Ovozini o'chirish (mute)"));
        m.addItem (3, U ("Nusxa (Ctrl+D)"));
        if (dynamic_cast<te::MidiClip*> (&c)) m.addItem (5, U ("Piano roll'da ochish"));
        if (auto* wc = dynamic_cast<te::WaveAudioClip*> (&c))
        {
            // Formatni moslashtirish (SRC): fayl sample rate'i DAW'nikiga mos kelmasa
            te::AudioFile af (app.engine, wc->getSourceFileReference().getFile());
            const double fr = af.isValid() ? af.getSampleRate() : 0.0, dr = app.engine.getDeviceManager().getSampleRate() > 0 ? app.engine.getDeviceManager().getSampleRate() : 48000.0;
            juce::PopupMenu sm;
            sm.addSectionHeader (U ("Fayl: ") + juce::String (fr, 0) + " Hz" + (af.isValid() ? juce::String::fromUTF8 (" \xc2\xb7 ") + juce::String (af.getBitsPerSample()) + "-bit" : juce::String()) + U ("  \xc2\xb7  DAW: ") + juce::String (dr, 0) + " Hz");
            { juce::PopupMenu::Item it (U ("DAW formatiga moslashtirish (") + juce::String (dr, 0) + " Hz)"); it.itemID = 30; it.isEnabled = std::abs (fr - dr) > 0.5; it.colour = std::abs (fr - dr) > 0.5 ? yellow : juce::Colour(); sm.addItem (std::move (it)); }
            sm.addSeparator();
            const double rates[] = { 44100, 48000, 88200, 96000 };
            const char* uses[] = { "CD Audio, musiqa", "Video, kino, professional audio", "Hi-Resolution Audio", "Professional recording va sound design" };
            for (int i = 0; i < 4; ++i)
            { juce::PopupMenu::Item it (juce::String (rates[i], 0) + juce::String::fromUTF8 (" Hz   \xe2\x80\x94  ") + U (uses[i])); it.itemID = 31 + i; it.isTicked = std::abs (fr - rates[i]) < 0.5; sm.addItem (std::move (it)); }
            m.addSeparator();
            { juce::PopupMenu::Item it (U ("Asl holiga qaytarish (tempo, ton, tezlik)")); it.itemID = 35; it.shortcutKeyDescription = "Ctrl+Shift+O";
              it.isEnabled = AppEngine::isAltered (*wc); if (it.isEnabled) it.colour = juce::Colour (0xff4ad6ff); m.addItem (std::move (it)); }
            const bool mism = af.isValid() && std::abs (fr - dr) > 0.5;
            m.addSubMenu (U ("Formatni moslashtirish (sample rate)") + (mism ? U ("  (mos emas)") : juce::String()), sm);
        }
        m.addSeparator();
        m.addItem (4, U ("O'chirish  (Del)"));
        juce::Component::SafePointer<Lanes> sp (this);
        te::Clip::Ptr cp (&c);
        m.showMenuAsync ({}, [sp, cp] (int r)
        {
            if (sp == nullptr || r == 0) return;
            auto& a = sp->app;
            if (r == 1) { if (auto* t = dynamic_cast<te::ClipTrack*> (cp->getTrack())) t->splitClip (*cp, a.edit->getTransport().getPosition()); }
            if (r == 2) cp->setMuted (! cp->isMuted());
            if (r == 3) sp->duplicate (*cp);
            if (r == 4) cp->removeFromParent();
            if (r == 5) if (auto* mc = dynamic_cast<te::MidiClip*> (cp.get())) if (sp->onOpenPianoRoll) sp->onOpenPianoRoll (*mc);
            if (r == 10 && sp->onDop) sp->onDop();
            if (r == 11 && sp->onBounce) sp->onBounce();
            if (r == 12 && sp->onRenderSel) sp->onRenderSel();
            if (r == 13 && sp->onQuantize) sp->onQuantize();
            if (r == 35 && sp->onRestoreOriginal) if (auto* wc = dynamic_cast<te::WaveAudioClip*> (cp.get())) sp->onRestoreOriginal (*wc);
            if (r >= 30 && r <= 34 && sp->onConvertRate)
                if (auto* wc = dynamic_cast<te::WaveAudioClip*> (cp.get())) { const double rates[] = { 0, 44100, 48000, 88200, 96000 }; sp->onConvertRate (*wc, rates[r - 30]); }
            if (sp) sp->repaint();
        });
    }

    void duplicate (te::Clip& c)
    {
        auto* t = dynamic_cast<te::ClipTrack*> (c.getTrack());
        if (t == nullptr) return;
        app.edit->getUndoManager().beginNewTransaction ("Nusxa");
        auto st = c.state.createCopy();
        te::EditItemID::remapIDs (st, nullptr, *app.edit);
        const auto len = c.getPosition().getLength();
        if (auto nc = t->insertClipWithState (st))
            nc->setStart (c.getPosition().getEnd(), false, true), juce::ignoreUnused (len);
    }

    // ---------------------------------------------------------------- fayl tashlash
    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void fileDragEnter (const juce::StringArray&, int, int) override { dropHint = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dropHint = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int x, int y) override
    {
        dropHint = false;
        for (auto& f : files) importAt (juce::File (f), x, y);
        repaint();
    }
    std::function<void (te::WaveAudioClip&, const juce::File&)> onAudioImported;
    static juce::File dragFile (const SourceDetails& d)
    {
        const auto s = d.description.toString();
        if (s.startsWith ("file:")) return juce::File (s.fromFirstOccurrenceOf ("file:", false, false));
        if (auto* ft = dynamic_cast<juce::FileTreeComponent*> (d.sourceComponent.get())) return ft->getSelectedFile();
        return {};
    }
    bool isInterestedInDragSource (const SourceDetails& d) override { return dragFile (d).existsAsFile(); }
    void itemDragEnter (const SourceDetails&) override { dropHint = true; repaint(); }
    void itemDragExit (const SourceDetails&) override { dropHint = false; repaint(); }
    void itemDropped (const SourceDetails& d) override
    {
        dropHint = false;
        importAt (dragFile (d), d.localPosition.x, d.localPosition.y);
        repaint();
    }

    void importAt (const juce::File& f, int x, int y)
    {
        if (! app.edit) return;
        auto* t = trackAtY (y);
        const bool midi = f.hasFileExtension ("mid;midi");
        if (t == nullptr) t = midi ? app.addInstrumentTrack (f.getFileNameWithoutExtension()) : app.addAudioTrack (f.getFileNameWithoutExtension());
        if (t == nullptr) return;
        const auto at = snapTime (*app.edit, vs.xToSec ((float) x), vs);
        if (midi) app.importMidi (*t, f, at);
        else if (auto c = app.importAudio (*t, f, at)) { if (onAudioImported) onAudioImported (*c, f); }
        vs.selectedTrack = t->itemID; vs.changed();
    }

    AppEngine& app;
private:
    ViewState& vs;
    bool dropHint = false;
    struct Drag { te::Clip::Ptr clip; te::TimePosition startTime; te::TimeDuration origLen, origOffset; te::AudioTrack* origTrack = nullptr; bool resizeEnd = false, resizeStart = false, copy = false, copied = false;
                  juce::Array<te::Clip*> group; juce::Array<te::TimePosition> groupStarts; } drag;
    std::map<te::EditItemID, std::unique_ptr<te::SmartThumbnail>> thumbs; std::map<te::EditItemID, juce::File> thumbFiles;

    te::SmartThumbnail& thumbFor (te::WaveAudioClip& c)
    {
        auto& p = thumbs[c.itemID];
        const te::AudioFile f (c.getPlaybackFile());
        if (p == nullptr) p = std::make_unique<te::SmartThumbnail> (app.engine, f, *this, app.edit.get());
        else if (thumbFiles[c.itemID] != f.getFile()) p->setNewFile (f);
        thumbFiles[c.itemID] = f.getFile();
        return *p;
    }
};

// =============================================================================  Trek sarlavhalari
class TrackHeaders : public juce::Component
{
public:
    std::function<void()> onSelectionChanged;
    TrackHeaders (AppEngine& a, ViewState& v) : app (a), vs (v) {}

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        g.fillAll (bg1);
        if (! app.edit) return;
        auto tracks = te::getAudioTracks (*app.edit);
        for (int i = 0; i < tracks.size(); ++i)
        {
            auto* t = tracks[i];
            const float y = (float) (i * vs.trackH - vs.scrollY), W = (float) getWidth();
            if (y > getHeight() || y + vs.trackH < 0) continue;
            auto r = Rectangle<float> (0, y, W, (float) vs.trackH);
            if (t->itemID == vs.selectedTrack) { g.setColour (red.withAlpha (0.06f)); g.fillRect (r); }
            g.setColour (t->getColour()); g.fillRoundedRectangle (10, y + 9, 4, (float) vs.trackH - 18, 2);
            label (g, t->getName(), { 22, y + 16, W - 120, 18 }, 12.5f, text, Justification::centredLeft, 600);
            caps (g, AppEngine::isInstrumentTrack (*t) ? "INSTRUMENT" : "AUDIO", { 22, y + 36, 120, 12 }, 9.0f, dim2);
            // M S R tugmalari
            const bool st[3] = { t->isMuted (false), t->isSolo (false), app.isArmed (*t) };
            const Colour cs[3] = { yellow, green, red };
            const char* lb[3] = { "M", "S", "R" };
            for (int k = 0; k < 3; ++k)
            {
                auto b = buttonRect (k, y);
                g.setColour (st[k] ? cs[k] : Colours::white.withAlpha (0.03f)); g.fillRoundedRectangle (b, 5);
                g.setColour (st[k] ? cs[k] : Colours::white.withAlpha (0.1f)); g.drawRoundedRectangle (b, 5, 1);
                label (g, lb[k], b, 9.5f, st[k] ? (k == 2 ? Colours::white : Colours::black) : dim, Justification::centred, 700);
            }
            // mini metr
            const float lv = levels.count (t->itemID) ? levels[t->itemID] : 0.0f;
            auto m = Rectangle<float> (W - 9, y + 10, 3, (float) vs.trackH - 20);
            g.setColour (Colours::white.withAlpha (0.05f)); g.fillRoundedRectangle (m, 1.5f);
            auto f = m.withTop (m.getBottom() - m.getHeight() * lv);
            g.setGradientFill (ColourGradient (Colour (0xffff3347), 0, m.getY(), green, 0, m.getBottom(), false)); g.fillRoundedRectangle (f, 1.5f);
            g.setColour (Colours::white.withAlpha (0.055f)); g.fillRect (0.0f, y + vs.trackH - 1, W, 1.0f);
        }
        g.setColour (line); g.drawVerticalLine (getWidth() - 1, 0, (float) getHeight());
    }

    juce::Rectangle<float> buttonRect (int k, float y) const { return { (float) getWidth() - 90 + (float) k * 25, y + (float) vs.trackH * 0.5f - 9, 21, 18 }; }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! app.edit) return;
        auto tracks = te::getAudioTracks (*app.edit);
        const int i = (e.y + vs.scrollY) / vs.trackH;
        if (! juce::isPositiveAndBelow (i, tracks.size())) return;
        auto* t = tracks[i];
        const float y = (float) (i * vs.trackH - vs.scrollY);
        for (int k = 0; k < 3; ++k)
            if (buttonRect (k, y).contains (e.position))
            {
                if (k == 0) t->setMute (! t->isMuted (false));
                if (k == 1) t->setSolo (! t->isSolo (false));
                if (k == 2) app.setArmed (*t, ! app.isArmed (*t));
                repaint(); return;
            }
        vs.selectedTrack = t->itemID;
        app.routeMidiTo (t);
        if (e.mods.isPopupMenu()) showTrackMenu (*t);
        else { reorderTrack = t; reorderTarget = -1; }
        if (onSelectionChanged) onSelectionChanged();
        vs.changed();
    }
    // ---- treklarni sudrab tartibini almashtirish
    std::function<void()> onTracksReordered;
    te::Track::Ptr reorderTrack; int reorderTarget = -1;
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (reorderTrack == nullptr || ! app.edit || std::abs (e.getDistanceFromDragStartY()) < 6) return;
        const int n = te::getAudioTracks (*app.edit).size();
        reorderTarget = juce::jlimit (0, n, (int) std::floor ((e.y + vs.scrollY) / (double) vs.trackH + 0.5));
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        repaint();
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        setMouseCursor (juce::MouseCursor::NormalCursor);
        if (reorderTrack != nullptr && reorderTarget >= 0)
            if (auto* at = dynamic_cast<te::AudioTrack*> (reorderTrack.get()))
                if (app.moveTrackTo (*at, reorderTarget)) { if (onTracksReordered) onTracksReordered(); vs.changed(); }
        reorderTrack = nullptr; reorderTarget = -1; repaint();
    }
    void paintOverChildren (juce::Graphics& g) override
    {
        if (reorderTarget < 0) return;
        const float y = (float) (reorderTarget * vs.trackH - vs.scrollY);
        g.setColour (juce::Colour (0xffff9a4a)); g.fillRoundedRectangle (4, y - 1.5f, (float) getWidth() - 8, 3, 1.5f);
        g.fillEllipse (2, y - 4, 8, 8);
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (! app.edit) return;
        auto tracks = te::getAudioTracks (*app.edit);
        const int i = (e.y + vs.scrollY) / vs.trackH;
        if (juce::isPositiveAndBelow (i, tracks.size()))
        {
            if (auto* bp = beatOf (*tracks[i]); bp != nullptr && e.x > 40) { bp->showWindowExplicitly(); return; }
            rename (*tracks[i]);
        }
        else if (onAddTrack) onAddTrack();
    }
    std::function<void()> onAddTrack;
    static te::Plugin* beatOf (te::AudioTrack& t)
    {
        for (auto p : t.pluginList.getPlugins()) if (p->getPluginType() == "azbeat") return p;
        return nullptr;
    }

    void rename (te::AudioTrack& t)
    {
        auto* w = new juce::AlertWindow (U ("Trek nomi"), {}, juce::MessageBoxIconType::NoIcon);
        w->addTextEditor ("n", t.getName());
        w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey)); w->addButton (U ("Bekor"), 0);
        te::Track::Ptr tp (&t);
        w->enterModalState (true, juce::ModalCallbackFunction::create ([w, tp, this] (int r) { if (r) tp->setName (w->getTextEditorContents ("n")); repaint(); }), true);
    }

    void showTrackMenu (te::AudioTrack& t)
    {
        juce::PopupMenu m, col;
        m.addSectionHeader (t.getName());
        if (beatOf (t)) { m.addItem (3, U ("AZ BEAT'ni ochish (padlar + sekvenser)")); m.addSeparator(); }
        m.addItem (1, U ("Nomini o'zgartirish"));
        for (int i = 0; i < 8; ++i) col.addColouredItem (100 + i, U ("\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88"), trackColours[i]);
        m.addSubMenu (U ("Rang"), col);
        m.addSeparator(); m.addItem (2, U ("Trekni o'chirish"));
        te::Track::Ptr tp (&t);
        m.showMenuAsync ({}, [this, tp] (int r)
        {
            if (r == 1) rename (*dynamic_cast<te::AudioTrack*> (tp.get()));
            if (r == 3) if (auto* at = dynamic_cast<te::AudioTrack*> (tp.get())) if (auto* bp = beatOf (*at)) bp->showWindowExplicitly();
            if (r >= 100) tp->setColour (trackColours[r - 100]);
            if (r == 2) { app.edit->getUndoManager().beginNewTransaction ("Trek"); app.edit->deleteTrack (tp.get()); vs.changed(); }
            repaint();
        });
    }

    std::map<te::EditItemID, float> levels;
private:
    AppEngine& app; ViewState& vs;
};
}
