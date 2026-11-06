// =============================================================================
//  AZ MUS — aranjirovka: vaqt chizg'ichi, trek sarlavhalari, kliplar, kursor
// =============================================================================
#pragma once
#include "AppEngine.h"

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
        auto body = r.withTrimmedTop (16).reduced (2, 2);

        if (auto* wc = dynamic_cast<te::WaveAudioClip*> (&c))
        {
            auto& th = thumbFor (*wc);
            if (th.isGeneratingProxy() || th.isOutOfDate()) { label (g, U ("To'lqin tayyorlanmoqda\xe2\x80\xa6"), body, 9, dim); return; }
            Graphics::ScopedSaveState s (g); g.reduceClipRegion (body.toNearestInt());
            const auto pos = wc->getPosition();
            const double vis1 = jmax (vs.startSec, pos.getStart().inSeconds()), vis2 = jmin (vs.xToSec ((float) getWidth()), pos.getEnd().inSeconds());
            if (vis2 <= vis1) return;
            const double speed = wc->getSpeedRatio();
            const auto t1 = te::TimePosition::fromSeconds ((vis1 - pos.getStart().inSeconds() + pos.getOffset().inSeconds()) * speed);
            const auto t2 = te::TimePosition::fromSeconds ((vis2 - pos.getStart().inSeconds() + pos.getOffset().inSeconds()) * speed);
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
                if (auto* ct = dynamic_cast<te::ClipTrack*> (c->getTrack())) ct->splitClip (*c, snapTime (*app.edit, vs.xToSec (e.position.x), vs));
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
            drag.resizeEnd = e.position.x > clipRect (*c).getRight() - 6;
            drag.origLen = c->getPosition().getLength();
            drag.copy = e.mods.isAltDown() && ! drag.resizeEnd;
            drag.copied = false;
            drag.group.clear(); drag.groupStarts.clear();
            for (auto* sc : app.selection.getItemsOfType<te::Clip>()) { drag.group.add (sc); drag.groupStarts.add (sc->getPosition().getStart()); }
            app.edit->getUndoManager().beginNewTransaction (drag.copy ? U ("Nusxa") : drag.resizeEnd ? U ("Uzunlik") : U ("Ko'chirish"));
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
        drag = {};
    }

    te::Clip::Ptr cloneInPlace (te::Clip& c)
    {
        auto* t = dynamic_cast<te::ClipTrack*> (c.getTrack());
        if (t == nullptr) return {};
        auto st = c.state.createCopy();
        te::EditItemID::remapIDs (st, nullptr, *app.edit);
        return t->insertClipWithState (st);
    }
    std::optional<juce::Rectangle<float>> marquee;

    void mouseMove (const juce::MouseEvent& e) override
    {
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
    bool isInterestedInDragSource (const SourceDetails& d) override { return d.description.toString().startsWith ("file:"); }
    void itemDragEnter (const SourceDetails&) override { dropHint = true; repaint(); }
    void itemDragExit (const SourceDetails&) override { dropHint = false; repaint(); }
    void itemDropped (const SourceDetails& d) override
    {
        dropHint = false;
        importAt (juce::File (d.description.toString().fromFirstOccurrenceOf ("file:", false, false)), d.localPosition.x, d.localPosition.y);
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
        if (midi) app.importMidi (*t, f, at); else app.importAudio (*t, f, at);
        vs.selectedTrack = t->itemID; vs.changed();
    }

    AppEngine& app;
private:
    ViewState& vs;
    bool dropHint = false;
    struct Drag { te::Clip::Ptr clip; te::TimePosition startTime; te::TimeDuration origLen; te::AudioTrack* origTrack = nullptr; bool resizeEnd = false, copy = false, copied = false;
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
        if (onSelectionChanged) onSelectionChanged();
        vs.changed();
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (! app.edit) return;
        auto tracks = te::getAudioTracks (*app.edit);
        const int i = (e.y + vs.scrollY) / vs.trackH;
        if (juce::isPositiveAndBelow (i, tracks.size())) rename (*tracks[i]);
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
        m.addItem (1, U ("Nomini o'zgartirish"));
        for (int i = 0; i < 8; ++i) col.addColouredItem (100 + i, U ("\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88"), trackColours[i]);
        m.addSubMenu (U ("Rang"), col);
        m.addSeparator(); m.addItem (2, U ("Trekni o'chirish"));
        te::Track::Ptr tp (&t);
        m.showMenuAsync ({}, [this, tp] (int r)
        {
            if (r == 1) rename (*dynamic_cast<te::AudioTrack*> (tp.get()));
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
