// =============================================================================
//  AZ MUS — panellar: yuqori panel, inspektor, mikser, media brauzer, holat qatori
// =============================================================================
#pragma once
#include "Arrangement.h"
#include "PluginPicker.h"
#include "SampleSync.h"

namespace az
{
inline juce::TextButton* iconButton (juce::TextButton& b, const char* icon, const char* tip, juce::Colour accent = red)
{
    b.setButtonText (icon); b.setTooltip (U (tip));
    b.getProperties().set ("accent", (juce::int64) accent.getARGB());
    return &b;
}

// =============================================================================  LCD
class LcdDisplay : public juce::Component
{
public:
    explicit LcdDisplay (AppEngine& a) : app (a) {}
    std::function<juce::String()> positionText;     // tanlangan vaqt formatida
    std::function<void()> onFormatClick;
    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (Colour (0xff050507)); g.fillRoundedRectangle (r, 10);
        g.setColour (Colours::white.withAlpha (0.08f)); g.drawRoundedRectangle (r, 10, 1);
        if (! app.edit) return;
        caps (g, "POZITSIYA", { 14, 4, 120, 10 }, 8.5f);
        auto pos = positionText ? positionText() : app.positionString();
        float x = 14;
        const float fs = pos.length() > 12 ? 17.0f : 21.0f;
        g.setFont (font (fs, 700, 0.04f));
        for (int i = 0; i < pos.length(); ++i)
        {
            const auto ch = String::charToString (pos[i]);
            g.setColour (ch == "." || ch == ":" ? red : Colours::white);
            const float w = textWidth (font (fs, 700, 0.04f), ch) + 1.5f;
            g.drawText (ch, Rectangle<float> (x, 14, w + 4, 24), Justification::centredLeft); x += w;
        }
        posCell = { 0, 0, x + 6, (float) getHeight() };
        cells.clear();
        const String ks[] = { "VAQT", "TONALLIK" };
        const String vs[] = { app.timeString(), app.edit->state.getProperty ("azKey", "A minor").toString() };
        float cx = jmax (x + 18, 160.0f);
        for (int i = 0; i < 2; ++i)
        {
            const float w = i == 0 ? 82.0f : 86.0f;
            caps (g, ks[i], { cx, 4, w, 10 }, 8.5f);
            label (g, vs[i], { cx, 16, w + 10, 20 }, 13, text, Justification::centredLeft, 600);
            cells.add ({ cx, 0, w, (float) getHeight() });
            cx += w + 14;
        }
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! app.edit) return;
        if (posCell.contains (e.position) && onFormatClick) onFormatClick();
        if (cells.size() == 2 && cells[1].contains (e.position)) showKeyMenu();
    }
    void showKeyMenu()
    {
        juce::PopupMenu m; const char* notes[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
        juce::PopupMenu maj, min;
        for (int i = 0; i < 12; ++i) { maj.addItem (i + 1, juce::String (notes[i]) + " major"); min.addItem (i + 101, juce::String (notes[i]) + " minor"); }
        m.addSubMenu ("Major", maj); m.addSubMenu ("Minor", min);
        m.showMenuAsync ({}, [this, notes] (int r)
        { if (r) { app.edit->state.setProperty ("azKey", juce::String (notes[(r - 1) % 100]) + (r > 100 ? " minor" : " major"), &app.edit->getUndoManager()); repaint(); } });
    }
private:
    AppEngine& app; juce::Array<juce::Rectangle<float>> cells; juce::Rectangle<float> posCell;
};

// =============================================================================  Temp qutisi (Cubase transport kabi)
// ♩ 96.000 ▲▼ · 4/4 · Tap · ▾
class TempoBox : public juce::Component, private juce::Timer
{
public:
    explicit TempoBox (AppEngine& a) : app (a)
    {
        for (auto* b : { &upB, &downB, &tapB, &moreB }) { addAndMakeVisible (b); b->getProperties().set ("radius", 4.0f); b->setWantsKeyboardFocus (false); b->setMouseClickGrabsKeyboardFocus (false); }
        upB.setButtonText (juce::String::fromUTF8 ("\xe2\x96\xb2")); downB.setButtonText (juce::String::fromUTF8 ("\xe2\x96\xbc"));
        tapB.setButtonText ("Tap"); moreB.setButtonText (juce::String::fromUTF8 ("\xe2\x96\xbe"));
        upB.setTooltip (U ("Temp +1 (Shift: +0.01)")); downB.setTooltip (U ("Temp -1 (Shift: -0.01)"));
        tapB.setTooltip (U ("Tap tempo: ritmga bosing yoki Tap'dan keyin Probel bilan sanang. Ijro paytida temp to'xtatilganda (yoki Enter) qo'llanadi \xe2\x80\x94 ovoz uzilmaydi"));
        tapB.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffff9a4a).withAlpha (0.45f));
        upB.onClick = [this] { step (juce::ModifierKeys::currentModifiers.isShiftDown() ? 0.01 : 1.0); };
        downB.onClick = [this] { step (juce::ModifierKeys::currentModifiers.isShiftDown() ? -0.01 : -1.0); };
        upB.setRepeatSpeed (400, 70); downB.setRepeatSpeed (400, 70);
        tapB.onClick = [this] { tap(); };
        moreB.onClick = [this] { showMore(); };
        startTimerHz (8);
    }
    double bpm() const { return app.edit ? app.edit->tempoSequence.getTempo (0)->getBpm() : 120.0; }
    void setBpm (double b)
    {
        if (! app.edit) return;
        b = juce::jlimit (20.0, 400.0, std::round (b * 1000.0) / 1000.0);
        app.edit->tempoSequence.getTempo (0)->setBpm (b);
        repaint(); if (onChanged) onChanged();
    }
    std::function<void()> onChanged;
    void step (double d) { setBpm (bpm() + d); }
    // Tap tempo: ijro paytida temp DARHOL o'zgartirilmaydi (ovoz uzilmasligi uchun) —
    // o'lchangan qiymat ko'rsatiladi va to'xtatilganda yoki Enter bosilganda qo'llanadi.
    // Tap rejimi: Tap tugmasi bosilgach Probel (Space) ham "tap" bo'ladi; Esc yoki 4 s jimlik — chiqish.
    bool tapMode = false;
    double pendingBpm = 0;
    void tap()
    {
        const double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        if (! taps.isEmpty() && now - taps.getLast() > 2.0) taps.clear();
        taps.add (now); lastTap = now; tapMode = true;
        while (taps.size() > 8) taps.remove (0);
        if (taps.size() >= 2)
        {
            const double avg = (taps.getLast() - taps.getFirst()) / (taps.size() - 1);
            if (avg > 0.15) pendingBpm = std::round (60.0 / avg * 100.0) / 100.0;
            if (! isPlaying()) applyPending();
        }
        tapFlash = 3; tapB.setToggleState (true, juce::dontSendNotification); repaint();
    }
    bool isPlaying() const { return app.edit && app.edit->getTransport().isPlaying(); }
    void applyPending() { if (pendingBpm > 0) { setBpm (pendingBpm); pendingBpm = 0; } }
    void exitTapMode() { tapMode = false; taps.clear(); tapB.setToggleState (false, juce::dontSendNotification); repaint(); }
    void resized() override
    {
        auto r = getLocalBounds().reduced (4, 7);
        moreB.setBounds (r.removeFromRight (18)); r.removeFromRight (4);
        tapB.setBounds (r.removeFromRight (40)); r.removeFromRight (6);
        sigArea = r.removeFromRight (40).toFloat(); r.removeFromRight (6);
        auto ud = r.removeFromRight (16);
        upB.setBounds (ud.removeFromTop (ud.getHeight() / 2)); downB.setBounds (ud);
        bpmArea = r.toFloat();
    }
    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (Colour (0xff050507)); g.fillRoundedRectangle (r, 10);
        g.setColour (Colours::white.withAlpha (0.08f)); g.drawRoundedRectangle (r, 10, 1);
        caps (g, "TEMP", { bpmArea.getX() + 22, 3, 60, 10 }, 8.5f);
        label (g, String::fromUTF8 ("\xe2\x99\xa9"), bpmArea.withWidth (20).translated (2, 4), 15, tapFlash > 0 ? red : silver, Justification::centred, 600);
        if (pendingBpm > 0)
        {
            label (g, String (pendingBpm, 2), bpmArea.withTrimmedLeft (22).translated (0, 5), 15, Colour (0xffff9a4a), Justification::centredLeft, 700);
            label (g, U ("TAP \xc2\xb7 Enter = qo'llash"), { bpmArea.getX() + 50, 3, 120, 10 }, 7.5f, Colour (0xffff9a4a), Justification::centredLeft, 600);
        }
        else label (g, String (bpm(), 3), bpmArea.withTrimmedLeft (22).translated (0, 5), 15, tapMode ? Colour (0xffff9a4a) : Colours::white, Justification::centredLeft, 700);
        caps (g, U ("O'LCHOV"), { sigArea.getX(), 3, 60, 10 }, 8.5f);
        if (app.edit) if (auto* sg = app.edit->tempoSequence.getTimeSig (0))
            label (g, String (sg->numerator) + "/" + String (sg->denominator), sigArea.translated (0, 5), 14, text, Justification::centredLeft, 600);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        dragStart = bpm();
        if (sigArea.contains (e.position)) showSigMenu();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! bpmArea.contains (e.mouseDownPosition)) return;
        const double k = e.mods.isShiftDown() ? 0.01 : 0.2;
        setBpm (std::round ((dragStart - e.getDistanceFromDragStartY() * k) * 100.0) / 100.0);
    }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (bpmArea.contains (e.position)) step ((w.deltaY > 0 ? 1.0 : -1.0) * (e.mods.isShiftDown() ? 0.01 : 1.0));
    }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (! app.edit || ! bpmArea.contains (e.position)) return;
        if (pendingBpm > 0) { applyPending(); return; }
        auto* w = new juce::AlertWindow (U ("Temp (BPM)"), {}, juce::MessageBoxIconType::NoIcon);
        w->addTextEditor ("b", juce::String (bpm(), 3));
        w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey)); w->addButton (U ("Bekor"), 0);
        w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int r) { if (r) setBpm (w->getTextEditorContents ("b").getDoubleValue()); }), true);
    }
    void showSigMenu()
    {
        juce::PopupMenu m; static const int sigs[][2] = { { 4, 4 }, { 3, 4 }, { 2, 4 }, { 5, 4 }, { 6, 8 }, { 7, 8 }, { 9, 8 }, { 12, 8 }, { 6, 4 }, { 2, 2 } };
        for (int i = 0; i < 10; ++i) m.addItem (i + 1, juce::String (sigs[i][0]) + "/" + juce::String (sigs[i][1]));
        m.showMenuAsync ({}, [this] (int r) { if (r && app.edit) { auto s = app.edit->tempoSequence.getTimeSig (0); s->numerator = sigs[r - 1][0]; s->denominator = sigs[r - 1][1]; repaint(); if (onChanged) onChanged(); } });
    }
    void showMore()
    {
        juce::PopupMenu m;
        m.addItem (1, U ("Temp \xc3\x97 2 (double time)")); m.addItem (2, U ("Temp \xc3\xb7 2 (half time)"));
        m.addItem (3, U ("Butun songa yaxlitlash")); m.addSeparator();
        for (double b : { 70.0, 80.0, 90.0, 96.0, 100.0, 110.0, 120.0, 128.0, 140.0 }) m.addItem (100 + (int) b, juce::String ((int) b) + " BPM");
        m.showMenuAsync ({}, [this] (int r)
        {
            if (r == 1) setBpm (bpm() * 2); if (r == 2) setBpm (bpm() / 2); if (r == 3) setBpm (std::round (bpm()));
            if (r >= 100) setBpm (r - 100);
        });
    }
private:
    void timerCallback() override
    {
        if (tapFlash > 0 && --tapFlash == 0) repaint();
        if (pendingBpm > 0 && ! isPlaying()) applyPending();
        if (tapMode && juce::Time::getMillisecondCounterHiRes() / 1000.0 - lastTap > 4.0 && pendingBpm <= 0) exitTapMode();
    }
    AppEngine& app; juce::TextButton upB, downB, tapB, moreB;
    juce::Rectangle<float> bpmArea, sigArea; juce::Array<double> taps; double dragStart = 120, lastTap = 0; int tapFlash = 0;
};

// =============================================================================  Grid paneli (Logic X tuzilmasi + Cubase bo'linmalari)
// [# Grid: 1/8 ▾] [Snap: Grid ▾] [Q 1/8 ▾] [Q] [Bars+Beats ▾]
class GridBar : public juce::Component
{
public:
    std::function<void()> onQuantize, onChanged;
    GridBar (AppEngine& a, ViewState& v) : app (a), vs (v)
    {
        for (auto* b : { &gridB, &snapTypeB, &qB, &qApplyB, &fmtB, &magB }) { addAndMakeVisible (b); b->getProperties().set ("radius", 5.0f); }
        magB.setButtonText (U ("Magnit: zarba")); magB.setClickingTogglesState (true);
        magB.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff4ad6ff).withAlpha (0.35f));
        magB.setTooltip (U ("Qaychi (Alt yoki 2-asbob) audiodagi zarba boshiga yopishadi. O'chirsangiz \xe2\x80\x94 gridga"));
        magB.onClick = [this] { vs.transientMagnet = magB.getToggleState(); changed(); };
        gridB.setTooltip (U ("Grid: bo'linma (Smart \xc2\xb7 Bar \xc2\xb7 Beat \xc2\xb7 1/1 \xe2\x80\xa6 1/128, Triplet, Dotted)"));
        snapTypeB.setTooltip (U ("Snap turi")); qB.setTooltip (U ("Quantize qiymati")); qApplyB.setTooltip (U ("Tanlanganlarni quantize qilish (Q)"));
        fmtB.setTooltip (U ("Vaqt formati"));
        qApplyB.setButtonText ("Q");
        gridB.onClick = [this] { showGridMenu(); };
        snapTypeB.onClick = [this] { showSnapMenu(); };
        qB.onClick = [this] { showQMenu(); };
        qApplyB.onClick = [this] { if (onQuantize) onQuantize(); };
        fmtB.onClick = [this] { showFormatMenu(); };
        refresh();
    }
    void refresh()
    {
        gridB.setButtonText (juce::String::fromUTF8 ("# ") + vs.gridName() + juce::String::fromUTF8 ("  \xe2\x96\xbe"));
        const char* st[] = { "Grid", "Grid Relative", "Events", "Magnetic Cursor" };
        snapTypeB.setButtonText (juce::String (st[vs.snapType]) + juce::String::fromUTF8 ("  \xe2\x96\xbe"));
        qB.setButtonText (juce::String ("Q  ") + (vs.linkQ ? vs.gridName() : ViewState::divisionName (vs.quantize)) + juce::String::fromUTF8 ("  \xe2\x96\xbe"));
        const char* fm[] = { "Bars+Beats", "Seconds", "Timecode", "Samples" };
        fmtB.setButtonText (juce::String (fm[vs.timeFormat]) + juce::String::fromUTF8 ("  \xe2\x96\xbe"));
        magB.setToggleState (vs.transientMagnet, juce::dontSendNotification);
        repaint();
    }
    double quantizeBeats() const { return vs.linkQ ? vs.gridBeats (*app.edit) : vs.quantize; }

    static void addDivisions (juce::PopupMenu& m, int base, double current, bool active)
    {
        static const double ds[] = { 4, 2, 1, 0.5, 0.25, 0.125, 0.0625, 0.03125 };
        for (int i = 0; i < 8; ++i)  m.addItem (base + i, ViewState::divisionName (ds[i]), true, active && std::abs (current - ds[i]) < 1e-6);
        m.addSeparator();
        for (int i = 1; i < 7; ++i)  m.addItem (base + 10 + i, ViewState::divisionName (ds[i]).replace ("T", "") + "  Triplet", true, active && std::abs (current - ds[i] * 2 / 3) < 1e-6);
        m.addSeparator();
        for (int i = 1; i < 7; ++i)  m.addItem (base + 20 + i, ViewState::divisionName (ds[i]) + "  Dotted", true, active && std::abs (current - ds[i] * 1.5) < 1e-6);
    }
    static double divisionFromId (int id)
    {
        static const double ds[] = { 4, 2, 1, 0.5, 0.25, 0.125, 0.0625, 0.03125 };
        if (id < 10) return ds[id]; if (id < 20) return ds[id - 10] * 2.0 / 3.0; return ds[id - 20] * 1.5;
    }
    void showGridMenu()
    {
        juce::PopupMenu m;
        m.addSectionHeader (U ("Logic X: snap rejimi"));
        m.addItem (1, U ("Smart (zoomga qarab)"), true, vs.gridMode == ViewState::gridSmart);
        m.addItem (2, "Bar", true, vs.gridMode == ViewState::gridBar);
        m.addItem (3, "Beat", true, vs.gridMode == ViewState::gridBeat);
        m.addSectionHeader (U ("Bo'linma (Division)"));
        addDivisions (m, 100, vs.division, vs.gridMode == ViewState::gridDivision);
        m.addSeparator();
        m.addItem (5, U ("Snap yoqilgan (J)"), true, vs.snap);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&gridB), [this] (int r)
        {
            if (r == 1) vs.gridMode = ViewState::gridSmart; if (r == 2) vs.gridMode = ViewState::gridBar; if (r == 3) vs.gridMode = ViewState::gridBeat;
            if (r == 5) vs.snap = ! vs.snap;
            if (r >= 100) { vs.gridMode = ViewState::gridDivision; vs.division = divisionFromId (r - 100); vs.snap = true; }
            if (r) changed();
        });
    }
    void showSnapMenu()
    {
        juce::PopupMenu m;
        m.addItem (1, U ("Grid \xe2\x80\x94 to'g'ridan-to'g'ri gridga"), true, vs.snapType == 0);
        m.addItem (2, U ("Grid Relative \xe2\x80\x94 gridga nisbatan joy saqlanadi"), true, vs.snapType == 1);
        m.addItem (3, U ("Events \xe2\x80\x94 klip chetlariga yopishadi"), true, vs.snapType == 2);
        m.addItem (4, U ("Magnetic Cursor \xe2\x80\x94 kursorga yopishadi"), true, vs.snapType == 3);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&snapTypeB), [this] (int r) { if (r) { vs.snapType = r - 1; changed(); } });
    }
    void showQMenu()
    {
        juce::PopupMenu m;
        m.addItem (1, "Link to Grid", true, vs.linkQ);
        m.addSeparator();
        addDivisions (m, 100, vs.quantize, ! vs.linkQ);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&qB), [this] (int r)
        {
            if (r == 1) vs.linkQ = ! vs.linkQ;
            if (r >= 100) { vs.quantize = divisionFromId (r - 100); vs.linkQ = false; }
            if (r) changed();
        });
    }
    void showFormatMenu()
    {
        juce::PopupMenu m;
        m.addItem (1, "Bars+Beats", true, vs.timeFormat == 0); m.addItem (2, "Seconds", true, vs.timeFormat == 1);
        m.addItem (3, "Timecode", true, vs.timeFormat == 2); m.addItem (4, "Samples", true, vs.timeFormat == 3);
        juce::PopupMenu fps; for (int f : { 24, 25, 30, 60 }) fps.addItem (10 + f, juce::String (f) + " fps", true, vs.fps == f);
        m.addSubMenu ("Timecode fps", fps);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&fmtB), [this] (int r)
        {
            if (r >= 1 && r <= 4) vs.timeFormat = r - 1;
            if (r > 10) { vs.fps = r - 10; vs.timeFormat = ViewState::fmtTimecode; }
            if (r) changed();
        });
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (6, 3);
        gridB.setBounds (r.removeFromLeft (92)); r.removeFromLeft (6);
        snapTypeB.setBounds (r.removeFromLeft (128)); r.removeFromLeft (14);
        qB.setBounds (r.removeFromLeft (92)); r.removeFromLeft (4);
        qApplyB.setBounds (r.removeFromLeft (30)); r.removeFromLeft (14);
        fmtB.setBounds (r.removeFromLeft (112)); r.removeFromLeft (14);
        magB.setBounds (r.removeFromLeft (120));
    }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff0b0a0d));
        g.setColour (line); g.drawHorizontalLine (getHeight() - 1, 0, (float) getWidth());
    }
    juce::TextButton gridB, snapTypeB, qB, qApplyB, fmtB, magB;
private:
    void changed() { refresh(); vs.changed(); if (onChanged) onChanged(); }
    AppEngine& app; ViewState& vs;
};

// =============================================================================  Yuqori panel
// 1-qator: menyu · 2-qator: logotip + til, transport, displey, panel tugmalari, CPU
class TopBar : public juce::Component
{
public:
    static constexpr int menuH = 26, barH = 58;
    std::function<void (int)> onLanguage, onTogglePanel, onUiZoom;
    std::function<void()> onTheme;
    float uiScale = 1.0f;

    TopBar (AppEngine& a, juce::MenuBarModel* model) : app (a), menu (model), lcd (a), tempo (a)
    {
        addAndMakeVisible (menu); addAndMakeVisible (lcd); addAndMakeVisible (tempo);
        iconButton (startB, "#start", "Boshiga (Home / Num .)"); iconButton (rewB, "#rew", "Orqaga (Num -)"); iconButton (ffB, "#ff", "Oldinga (Num +)");
        iconButton (stopB, "#stop", "To'xtatish (Num 0)"); iconButton (playB, "#play", "Ijro (Space / Enter)", green); iconButton (recB, "#rec", "Yozish (R / Num *)", recRed);
        iconButton (loopB, "#loop", "Loop (L / Num /)"); iconButton (clickB, "#click", "Metronom (C)");
        for (auto* b : { &startB, &rewB, &ffB, &stopB, &playB, &recB, &loopB, &clickB }) { addAndMakeVisible (b); b->getProperties().set ("emboss", true); b->getProperties().set ("radius", 6.0f); }
        addAndMakeVisible (themeB); themeB.getProperties().set ("radius", 5.0f);
        themeB.setTooltip (U ("Mavzu (tema): AZ Qizil â Indigo"));
        themeB.onClick = [this] { if (onTheme) onTheme(); };
        const char* langs[] = { "UZ", "RU", "EN" };
        for (int i = 0; i < 3; ++i)
        {
            auto* b = langB.add (new juce::TextButton (langs[i]));
            b->getProperties().set ("radius", 5.0f);
            b->setClickingTogglesState (false);
            b->onClick = [this, i] { if (onLanguage) onLanguage (i); };
            addAndMakeVisible (b);
        }
        iconButton (zoomOutB, "#minus", "Kichiklashtirish (Ctrl -)"); iconButton (zoomInB, "#plus", "Kattalashtirish (Ctrl +)");
        zoomOutB.onClick = [this] { if (onUiZoom) onUiZoom (-1); }; zoomInB.onClick = [this] { if (onUiZoom) onUiZoom (1); };
        for (auto* b : { &zoomOutB, &zoomInB }) { b->getProperties().set ("radius", 11.0f); addAndMakeVisible (b); }
        iconButton (pInsp, "#pinsp", "Inspektor (Alt+I)"); iconButton (pMedia, "#pmedia", "Media brauzer (F5)"); iconButton (pMix, "#pmix", "Mikser (F3)");
        int k = 0;
        for (auto* b : { &pInsp, &pMedia, &pMix }) { addAndMakeVisible (b); b->onClick = [this, k] { if (onTogglePanel) onTogglePanel (k); }; ++k; }
        refreshLang();
    }
    void refreshLang() { for (int i = 0; i < 3; ++i) langB[i]->setToggleState (i == lang, juce::dontSendNotification); repaint(); }
    void setPanels (bool insp, bool media, bool mix)
    {
        pInsp.setToggleState (insp, juce::dontSendNotification); pMedia.setToggleState (media, juce::dontSendNotification); pMix.setToggleState (mix, juce::dontSendNotification);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto mr = r.removeFromTop (menuH).reduced (8, 0);
        zoomInB.setBounds (mr.removeFromRight (22).withSizeKeepingCentre (22, 22)); mr.removeFromRight (44);
        zoomOutB.setBounds (mr.removeFromRight (22).withSizeKeepingCentre (22, 22));
        menu.setBounds (mr.removeFromLeft (juce::jmin (mr.getWidth() - 10, 640)));
        r = r.reduced (14, 0);
        r.removeFromLeft (172);                                   // logotip
        for (auto* b : langB) { b->setBounds (r.removeFromLeft (30).withSizeKeepingCentre (30, 24)); r.removeFromLeft (3); }
        r.removeFromLeft (3); themeB.setBounds (r.removeFromLeft (30).withSizeKeepingCentre (30, 24));
        r.removeFromLeft (14);
        const bool narrow = getWidth() < 1400;
        cpuW = narrow ? 0 : 158;
        r.removeFromRight (cpuW);
        for (auto* b : { &pMix, &pMedia, &pInsp }) { b->setBounds (r.removeFromRight (32).withSizeKeepingCentre (32, 28)); r.removeFromRight (4); }
        r.removeFromRight (10);
        tempo.setBounds (r.removeFromRight (232).withSizeKeepingCentre (232, 42)); r.removeFromRight (8);
        const int lcdW = juce::jlimit (300, 420, r.getWidth() - 8 * 38 - 10);
        lcd.setBounds (r.removeFromRight (lcdW).withSizeKeepingCentre (lcdW, 42));
        r.removeFromRight (10);
        for (auto* b : { &clickB, &loopB, &recB, &playB, &stopB, &ffB, &rewB, &startB })
        { b->setBounds (r.removeFromRight (34).withSizeKeepingCentre (34, 30)); r.removeFromRight (4); }
    }
    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        g.setColour (Colour (0xff0b0a0d)); g.fillRect (0, 0, getWidth(), menuH);
        g.setColour (line); g.drawHorizontalLine (menuH - 1, 0, (float) getWidth());
        label (g, juce::String (roundToInt (uiScale * 100)) + "%", { (float) zoomOutB.getRight(), 0, (float) (zoomInB.getX() - zoomOutB.getRight()), (float) menuH }, 10.5f, silver, Justification::centred, 600);
        auto bar = Rectangle<float> (0, (float) menuH, (float) getWidth(), (float) barH);
        g.setGradientFill (ColourGradient (Colour (0xff0f0d10), 0, bar.getY(), Colour (0xff0a090c), 0, bar.getBottom(), false)); g.fillRect (bar);
        g.setColour (line); g.drawHorizontalLine (getHeight() - 1, 0, (float) getWidth());
        const float cy = bar.getCentreY();
        logoMark (g, { 16, cy - 16, 32, 32 });
        g.setFont (font (17, 700, 0.08f)); g.setColour (text); g.drawText ("AZ", Rectangle<float> (58, cy - 19, 40, 20), Justification::centredLeft);
        g.setColour (red); g.drawText ("MUS", Rectangle<float> (58 + textWidth (font (17, 700, 0.08f), "AZ "), cy - 19, 60, 20), Justification::centredLeft);
        g.setColour (dim); g.setFont (font (8, 500, 0.26f)); g.drawText ("AZIMXODJAYEV MUSIC", Rectangle<float> (58, cy + 2, 130, 12), Justification::centredLeft);
        // transport paneli: bo'rtma 3D, mavzu rangida
        {
            auto pr = startB.getBounds().getUnion (lcd.getBounds()).toFloat().expanded (9, 5);
            embossPanel (g, pr, red, 10);
            insetWell (g, lcd.getBounds().toFloat(), 10);
        }
        // mavzu tugmasi ikonkasi: ikki rang (joriy mavzu chap tomonda)
        {
            auto tb = themeB.getBounds().toFloat().reduced (7, 6);
            const Colour a (0xffff3347), b (0xff5a62e8);
            Path l, rr; l.addTriangle (tb.getTopLeft(), tb.getTopRight(), tb.getBottomLeft()); rr.addTriangle (tb.getTopRight(), tb.getBottomRight(), tb.getBottomLeft());
            g.setColour (themeId == 0 ? a : b); g.fillPath (l);
            g.setColour (themeId == 0 ? b : a); g.fillPath (rr);
        }
        if (cpuW > 0)
        {
            const float x = (float) getWidth() - 14 - (float) cpuW + 8;
            caps (g, "CPU", { x, cy - 6, 30, 12 }, 9.5f, dim);
            auto b = Rectangle<float> (x + 32, cy - 2.5f, 36, 5);
            g.setColour (Colours::white.withAlpha (0.07f)); g.fillRoundedRectangle (b, 2.5f);
            const float cpu = jlimit (0.0f, 1.0f, app.engine.getDeviceManager().getCpuUsage());
            g.setColour (cpu < 0.5f ? green : cpu < 0.8f ? yellow : red); g.fillRoundedRectangle (b.withWidth (jmax (3.0f, 36 * cpu)), 2.5f);
            auto& dm = app.engine.getDeviceManager();
            auto* dev = dm.deviceManager.getCurrentAudioDevice();
            const String type = dev ? dm.deviceManager.getCurrentAudioDeviceType() : U ("Audio yo'q");
            const double lat = dev ? (dev->getOutputLatencyInSamples() + dev->getCurrentBufferSizeSamples()) * 1000.0 / jmax (1.0, dev->getCurrentSampleRate()) : 0.0;
            caps (g, type.toUpperCase() + (dev ? U (" \xc2\xb7 ") + String (lat, 1) + " ms" : String()), { x + 74, cy - 7, (float) cpuW - 74, 14 }, 9.5f, dev ? green : red);
        }
    }
    void refreshState()
    {
        if (! app.edit) return;
        auto& t = app.edit->getTransport();
        playB.setToggleState (t.isPlaying(), juce::dontSendNotification);
        recB.setToggleState (t.isRecording(), juce::dontSendNotification);
        loopB.setToggleState (t.looping, juce::dontSendNotification);
        clickB.setToggleState (app.edit->clickTrackEnabled, juce::dontSendNotification);
        lcd.repaint(); tempo.repaint(); if (cpuW > 0) repaint (getWidth() - cpuW - 20, menuH, cpuW + 20, barH);
    }
    juce::TextButton startB, rewB, ffB, stopB, playB, recB, loopB, clickB, pInsp, pMedia, pMix, zoomOutB, zoomInB, themeB;
    LcdDisplay& getLcd() { return lcd; }
    TempoBox& getTempo() { return tempo; }
private:
    AppEngine& app; juce::MenuBarComponent menu; LcdDisplay lcd; TempoBox tempo;
    juce::OwnedArray<juce::TextButton> langB; int cpuW = 158;
};

// =============================================================================  Plagin menyusi
inline void showPluginMenu (AppEngine& app, te::AudioTrack& t, std::function<void()> done, bool instrumentsOnly = false)
{
    juce::PopupMenu m, azm, aw, vst, inst;
    for (int i = 0; i < (int) std::size (builtIns); ++i)
        (builtIns[i].synth ? inst : azm).addItem (1 + i, U (builtIns[i].name));
    std::map<juce::String, juce::PopupMenu> awCats;
    for (int i = 0; i < (int) std::size (airwindowsPlugins); ++i)
        awCats[airwindowsPlugins[i].category].addItem (1000 + i, airwindowsPlugins[i].name);
    for (auto& [c, sub] : awCats) aw.addSubMenu (U (c.toRawUTF8()), sub);
    auto types = app.engine.getPluginManager().knownPluginList.getTypes();
    std::map<juce::String, juce::PopupMenu> byVendor;
    for (int i = 0; i < types.size(); ++i)
        (types[i].isInstrument ? inst : byVendor[types[i].manufacturerName.isEmpty() ? "Boshqa" : types[i].manufacturerName]).addItem (5000 + i, types[i].name);
    for (auto& [v, sub] : byVendor) vst.addSubMenu (v, sub);
    if (! instrumentsOnly)
    {
        m.addSubMenu (U ("AZ effektlar"), azm);
        m.addSubMenu (U ("Airwindows (") + juce::String ((int) std::size (airwindowsPlugins)) + ")", aw);
        m.addSubMenu (U ("VST3 plaginlar (") + juce::String (types.size()) + ")", vst, types.size() > 0);
    }
    m.addSubMenu (U ("Instrumentlar"), inst);
    te::Track::Ptr tp (&t);
    m.showMenuAsync ({}, [&app, tp, types, done] (int r)
    {
        auto* at = dynamic_cast<te::AudioTrack*> (tp.get());
        if (r == 0 || at == nullptr) return;
        te::Plugin::Ptr p;
        if (r < 1000) p = app.insertPlugin (*at, builtIns[r - 1].xmlType, {});
        else if (r < 5000) p = app.insertPlugin (*at, airwindowsPlugins[r - 1000].xmlType, {});
        else p = app.insertPlugin (*at, te::ExternalPlugin::xmlTypeName, types[r - 5000]);
        if (p != nullptr) p->showWindowExplicitly();
        if (done) done();
    });
}

// =============================================================================  Inspektor
class Inspector : public juce::Component, private juce::Timer
{
public:
    std::function<void()> onPluginToggled;
    Inspector (AppEngine& a, ViewState& v) : app (a), vs (v)
    {
        for (auto* s : { &vol, &pan })
        {
            s->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag); s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            s->setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
            addAndMakeVisible (s);
        }
        vol.setRange (-60.0, 6.0, 0.1); vol.setSkewFactorFromMidPoint (-12.0); vol.setDoubleClickReturnValue (true, 0.0);
        vol.setColour (juce::Slider::rotarySliderFillColourId, red);
        pan.setRange (-1.0, 1.0, 0.01); pan.setDoubleClickReturnValue (true, 0.0); pan.getProperties().set ("bipolar", true);
        pan.setColour (juce::Slider::rotarySliderFillColourId, silver);
        vol.onValueChange = [this] { if (auto* t = track()) t->getVolumePlugin()->setVolumeDb ((float) vol.getValue()); repaint(); };
        pan.onValueChange = [this] { if (auto* t = track()) t->getVolumePlugin()->setPan ((float) pan.getValue()); repaint(); };
        addAndMakeVisible (input); input.onChange = [this] { applyInput(); };
        addAndMakeVisible (eqView); addAndMakeVisible (imagerView);
        startTimerHz (4);
    }
    te::AudioTrack* track() const { return app.edit ? findTrack (*app.edit, vs.selectedTrack) : nullptr; }

    void refresh()
    {
        auto* t = track();
        if (t != nullptr) ensureTrackTools (*t);           // har trekda o'z AZ EQ va AZ Imager'i
        eqView.setPlugins (t ? findTrackEq (*t) : nullptr, t ? findImager (*t) : nullptr);
        imagerView.setPlugin (t ? findImager (*t) : nullptr);
        eqView.setVisible (t != nullptr); imagerView.setVisible (t != nullptr);
        if (t) { vol.setValue (t->getVolumePlugin()->getVolumeDb(), juce::dontSendNotification); pan.setValue (t->getVolumePlugin()->getPan(), juce::dontSendNotification); }
        input.clear (juce::dontSendNotification);
        if (t && ! AppEngine::isInstrumentTrack (*t))
        {
            input.addItem (U ("\xe2\x80\x94 kirish yo'q \xe2\x80\x94"), 1);
            int sel = 1, id = 2;
            for (auto* inst : app.edit->getAllInputDevices())
                if (inst->getInputDevice().getDeviceType() == te::InputDevice::waveDevice)
                {
                    input.addItem (inst->getInputDevice().getName(), id);
                    if (te::isOnTargetTrack (*inst, *t, 0)) sel = id;
                    ++id;
                }
            input.setSelectedId (sel, juce::dontSendNotification);
        }
        input.setVisible (t && ! AppEngine::isInstrumentTrack (*t));
        resized(); repaint();
    }
    void applyInput()
    {
        auto* t = track(); if (! t) return;
        int id = 2;
        for (auto* inst : app.edit->getAllInputDevices())
            if (inst->getInputDevice().getDeviceType() == te::InputDevice::waveDevice)
            {
                if (id == input.getSelectedId()) { (void) inst->setTarget (t->itemID, true, &app.edit->getUndoManager(), 0); }
                else if (te::isOnTargetTrack (*inst, *t, 0)) (void) inst->removeTarget (t->itemID, &app.edit->getUndoManager());
                ++id;
            }
        app.edit->restartPlayback();
    }

    static constexpr int eqH = 128, imagerH = 258;
    int sendsTop() const { return eqH + 112 + 32 * slotCount() + 12; }
    int imagerTop() const { return channelTop() + 184; }
    int contentHeight() const { return imagerTop() + imagerH + 12; }
    int channelTop() const { auto* t = track(); return sendsTop() + 22 + 26 * ((t ? Routing::sends (*t).size() : 0) + 1) + 18; }

    void resized() override
    {
        scrollY = juce::jlimit (0, juce::jmax (0, contentHeight() - getHeight()), scrollY);
        const int y = channelTop() - scrollY;
        vol.setBounds (18, y + 20, 56, 56); pan.setBounds (88, y + 20, 56, 56);
        input.setBounds (12, y + 128, getWidth() - 24, 28);
        eqView.setBounds (8, 76 - scrollY, getWidth() - 16, eqH - 8);
        imagerView.setBounds (8, imagerTop() - scrollY, getWidth() - 16, imagerH);
    }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        const int maxS = juce::jmax (0, contentHeight() - getHeight());
        if (maxS == 0) { Component::mouseWheelMove (e, w); return; }
        scrollY = juce::jlimit (0, maxS, scrollY - juce::roundToInt (w.deltaY * 160.0f));
        resized(); repaint();
    }
    int slotCount() const { auto* t = track(); return t ? AppEngine::userPlugins (*t).size() + 1 : 0; }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        g.fillAll (bg1);
        g.setColour (line); g.drawVerticalLine (getWidth() - 1, 0, (float) getHeight());
        if (const int maxS = jmax (0, contentHeight() - getHeight()); maxS > 0)
        {   // aylantirish chizig'i
            const float h = (float) getHeight() * getHeight() / contentHeight(), y = (float) scrollY / maxS * (getHeight() - h);
            g.setColour (Colours::white.withAlpha (0.12f)); g.fillRoundedRectangle ((float) getWidth() - 5, y, 3, h, 1.5f);
        }
        g.setOrigin (0, -scrollY);
        caps (g, "INSPEKTOR", { 14, 12, 200, 12 });
        auto* t = track();
        if (! t) { label (g, U ("Trek tanlang"), { 14, 40, 200, 20 }, 12, dim); return; }
        auto nr = Rectangle<float> (12, 32, (float) getWidth() - 24, 38);
        g.setColour (t->getColour().withAlpha (0.1f)); g.fillRoundedRectangle (nr, 9);
        g.setColour (t->getColour().withAlpha (0.4f)); g.drawRoundedRectangle (nr, 9, 1);
        g.setColour (t->getColour()); g.fillRoundedRectangle (nr.getX() + 10, nr.getCentreY() - 4, 8, 8, 2);
        label (g, t->getName(), nr.withTrimmedLeft (26).withTrimmedRight (70), 13, text, Justification::centredLeft, 700);
        caps (g, AppEngine::isInstrumentTrack (*t) ? "INSTR" : "AUDIO", nr.withTrimmedRight (10), 9, dim, Justification::centredRight);

        caps (g, "INSERTLAR", { 14, (float) eqH + 84, 200, 12 });
        auto plugs = AppEngine::userPlugins (*t);
        slots.clear();
        for (int i = 0; i <= plugs.size(); ++i)
        {
            auto r = Rectangle<float> (12, (float) eqH + 104.0f + (float) i * 32, (float) getWidth() - 24, 28);
            slots.add (r);
            if (i == plugs.size()) { panel (g, r, 7, 0.015f, 0.06f); label (g, U ("+  Plagin qo'shish"), r.reduced (12, 0), 11.5f, dim, Justification::centredLeft, 500); break; }
            auto* p = plugs[i];
            const bool az = pluginDisplayName (*p).startsWithIgnoreCase ("AZ");
            if (az) { g.setColour (red.withAlpha (0.1f)); g.fillRoundedRectangle (r, 7); g.setColour (red.withAlpha (0.35f)); g.drawRoundedRectangle (r.reduced (0.5f), 7, 1); }
            else panel (g, r, 7);
            g.setColour (! p->isEnabled() ? dim2 : az ? red : green); g.fillEllipse (r.getX() + 10, r.getCentreY() - 3, 6, 6);
            label (g, pluginDisplayName (*p), r.withTrimmedLeft (24).withTrimmedRight (50), 11.5f, p->isEnabled() ? silver : dim2, Justification::centredLeft, az ? 700 : 400);
            caps (g, p->isSynth() ? "INSTR" : dynamic_cast<te::ExternalPlugin*> (p) ? "VST3" : "FX", r.withTrimmedRight (9), 8.5f, az ? redSoft : dim, Justification::centredRight);
        }
        // ---- sendlar (Group/FX kanallarga)
        {
            const float sy = (float) sendsTop();
            caps (g, "SENDLAR", { 14, sy, 200, 12 });
            sendRows.clear();
            auto ss = Routing::sends (*t);
            for (int i = 0; i <= ss.size(); ++i)
            {
                auto r = Rectangle<float> (12, sy + 18 + (float) i * 26, (float) getWidth() - 24, 23);
                sendRows.add (r);
                if (i == ss.size()) { panel (g, r, 6, 0.015f, 0.06f); label (g, U ("+  Send (FX kanalga)"), r.reduced (10, 0), 11, dim, Justification::centredLeft, 500); break; }
                auto* s = ss[i];
                panel (g, r, 6);
                const float frac = jlimit (0.0f, 1.0f, (s->getGainDb() + 60.0f) / 66.0f);
                g.setColour (blue.withAlpha (0.18f)); g.fillRoundedRectangle (r.withWidth (r.getWidth() * frac), 6);
                label (g, s->getBusName(), r.reduced (10, 0).withTrimmedRight (60), 11, silver, Justification::centredLeft, 500);
                label (g, String (s->getGainDb(), 1) + " dB", r.reduced (10, 0), 10.5f, text, Justification::centredRight, 500);
            }
        }
        const float y = (float) channelTop();
        caps (g, "KANAL", { 14, y, 200, 12 });
        caps (g, "VOLUME", { 10, y + 80, 72, 12 }, 8.5f, dim, Justification::centred);
        caps (g, "PAN", { 80, y + 80, 72, 12 }, 8.5f, dim, Justification::centred);
        label (g, String (vol.getValue(), 1) + " dB", { 150, y + 30, 80, 14 }, 12, text, Justification::centredLeft, 600);
        const double pv = pan.getValue();
        label (g, std::abs (pv) < 0.01 ? String ("C") : (pv < 0 ? "L" : "R") + String (roundToInt (std::abs (pv) * 100)), { 150, y + 48, 80, 14 }, 12, dim, Justification::centredLeft, 500);
        if (input.isVisible()) caps (g, "AUDIO KIRISH", { 14, y + 110, 200, 12 });
        else label (g, U ("MIDI: barcha klaviaturalar \xe2\x86\x92 shu trek"), { 14, y + 110, (float) getWidth() - 20, 14 }, 10.5f, dim);
        caps (g, U ("STEREO IMAGER"), { 14, (float) imagerTop() - 16, 200, 12 });
    }

    void mouseDown (const juce::MouseEvent& ev) override
    {
        const auto e = ev.withNewPosition (ev.position.translated (0, (float) scrollY));
        auto* t = track(); if (! t) return;
        dragSend = nullptr;
        auto ss = Routing::sends (*t);
        for (int i = 0; i < sendRows.size(); ++i)
            if (sendRows[i].contains (e.position))
            {
                if (i == ss.size()) { showSendMenu (*t); return; }
                if (e.mods.isAltDown()) { ss[i]->setEnabled (! ss[i]->isEnabled()); refresh(); if (onPluginToggled) onPluginToggled(); return; }
                if (e.mods.isPopupMenu())
                {
                    te::Plugin::Ptr p (ss[i]);
                    juce::PopupMenu m; m.addItem (1, U ("Sendni o'chirish"));
                    m.showMenuAsync ({}, [this, p] (int r) { if (r == 1) p->deleteFromParent(); refresh(); });
                    return;
                }
                dragSend = ss[i]; dragStartDb = ss[i]->getGainDb(); return;
            }
        auto plugs = AppEngine::userPlugins (*t);
        for (int i = 0; i < slots.size(); ++i)
            if (slots[i].contains (e.position))
            {
                if (i == plugs.size())
                {
                    te::Track::Ptr tp (t);
                    showPluginPicker (app, localAreaToGlobal (slots[i].translated (0, (float) -scrollY).toNearestInt()), [this, tp] (const PickItem& it)
                    {
                        if (auto* at = dynamic_cast<te::AudioTrack*> (tp.get()))
                            if (auto p = insertPicked (app, *at, it)) p->showWindowExplicitly();
                        refresh();
                    });
                    return;
                }
                te::Plugin::Ptr p (plugs[i]);
                if (e.mods.isAltDown()) { p->setEnabled (! p->isEnabled()); refresh(); if (onPluginToggled) onPluginToggled(); return; }   // Alt+bosish = bypass
                if (e.mods.isPopupMenu())
                {
                    juce::PopupMenu m; m.addSectionHeader (pluginDisplayName (*p));
                    m.addItem (1, U ("Oynasini ochish")); m.addItem (2, p->isEnabled() ? "Bypass" : U ("Yoqish"));
                    m.addSeparator(); m.addItem (3, U ("O'chirish"));
                    m.showMenuAsync ({}, [this, p] (int r)
                    {
                        if (r == 1) p->showWindowExplicitly();
                        if (r == 2) p->setEnabled (! p->isEnabled());
                        if (r == 3) { p->edit.getUndoManager().beginNewTransaction ("Plagin"); p->deleteFromParent(); }
                        refresh();
                    });
                }
                else p->showWindowExplicitly();
                return;
            }
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragSend == nullptr) return;
        dragSend->setGainDb (juce::jlimit (-60.0f, 6.0f, dragStartDb + (float) e.getDistanceFromDragStartX() * 0.25f));
        repaint();
    }
    void mouseDoubleClick (const juce::MouseEvent& ev) override
    {
        const auto e = ev.withNewPosition (ev.position.translated (0, (float) scrollY));
        if (auto* t = track()) { auto ss = Routing::sends (*t); for (int i = 0; i < juce::jmin (ss.size(), sendRows.size()); ++i) if (sendRows[i].contains (e.position)) { ss[i]->setGainDb (-10.0f); repaint(); } }
    }
    void showSendMenu (te::AudioTrack& t)
    {
        Routing rt (app);
        juce::PopupMenu m; auto fx = rt.fxTracks();
        for (int i = 0; i < fx.size(); ++i) if (fx[i] != &t) m.addItem (100 + i, fx[i]->getName());
        if (fx.size() > 0) m.addSeparator();
        m.addItem (1, U ("Yangi FX kanal (AZ Reverb)"));
        m.addItem (2, U ("Yangi FX kanal (AZ Delay)"));
        m.addItem (3, U ("Yangi FX kanal (plagin tanlash...)"));
        te::Track::Ptr tp (&t);
        m.showMenuAsync ({}, [this, tp, fx] (int r)
        {
            auto* at = dynamic_cast<te::AudioTrack*> (tp.get()); if (at == nullptr || r == 0) return;
            Routing rt2 (app);
            if (r >= 100) { rt2.addSend (*at, Routing::fxBus (*fx[r - 100])); refresh(); if (onRoutingChanged) onRoutingChanged(); return; }
            auto done = [this, tp] (te::AudioTrack* fxT)
            {
                if (auto* src = dynamic_cast<te::AudioTrack*> (tp.get())) if (fxT) Routing (app).addSend (*src, Routing::fxBus (*fxT));
                refresh(); if (onRoutingChanged) onRoutingChanged();
            };
            if (r == 1) done (rt2.addFxChannel ("Reverb", "reverb", {}));
            if (r == 2) done (rt2.addFxChannel ("Delay", "delay", {}));
            if (r == 3)
                showPluginPicker (app, localAreaToGlobal (getLocalBounds().withHeight (40)), [this, done] (const PickItem& it)
                {
                    Routing rt3 (app);
                    juce::String xml; juce::PluginDescription d;
                    if (it.kind == 0) xml = builtIns[it.index].xmlType;
                    else if (it.kind == 1) xml = airwindowsPlugins[it.index].xmlType;
                    else if (it.kind == 2) { xml = te::ExternalPlugin::xmlTypeName; d = app.engine.getPluginManager().knownPluginList.getTypes()[it.index]; }
                    done (rt3.addFxChannel (it.name, xml, d));
                });
        });
    }
    std::function<void()> onRoutingChanged;

private:
    void timerCallback() override
    {
        auto* t = track();
        const int n = slotCount() * 100 + (t ? Routing::sends (*t).size() : 0);
        if (n != lastSlots) { lastSlots = n; refresh(); }
    }
    te::AuxSendPlugin* dragSend = nullptr; float dragStartDb = 0;
    juce::Array<juce::Rectangle<float>> sendRows;
    AppEngine& app; ViewState& vs;
    juce::Slider vol, pan; juce::ComboBox input;
    EqView eqView { true }; ImagerView imagerView { true }; int scrollY = 0;
    juce::Array<juce::Rectangle<float>> slots; int lastSlots = -1;
};

// =============================================================================  Mikser
// ---------------------------------------------------------------- Master insertlar (mastering)
inline juce::Array<te::Plugin*> masterInserts (te::Edit& e)
{
    juce::Array<te::Plugin*> r;
    for (auto p : e.getMasterPluginList().getPlugins()) { if (p->getPluginType() == "azcontrolroom") break; r.add (p); }
    return r;
}
inline te::Plugin::Ptr createPicked (AppEngine& app, const PickItem& it)
{
    juce::String xml; juce::PluginDescription d;
    if (it.kind == 0) xml = builtIns[it.index].xmlType;
    else if (it.kind == 1) xml = airwindowsPlugins[it.index].xmlType;
    else if (it.kind == 2) { xml = te::ExternalPlugin::xmlTypeName; d = app.engine.getPluginManager().knownPluginList.getTypes()[it.index]; }
    else return {};
    return app.edit->getPluginCache().createNewPlugin (xml, d);
}
// Control Room insert: CR'dan keyin (faqat monitor, eksportga tushmaydi)
inline te::Plugin::Ptr insertControlRoom (AppEngine& app, const PickItem& it)
{
    Routing (app).controlRoom();
    auto p = createPicked (app, it);
    if (p == nullptr) return {};
    app.edit->getUndoManager().beginNewTransaction ("Control Room insert");
    app.edit->getMasterPluginList().insertPlugin (p, -1, nullptr);
    return p;
}
inline te::Plugin::Ptr insertMaster (AppEngine& app, const PickItem& it)
{
    auto p = createPicked (app, it);
    if (p == nullptr) return {};
    auto& list = app.edit->getMasterPluginList();
    int idx = list.size();
    for (int i = 0; i < list.size(); ++i) if (list.getPlugins()[i]->getPluginType() == "azcontrolroom") { idx = i; break; }
    app.edit->getUndoManager().beginNewTransaction ("Master insert");
    list.insertPlugin (p, idx, nullptr);
    return p;
}

class ChannelStrip : public juce::Component
{
public:
    te::AudioTrack* getTrack() const { return track; }
    ChannelStrip (AppEngine& a, te::AudioTrack* t) : app (a), track (t)
    {
        fader.setSliderStyle (juce::Slider::LinearVertical); fader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        fader.setRange (-60.0, 6.0, 0.1); fader.setSkewFactorFromMidPoint (-10.0); fader.setDoubleClickReturnValue (true, 0.0);
        pan.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag); pan.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        pan.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        pan.setRange (-1.0, 1.0, 0.01); pan.setDoubleClickReturnValue (true, 0.0); pan.getProperties().set ("bipolar", true);
        pan.setColour (juce::Slider::rotarySliderFillColourId, silver);
        addAndMakeVisible (fader); addAndMakeVisible (pan);
        auto vp = volumePlugin();
        if (vp) { fader.setValue (vp->getVolumeDb(), juce::dontSendNotification); pan.setValue (vp->getPan(), juce::dontSendNotification); }
        fader.onValueChange = [this] { if (auto v = volumePlugin()) v->setVolumeDb ((float) fader.getValue()); repaint(); };
        pan.onValueChange = [this] { if (auto v = volumePlugin()) v->setPan ((float) pan.getValue()); };
        attachMeter();
    }
    ~ChannelStrip() override { detachMeter(); }

    te::VolumeAndPanPlugin* volumePlugin() const { return track ? track->getVolumePlugin() : app.edit->getMasterVolumePlugin().get(); }
    void attachMeter()
    {
        if (track) { if (auto* lm = track->getLevelMeterPlugin()) { lm->measurer.addClient (client); measurer = &lm->measurer; } }
        else if (auto* ctx = app.edit->getCurrentPlaybackContext()) { ctx->masterLevels.addClient (client); measurer = &ctx->masterLevels; }
    }
    void detachMeter() { if (measurer) measurer->removeClient (client); measurer = nullptr; }

    int panY() const   { return track ? 84 : 64; }
    int faderY() const { return track ? 120 : 104; }
    void resized() override
    {
        pan.setBounds (getWidth() / 2 - 17, panY(), 34, 34);
        fader.setBounds (8, faderY(), 30, getHeight() - faderY() - 46);
    }
    std::function<void()> onRoutingChanged;
    std::function<void (ChannelStrip&)> onToggleSpectro;
    std::function<void (ChannelStrip&, int)> onReorderDrag;     // mikser koordinatasidagi x
    std::function<void (ChannelStrip&)> onReorderEnd;
    bool spectroOpen = false, reordering = false;
    juce::Rectangle<float> specBtn() const { return { (float) getWidth() - 24, (float) panY() + 9, 18, 16 }; }
    bool inNameArea (juce::Point<float> p) const { return p.y > getHeight() - 40; }
    juce::Rectangle<float> insSlot (int i) const  { return { 6, 4.0f + (float) i * 15.0f, (float) getWidth() - 12, 13 }; }
    juce::Rectangle<float> sendSlot (int i) const { return { 6, 52.0f + (float) i * 15.0f, (float) getWidth() - 12, 13 }; }
    void trackMouseDown (const juce::MouseEvent& e)
    {
        reordering = false;
        if (specBtn().contains (e.position)) { if (onToggleSpectro) onToggleSpectro (*this); return; }
        if (inNameArea (e.position) && ! e.mods.isPopupMenu()) { reordering = true; return; }
        auto plugs = AppEngine::userPlugins (*track);
        juce::Component::SafePointer<ChannelStrip> sp (this);
        te::Track::Ptr tp (track);
        for (int i = 0; i < 3; ++i)
        {
            if (! insSlot (i).contains (e.position)) continue;
            if (i < plugs.size())
            {
                te::Plugin::Ptr p (plugs[i]);
                if (e.mods.isAltDown()) { p->setEnabled (! p->isEnabled()); changed(); return; }   // Alt+bosish = bypass
                if (! e.mods.isPopupMenu()) { p->showWindowExplicitly(); return; }
                juce::PopupMenu m; m.addSectionHeader (pluginDisplayName (*p));
                m.addItem (1, U ("Oynasini ochish")); m.addItem (2, p->isEnabled() ? "Bypass" : U ("Yoqish"));
                m.addItem (4, U ("Almashtirish...")); m.addSeparator(); m.addItem (3, U ("O'chirish"));
                m.showMenuAsync ({}, [sp, p, tp, this, i] (int r)
                {
                    if (r == 1) p->showWindowExplicitly();
                    if (r == 2) p->setEnabled (! p->isEnabled());
                    if (r == 3) { p->edit.getUndoManager().beginNewTransaction ("Insert"); p->deleteFromParent(); }
                    if (r == 4 && sp)
                        showPluginPicker (app, localAreaToGlobal (insSlot (i).toNearestInt()), [sp, p, tp, this] (const PickItem& it)
                        {
                            auto* at = dynamic_cast<te::AudioTrack*> (tp.get()); if (! at || ! sp) return;
                            const int idx = at->pluginList.indexOf (p.get());
                            p->deleteFromParent();
                            if (auto np = insertPicked (app, *at, it)) { if (idx >= 0) { np->removeFromParent(); at->pluginList.insertPlugin (np, juce::jmin (idx, at->pluginList.size()), nullptr); } np->showWindowExplicitly(); }
                            sp->changed();
                        });
                    if (sp) sp->changed();
                });
            }
            else
                showPluginPicker (app, localAreaToGlobal (insSlot (i).toNearestInt()), [sp, tp, this] (const PickItem& it)
                {
                    if (! sp) return;
                    if (auto* at = dynamic_cast<te::AudioTrack*> (tp.get())) if (auto p = insertPicked (app, *at, it)) p->showWindowExplicitly();
                    sp->changed();
                });
            return;
        }
        auto ss = Routing::sends (*track);
        for (int i = 0; i < 2; ++i)
        {
            if (! sendSlot (i).contains (e.position)) continue;
            if (i < ss.size())
            {
                auto* snd = ss[i];
                if (e.mods.isAltDown()) { snd->setEnabled (! snd->isEnabled()); changed(); return; }
                if (e.mods.isPopupMenu())
                {
                    te::Plugin::Ptr p (snd);
                    juce::PopupMenu m; m.addSectionHeader (juce::String::fromUTF8 ("Send \xe2\x86\x92 ") + app.edit->getAuxBusName (snd->getBusNumber()));
                    m.addItem (2, snd->isEnabled() ? U ("O'chirib qo'yish (bypass)") : U ("Yoqish")); m.addItem (5, "0 dB"); m.addItem (6, "-10 dB");
                    m.addSeparator(); m.addItem (3, U ("Sendni olib tashlash"));
                    m.showMenuAsync ({}, [sp, p] (int r)
                    {
                        auto* s2 = dynamic_cast<te::AuxSendPlugin*> (p.get()); if (! s2) return;
                        if (r == 2) s2->setEnabled (! s2->isEnabled());
                        if (r == 5) s2->setGainDb (0); if (r == 6) s2->setGainDb (-10);
                        if (r == 3) { s2->edit.getUndoManager().beginNewTransaction ("Send"); s2->deleteFromParent(); }
                        if (sp) sp->changed();
                    });
                }
                else { dragSend = snd; dragStartDb = snd->getGainDb(); }
            }
            else if (i == ss.size()) showSendMenu (sendSlot (i));
            return;
        }
    }
    void showSendMenu (juce::Rectangle<float> area)
    {
        Routing rt (app);
        juce::PopupMenu m; auto fx = rt.fxTracks();
        for (int k = 0; k < fx.size(); ++k) if (fx[k] != track) m.addItem (100 + k, fx[k]->getName());
        if (fx.size() > 0) m.addSeparator();
        m.addItem (1, U ("Yangi FX kanal (AZ Reverb)")); m.addItem (2, U ("Yangi FX kanal (AZ Delay)")); m.addItem (3, U ("Yangi FX kanal (plagin tanlash...)"));
        juce::Component::SafePointer<ChannelStrip> sp (this);
        te::Track::Ptr tp (track);
        m.showMenuAsync ({}, [this, sp, tp, fx, area] (int r)
        {
            auto* at = dynamic_cast<te::AudioTrack*> (tp.get()); if (! at || r == 0 || ! sp) return;
            Routing rt2 (app);
            auto done = [this, tp, sp] (te::AudioTrack* fxT)
            {
                if (auto* src = dynamic_cast<te::AudioTrack*> (tp.get())) if (fxT) Routing (app).addSend (*src, Routing::fxBus (*fxT));
                if (sp) sp->changed (true);
            };
            if (r >= 100) { done (fx[r - 100]); return; }
            if (r == 1) done (rt2.addFxChannel ("Reverb", "reverb", {}));
            if (r == 2) done (rt2.addFxChannel ("Delay", "delay", {}));
            if (r == 3)
                showPluginPicker (app, localAreaToGlobal (area.toNearestInt()), [this, done] (const PickItem& it)
                {
                    juce::String xml; juce::PluginDescription d;
                    if (it.kind == 0) xml = builtIns[it.index].xmlType;
                    else if (it.kind == 1) xml = airwindowsPlugins[it.index].xmlType;
                    else if (it.kind == 2) { xml = te::ExternalPlugin::xmlTypeName; d = app.engine.getPluginManager().knownPluginList.getTypes()[it.index]; }
                    done (Routing (app).addFxChannel (it.name, xml, d));
                });
        });
    }
    void changed (bool structural = false) { repaint(); if (onRoutingChanged && structural) onRoutingChanged(); else if (onRoutingChanged) onRoutingChanged(); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (reordering && std::abs (e.getDistanceFromDragStartX()) > 6)
        {
            setMouseCursor (juce::MouseCursor::DraggingHandCursor);
            if (onReorderDrag && getParentComponent()) onReorderDrag (*this, e.getEventRelativeTo (getParentComponent()).x);
            return;
        }
        if (dragSend == nullptr) return;
        dragSend->setGainDb ((float) juce::jlimit (-60.0, 6.0, dragStartDb + e.getDistanceFromDragStartX() * 0.25 - e.getDistanceFromDragStartY() * 0.25));
        repaint();
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        dragSend = nullptr;
        if (reordering && std::abs (e.getDistanceFromDragStartX()) > 6 && onReorderEnd) { setMouseCursor (juce::MouseCursor::NormalCursor); onReorderEnd (*this); }
        reordering = false;
    }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (track == nullptr) return;
        auto ss = Routing::sends (*track);
        for (int i = 0; i < juce::jmin (2, ss.size()); ++i) if (sendSlot (i).contains (e.position)) { ss[i]->setGainDb (0); repaint(); }
    }
    void tick()
    {
        bool ch = false;
        for (int c = 0; c < 2; ++c)
        {
            const float db = client.getAndClearAudioLevel (c).dB;
            const float v = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f);
            const float nv = v > lv[c] ? v : juce::jmax (0.0f, lv[c] - 0.03f);
            if (std::abs (nv - lv[c]) > 0.003f) { lv[c] = nv; ch = true; }
        }
        if (ch) repaint (40, faderY(), 30, getHeight() - faderY() - 46);
    }
    float level() const { return juce::jmax (lv[0], lv[1]); }

    juce::Rectangle<float> masterSlot (int i) const { return { 6, 4.0f + (float) i * 14.5f, (float) getWidth() - 12, 13 }; }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (app.edit == nullptr) return;
        if (track != nullptr) { trackMouseDown (e); return; }
        auto plugs = masterInserts (*app.edit);
        for (int i = 0; i < 4; ++i)
        {
            if (! masterSlot (i).contains (e.position)) continue;
            if (i < plugs.size())
            {
                te::Plugin::Ptr p (plugs[i]);
                if (e.mods.isAltDown()) { p->setEnabled (! p->isEnabled()); repaint(); return; }
                if (! e.mods.isPopupMenu()) { p->showWindowExplicitly(); return; }
                juce::PopupMenu m; m.addSectionHeader (pluginDisplayName (*p));
                m.addItem (1, U ("Oynasini ochish")); m.addItem (2, p->isEnabled() ? "Bypass" : U ("Yoqish"));
                m.addSeparator(); m.addItem (3, U ("O'chirish"));
                juce::Component::SafePointer<ChannelStrip> sp (this);
                m.showMenuAsync ({}, [sp, p] (int r)
                {
                    if (r == 1) p->showWindowExplicitly();
                    if (r == 2) p->setEnabled (! p->isEnabled());
                    if (r == 3) { p->edit.getUndoManager().beginNewTransaction ("Master insert"); p->deleteFromParent(); }
                    if (sp) sp->repaint();
                });
            }
            else
            {
                juce::Component::SafePointer<ChannelStrip> sp (this);
                showPluginPicker (app, localAreaToGlobal (masterSlot (i).toNearestInt()), [sp, this] (const PickItem& it)
                { if (sp == nullptr) return; if (auto p = insertMaster (app, it)) p->showWindowExplicitly(); sp->repaint(); });
            }
            return;
        }
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        const bool master = track == nullptr;
        const auto col = master ? red : track->getColour();
        g.setColour (Colours::white.withAlpha (0.02f)); g.fillRoundedRectangle (r, 10);
        g.setColour (master ? red.withAlpha (0.3f) : Colours::white.withAlpha (0.06f)); g.drawRoundedRectangle (r, 10, 1);
        // insertlar
        if (track)
        {
            auto plugs = AppEngine::userPlugins (*track);
            for (int i = 0; i < 3; ++i)
            {
                auto s = insSlot (i);
                const bool has = i < plugs.size(), add = i == plugs.size();
                const bool az = has && pluginDisplayName (*plugs[i]).startsWithIgnoreCase ("AZ");
                const bool on = has && plugs[i]->isEnabled();
                g.setColour (has ? (az ? red : silver).withAlpha (on ? 0.16f : 0.05f) : Colours::white.withAlpha (add ? 0.05f : 0.025f)); g.fillRoundedRectangle (s, 4);
                String txt = has ? pluginDisplayName (*plugs[i]) : add ? U ("+ Insert") : String::fromUTF8 ("\xe2\x80\x94");
                if (i == 2 && plugs.size() > 3) txt = "+" + String (plugs.size() - 2) + " insert";
                label (g, txt, s.reduced (5, 0), 8.5f, has ? (on ? (az ? redSoft : Colour (0xffd0c9ca)) : dim2) : add ? silver : dim2, Justification::centredLeft, has ? 500 : 400);
            }
            auto ss = Routing::sends (*track);
            for (int i = 0; i < 2; ++i)
            {
                auto s = sendSlot (i);
                const bool has = i < ss.size(), add = i == ss.size();
                const Colour sc (0xff4ad6ff);
                g.setColour (has ? sc.withAlpha (ss[i]->isEnabled() ? 0.14f : 0.05f) : Colours::white.withAlpha (add ? 0.05f : 0.025f)); g.fillRoundedRectangle (s, 4);
                if (has)
                {
                    const float frac = jlimit (0.0f, 1.0f, (ss[i]->getGainDb() + 60.0f) / 66.0f);
                    g.setColour (sc.withAlpha (0.25f)); g.fillRoundedRectangle (s.withWidth (s.getWidth() * frac), 4);
                    label (g, String::fromUTF8 ("\xe2\x86\x92 ") + app.edit->getAuxBusName (ss[i]->getBusNumber()), s.reduced (5, 0), 8.5f, ss[i]->isEnabled() ? sc : dim2, Justification::centredLeft, 500);
                    label (g, String (ss[i]->getGainDb(), 1), s.reduced (4, 0), 8, silver, Justification::centredRight, 500);
                }
                else label (g, add ? U ("+ Send") : String::fromUTF8 ("\xe2\x80\x94"), s.reduced (5, 0), 8.5f, add ? silver : dim2, Justification::centredLeft, 400);
            }
            // spektrogramma tugmasi
            {
                auto sb = specBtn();
                g.setColour (spectroOpen ? Colour (0xff4ad6ff).withAlpha (0.3f) : Colours::white.withAlpha (0.05f)); g.fillRoundedRectangle (sb, 3);
                Path w; for (int k = 0; k < 3; ++k) { const float yy = sb.getBottom() - 3 - k * 3.2f, xo = k * 1.6f;
                    w.startNewSubPath (sb.getX() + 3 + xo, yy); w.lineTo (sb.getX() + 7 + xo, yy - 4); w.lineTo (sb.getX() + 10 + xo, yy - 1.5f); w.lineTo (sb.getRight() - 3 - (2 - k) * 1.0f, yy - 5); }
                g.setColour (spectroOpen ? Colour (0xff4ad6ff) : silver); g.strokePath (w, PathStrokeType (1.0f));
            }
        }
        else
        {
            // MASTER INSERT slotlari (mastering zanjiri)
            auto plugs = masterInserts (*app.edit);
            for (int i = 0; i < 4; ++i)
            {
                auto s = masterSlot (i);
                const bool has = i < plugs.size();
                const bool add = i == plugs.size();
                g.setColour (has ? red.withAlpha (plugs[i]->isEnabled() ? 0.16f : 0.06f) : Colours::white.withAlpha (add ? 0.05f : 0.025f)); g.fillRoundedRectangle (s, 4);
                const String txt = has ? pluginDisplayName (*plugs[i]) : add ? U ("+ Master insert") : String::fromUTF8 ("\xe2\x80\x94");
                label (g, txt, s.reduced (5, 0), 8.5f, has ? (plugs[i]->isEnabled() ? redSoft : dim2) : add ? silver : dim2, Justification::centredLeft, has ? 500 : 400);
            }
        }
        // metr
        for (int c = 0; c < 2; ++c)
        {
            auto m = Rectangle<float> (46.0f + (float) c * 9, (float) faderY(), 5, (float) (getHeight() - faderY() - 46));
            g.setColour (Colours::white.withAlpha (0.05f)); g.fillRoundedRectangle (m, 2.5f);
            auto f = m.withTop (m.getBottom() - m.getHeight() * lv[c]);
            g.setGradientFill (ColourGradient (red, 0, m.getY(), green, 0, m.getBottom(), false));
            g.fillRoundedRectangle (f, 2.5f);
        }
        label (g, String (fader.getValue(), 1) + " dB", { 0, (float) getHeight() - 42, r.getWidth(), 14 }, 10, silver, Justification::centred, 400);
        label (g, master ? String ("MASTER") : track->getName(), { 4, (float) getHeight() - 26, r.getWidth() - 8, 14 }, 10, master ? redSoft : text, Justification::centred, 600);
        g.setColour (col); g.fillRoundedRectangle (10, (float) getHeight() - 5, r.getWidth() - 20, 3, 1.5f);
    }
private:
    AppEngine& app; te::AudioTrack* track;
    juce::Slider fader, pan;
    te::LevelMeasurer::Client client; te::LevelMeasurer* measurer = nullptr;
    float lv[2] {};
    te::AuxSendPlugin* dragSend = nullptr; double dragStartDb = 0;
};


// =============================================================================  Control Room kanali
// Masterdan keyin, faqat monitorlarga: Monitor Match (monitor modeli emulyatsiyasi),
// 2 ta CR insert (masalan, tashqi Monitor Match VST3 yoki Sonarworks), balandlik, Dim, Mono, Mute.
class ControlRoomStrip : public juce::Component
{
public:
    explicit ControlRoomStrip (AppEngine& a) : app (a), rt (a)
    {
        for (auto* b : { &mmOn, &dimB, &monoB, &muteB }) { addAndMakeVisible (b); b->setClickingTogglesState (true); b->getProperties().set ("radius", 5.0f); }
        mmOn.setButtonText ("MONITOR MATCH"); dimB.setButtonText ("DIM"); monoB.setButtonText ("MONO"); muteB.setButtonText ("MUTE");
        mmOn.setColour (juce::TextButton::buttonOnColourId, cyan.withAlpha (0.35f));
        dimB.setColour (juce::TextButton::buttonOnColourId, yellow.withAlpha (0.35f));
        monoB.setColour (juce::TextButton::buttonOnColourId, blue.withAlpha (0.35f));
        muteB.setColour (juce::TextButton::buttonOnColourId, red.withAlpha (0.45f));
        addAndMakeVisible (src); addAndMakeVisible (tgt); addAndMakeVisible (amt); addAndMakeVisible (lvl);
        src.setTooltip (U ("Sizning monitoringiz (manba)")); tgt.setTooltip (U ("Qaysi monitorda eshitmoqchisiz (maqsad)"));
        for (int i = 0; i < (int) mm::sourceProfiles().size(); ++i) src.addItem (MonitorMatchPlugin::sourceName (i), i + 1);
        src.addSeparator(); src.addItem (U ("O'lchov fayli (REW)..."), 1000);
        for (int i = 0; i < (int) mm::targetProfiles().size(); ++i) tgt.addItem (MonitorMatchPlugin::targetName (i), i + 1);
        amt.setSliderStyle (juce::Slider::LinearBar); amt.setRange (0, 100, 1); amt.setTextValueSuffix (U ("% emulyatsiya"));
        amt.setColour (juce::Slider::trackColourId, cyan.withAlpha (0.28f));
        lvl.setSliderStyle (juce::Slider::LinearVertical); lvl.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        lvl.setRange (-60.0, 6.0, 0.1); lvl.setSkewFactorFromMidPoint (-10.0); lvl.setDoubleClickReturnValue (true, 0.0);
        lvl.setTooltip (U ("Monitor balandligi (eksportga ta'sir qilmaydi)"));
        refresh();

        mmOn.onClick = [this]
        {
            auto* m = rt.monitorMatch (mmOn.getToggleState());
            if (m) m->setEnabled (mmOn.getToggleState());
            refresh();
        };
        src.onChange = [this]
        {
            const int id = src.getSelectedId();
            if (id == 1000) { loadMeasurement(); return; }
            if (auto* m = rt.monitorMatch (true)) { m->source = id - 1; m->setEnabled (true); }
            refresh();
        };
        tgt.onChange = [this] { if (auto* m = rt.monitorMatch (true)) { m->target = tgt.getSelectedId() - 1; m->setEnabled (true); } refresh(); };
        amt.onValueChange = [this] { if (auto* m = rt.monitorMatch (false)) m->amount = (float) amt.getValue() / 100.0f; repaint(); };
        lvl.onValueChange = [this] { if (auto* c = rt.controlRoom()) c->level = (float) lvl.getValue(); repaint(); };
        dimB.onClick  = [this] { if (auto* c = rt.controlRoom()) c->dim  = dimB.getToggleState(); };
        monoB.onClick = [this] { if (auto* c = rt.controlRoom()) c->mono = monoB.getToggleState(); };
        muteB.onClick = [this] { if (auto* c = rt.controlRoom()) c->mute = muteB.getToggleState(); };
    }

    void refresh()
    {
        if (! app.edit) return;
        auto* c = rt.controlRoom();
        if (c) { lvl.setValue (c->level.get(), juce::dontSendNotification); dimB.setToggleState (c->dim.get(), juce::dontSendNotification);
                 monoB.setToggleState (c->mono.get(), juce::dontSendNotification); muteB.setToggleState (c->mute.get(), juce::dontSendNotification); }
        auto* m = rt.monitorMatch (false);
        const bool on = m != nullptr && m->isEnabled();
        mmOn.setToggleState (on, juce::dontSendNotification);
        src.setSelectedId (m ? (m->source.get() >= (int) mm::sourceProfiles().size() ? 1000 : m->source.get() + 1) : 1, juce::dontSendNotification);
        if (m && m->source.get() >= (int) mm::sourceProfiles().size())
            src.setText (U ("O'lchov: ") + m->customName.get(), juce::dontSendNotification);
        tgt.setSelectedId (m ? m->target.get() + 1 : 2, juce::dontSendNotification);
        amt.setValue (m ? m->amount.get() * 100.0 : 100.0, juce::dontSendNotification);
        for (auto* w : std::initializer_list<juce::Component*> { &src, &tgt, &amt }) w->setAlpha (on ? 1.0f : 0.55f);
        repaint();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8, 6);
        auto right = r.removeFromRight (64);
        r.removeFromRight (6);
        r.removeFromTop (16);
        mmOn.setBounds (r.removeFromTop (20)); r.removeFromTop (4);
        src.setBounds (r.removeFromTop (20)); r.removeFromTop (3);
        tgt.setBounds (r.removeFromTop (20)); r.removeFromTop (3);
        amt.setBounds (r.removeFromTop (16)); r.removeFromTop (4);
        slotArea = r.removeFromTop (34).toFloat();
        curveArea = r.removeFromTop (juce::jmax (0, r.getHeight() - 2)).toFloat();
        auto bt = right.removeFromBottom (3 * 18);
        lvl.setBounds (right.withTrimmedTop (16).withTrimmedBottom (18).withSizeKeepingCentre (30, right.getHeight() - 34));
        dimB.setBounds (bt.removeFromTop (18).reduced (0, 1)); monoB.setBounds (bt.removeFromTop (18).reduced (0, 1)); muteB.setBounds (bt.removeFromTop (18).reduced (0, 1));
    }

    juce::Rectangle<float> crSlot (int i) const { return { slotArea.getX(), slotArea.getY() + (float) i * 17.0f, slotArea.getWidth(), 15.0f }; }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! app.edit) return;
        juce::Array<te::Plugin*> plugs;
        for (auto p : Routing::controlRoomInserts (*app.edit)) if (p->getPluginType() != MonitorMatchPlugin::xmlTypeName) plugs.add (p);
        for (int i = 0; i < 2; ++i)
        {
            if (! crSlot (i).contains (e.position)) continue;
            juce::Component::SafePointer<ControlRoomStrip> sp (this);
            if (i < plugs.size())
            {
                te::Plugin::Ptr p (plugs[i]);
                if (e.mods.isAltDown()) { p->setEnabled (! p->isEnabled()); repaint(); return; }
                if (! e.mods.isPopupMenu()) { p->showWindowExplicitly(); return; }
                juce::PopupMenu m; m.addSectionHeader (pluginDisplayName (*p));
                m.addItem (1, U ("Oynasini ochish")); m.addItem (2, p->isEnabled() ? "Bypass" : U ("Yoqish"));
                m.addSeparator(); m.addItem (3, U ("O'chirish"));
                m.showMenuAsync ({}, [sp, p] (int r)
                {
                    if (r == 1) p->showWindowExplicitly();
                    if (r == 2) p->setEnabled (! p->isEnabled());
                    if (r == 3) { p->edit.getUndoManager().beginNewTransaction ("Control Room insert"); p->deleteFromParent(); }
                    if (sp) sp->repaint();
                });
            }
            else
                showPluginPicker (app, localAreaToGlobal (crSlot (i).toNearestInt()), [sp, this] (const PickItem& it)
                { if (sp == nullptr) return; if (auto p = insertControlRoom (app, it)) p->showWindowExplicitly(); sp->repaint(); });
            return;
        }
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (cyan.withAlpha (0.035f)); g.fillRoundedRectangle (r, 10);
        g.setColour (cyan.withAlpha (0.3f)); g.drawRoundedRectangle (r, 10, 1);
        caps (g, "CONTROL ROOM", { 10, 5, r.getWidth() - 90, 12 }, 9.5f, cyan);
        label (g, String (lvl.getValue(), 1) + " dB", { r.getRight() - 72, 4, 64, 12 }, 9, silver, Justification::centred, 500);
        // CR insertlar
        juce::Array<te::Plugin*> plugs;
        if (app.edit) for (auto p : Routing::controlRoomInserts (*app.edit)) if (p->getPluginType() != MonitorMatchPlugin::xmlTypeName) plugs.add (p);
        for (int i = 0; i < 2; ++i)
        {
            auto s = crSlot (i);
            const bool has = i < plugs.size(), add = i == plugs.size();
            g.setColour (has ? cyan.withAlpha (plugs[i]->isEnabled() ? 0.16f : 0.06f) : Colours::white.withAlpha (add ? 0.05f : 0.025f)); g.fillRoundedRectangle (s, 4);
            label (g, has ? pluginDisplayName (*plugs[i]) : add ? U ("+ CR insert (faqat monitor)") : String::fromUTF8 ("\xe2\x80\x94"),
                   s.reduced (5, 0), 8.5f, has ? (plugs[i]->isEnabled() ? cyan : dim2) : add ? silver : dim2, Justification::centredLeft, has ? 500 : 400);
        }
        // korreksiya egri chizig'i (20 Hz – 20 kHz, ±12 dB)
        if (curveArea.getHeight() > 18)
        {
            auto c = curveArea.reduced (1);
            g.setColour (Colours::white.withAlpha (0.03f)); g.fillRoundedRectangle (c, 4);
            g.setColour (Colours::white.withAlpha (0.08f)); g.drawHorizontalLine ((int) c.getCentreY(), c.getX(), c.getRight());
            if (auto* m = app.edit ? Routing (app).monitorMatch (false) : nullptr)
            {
                Path p;
                for (int i = 0; i <= 80; ++i)
                {
                    const double hz = 20.0 * std::pow (1000.0, i / 80.0);
                    const float db = (float) jlimit (-12.0, 12.0, m->correctionDbAt (hz));
                    const float x = c.getX() + c.getWidth() * (float) i / 80.0f, y = c.getCentreY() - db / 12.0f * c.getHeight() * 0.5f;
                    if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
                }
                g.setColour (m->isEnabled() ? cyan : dim2); g.strokePath (p, PathStrokeType (1.4f));
            }
            label (g, "20", c.withTrimmedLeft (3), 7.5f, dim2, Justification::bottomLeft, 400);
            label (g, "20k", c.withTrimmedRight (3), 7.5f, dim2, Justification::bottomRight, 400);
        }
    }
private:
    void loadMeasurement()
    {
        chooser = std::make_unique<juce::FileChooser> (U ("Monitor o'lchovi (REW → Export measurement as text)"), juce::File(), "*.txt;*.frd;*.csv");
        juce::Component::SafePointer<ControlRoomStrip> sp (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [sp] (const juce::FileChooser& fc)
        {
            if (sp == nullptr) return;
            if (auto f = fc.getResult(); f.existsAsFile())
                if (auto* m = sp->rt.monitorMatch (true)) { m->loadMeasurement (f); m->setEnabled (true); }
            sp->refresh();
        });
    }
    static inline const juce::Colour cyan { 0xff4ad6ff };
    AppEngine& app; Routing rt;
    juce::TextButton mmOn, dimB, monoB, muteB;
    juce::ComboBox src, tgt; juce::Slider amt, lvl;
    juce::Rectangle<float> slotArea, curveArea;
    std::unique_ptr<juce::FileChooser> chooser;
};

class Mixer : public juce::Component, private juce::Timer
{
public:
    std::function<void (te::EditItemID, float)> onTrackLevel;
    std::function<void()> onDetach;          // alohida oynaga ajratish / qaytarish
    std::function<void()> onRoutingChanged;  // insert/send o'zgardi (inspektor yangilanadi)
    std::function<void()> onTracksReordered;
    void toggleSpectro (te::AudioTrack& t)
    {
        const bool open = ! (bool) t.state.getProperty ("azSpec", false);
        t.state.setProperty ("azSpec", open, nullptr);
        if (! open) removeScope (t);
        rebuild();
    }
    void rebuildIfNeeded()
    {
        if (! app.edit) return;
        auto ts = te::getAudioTracks (*app.edit);
        bool same = ts.size() == ids.size();
        for (int i = 0; same && i < ts.size(); ++i) same = ts[i]->itemID == ids[i];
        if (! same) rebuild(); else for (auto* s : strips) s->repaint();
    }
    bool detached = false;
    explicit Mixer (AppEngine& a) : app (a)
    {
        addAndMakeVisible (vp); vp.setViewedComponent (&holder, false); vp.setScrollBarsShown (false, true); startTimerHz (25);
        addAndMakeVisible (detachB); detachB.getProperties().set ("radius", 4.0f);
        detachB.setButtonText (juce::String::fromUTF8 ("\xe2\x86\x97"));
        detachB.setTooltip (U ("Mikserni alohida oynaga ajratish (ikkinchi displeyga olib o'tish mumkin). Chap chetdan sudrab ham ajratsa bo'ladi."));
        detachB.onClick = [this] { if (onDetach) onDetach(); };
    }
    void mouseDown (const juce::MouseEvent& e) override { dragFromEdge = e.x < 28 && ! detached; }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragFromEdge && e.getDistanceFromDragStart() > 40) { dragFromEdge = false; if (onDetach) onDetach(); }
    }
    void rebuild()
    {
        specs.clear(); strips.clear(); ids.clear(); master.reset(); croom.reset();
        if (! app.edit) return;
        for (auto* t : te::getAudioTracks (*app.edit))
        {
            auto* s = strips.add (new ChannelStrip (app, t)); holder.addAndMakeVisible (s); ids.add (t->itemID);
            s->onRoutingChanged = [this] { juce::Component::SafePointer<Mixer> sp (this); juce::MessageManager::callAsync ([sp] { if (sp) { sp->rebuildIfNeeded(); if (sp->onRoutingChanged) sp->onRoutingChanged(); } }); };
            s->spectroOpen = t->state.getProperty ("azSpec", false);
            te::Track::Ptr tp (t);
            s->onToggleSpectro = [this, tp] (ChannelStrip&) { juce::Component::SafePointer<Mixer> sp (this); juce::MessageManager::callAsync ([sp, tp] { if (sp) if (auto* at = dynamic_cast<te::AudioTrack*> (tp.get())) sp->toggleSpectro (*at); }); };
            s->onReorderDrag = [this] (ChannelStrip&, int x) { dropX = x; dropIndex = indexForX (x); holder.repaint(); repaint(); };
            s->onReorderEnd = [this, tp] (ChannelStrip&)
            {
                const int target = dropIndex; dropIndex = -1;
                juce::Component::SafePointer<Mixer> sp (this);
                juce::MessageManager::callAsync ([sp, tp, target]
                {
                    if (! sp) return;
                    if (auto* at = dynamic_cast<te::AudioTrack*> (tp.get())) if (sp->app.moveTrackTo (*at, target)) { sp->rebuild(); if (sp->onTracksReordered) sp->onTracksReordered(); }
                    sp->repaint();
                });
            };
            if (s->spectroOpen) { auto* sp = specs.add (new SpectroPanel (*t)); holder.addAndMakeVisible (sp); te::Track::Ptr tq (t);
                                  sp->onClose = [this, tq] { juce::Component::SafePointer<Mixer> m (this); juce::MessageManager::callAsync ([m, tq] { if (m) if (auto* at = dynamic_cast<te::AudioTrack*> (tq.get())) m->toggleSpectro (*at); }); }; }
        }
        master = std::make_unique<ChannelStrip> (app, nullptr); addAndMakeVisible (*master);
        croom = std::make_unique<ControlRoomStrip> (app); addAndMakeVisible (*croom);
        resized();
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (10, 8);
        detachB.setBounds (4, 6, 22, 20);
        r.removeFromLeft (18);
        if (croom) { croom->setBounds (r.removeFromRight (250)); r.removeFromRight (8); }
        if (master) master->setBounds (r.removeFromRight (104));
        r.removeFromRight (10);
        vp.setBounds (r);
        int x = 0;
        stripX.clear();
        for (int i = 0; i < strips.size(); ++i)
        {
            stripX.add (x);
            strips[i]->setBounds (x, 0, 92, r.getHeight() - 8); x += 98;
            for (auto* sp : specs) if (sp->getTrack() == strips[i]->getTrack()) { sp->setBounds (x - 2, 0, 330, r.getHeight() - 8); x += 336; }
        }
        stripX.add (x);
        holder.setSize (juce::jmax (r.getWidth(), x), r.getHeight() - 8);
    }
    void paint (juce::Graphics& g) override
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0e0c10), 0, 0, juce::Colour (0xff09080b), 0, (float) getHeight(), false)); g.fillAll();
        g.setColour (line); g.drawHorizontalLine (0, 0, (float) getWidth());
        juce::Graphics::ScopedSaveState s (g);
        g.setColour (juce::Colours::white.withAlpha (0.025f)); g.fillRect (0, 30, 26, getHeight() - 36);
        g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, 18, (float) getHeight() * 0.5f));
        caps (g, detached ? "MIKSER" : U ("MIKSER  \xe2\x87\xa1 sudrang"), { 18 - 60, (float) getHeight() * 0.5f - 8, 120, 12 }, 9.5f, dim2, juce::Justification::centred);
    }
private:
    void timerCallback() override
    {
        for (int i = 0; i < strips.size(); ++i) { strips[i]->tick(); if (onTrackLevel) onTrackLevel (ids[i], strips[i]->level()); }
        if (master) master->tick();
    }
    AppEngine& app; juce::Viewport vp; juce::Component holder;
    juce::OwnedArray<ChannelStrip> strips; juce::Array<te::EditItemID> ids; std::unique_ptr<ChannelStrip> master;
    std::unique_ptr<ControlRoomStrip> croom;
    juce::TextButton detachB; bool dragFromEdge = false;
    juce::OwnedArray<SpectroPanel> specs;
    juce::Array<int> stripX; int dropIndex = -1, dropX = 0;
    int indexForX (int x) const
    {
        for (int i = 0; i < strips.size(); ++i) if (x < stripX[i] + 46) return i;
        return strips.size();
    }
    void paintOverChildren (juce::Graphics& g) override
    {
        if (dropIndex < 0) return;
        const int hx = dropIndex < stripX.size() ? stripX[dropIndex] - 3 : 0;
        auto p = vp.getBounds().getPosition() + juce::Point<int> (hx - vp.getViewPositionX(), 0);
        g.setColour (juce::Colour (0xffff9a4a)); g.fillRoundedRectangle ((float) p.x - 1.5f, (float) p.y, 3.0f, (float) vp.getHeight() - 8, 1.5f);
    }
public:
    ControlRoomStrip* controlRoomStrip() { return croom.get(); }
};

// =============================================================================  Media brauzer
class MediaBrowser : public juce::Component, private juce::FileBrowserListener, private juce::ChangeListener, private juce::Timer
{
public:
    std::function<void (const juce::File&)> onInsertFile;
    bool syncBpm() const { return bpmB.getToggleState(); }
    bool syncKey() const { return keyB.getToggleState(); }
    double projBpm() const { return app.edit ? app.edit->tempoSequence.getTempo (0)->getBpm() : 120.0; }
    sync::KeyInfo projKey() const { return app.edit ? sync::keyFromString (app.edit->state.getProperty ("azKey", "A minor").toString()) : sync::KeyInfo {}; }

    // trekka qo'yilgan audio klipni loyiha BPM/tonalligiga moslash (yoqilgan bo'lsa)
    juce::String applySync (te::WaveAudioClip& c, const juce::File& f)
    {
        juce::String msg;
        if (autoLevel())
        {
            juce::AudioBuffer<float> b; sync::analyse (app.engine, f, projBpm(), &b);
            sampleRmsDb = activeRmsDb (b);
            const float g = computeAutoDb();
            c.setGainDB (g);
            msg << "gain " << (g > 0 ? "+" : "") << juce::String (g, 1) << " dB";
        }
        if (! syncBpm() && ! syncKey()) return msg;
        auto fi = sync::analyse (app.engine, f, projBpm());
        const auto arrow = juce::String::fromUTF8 (" \xe2\x86\x92 "), dot = juce::String::fromUTF8 ("  \xc2\xb7  ");
        if (syncBpm() && fi.loop && fi.bpm > 0)
        {
            c.setTimeStretchMode (te::TimeStretcher::soundtouchBetter);
            c.getLoopInfo().setBpm (fi.bpm, c.getAudioFile().getInfo());
            c.setAutoTempo (true);
            const double beats = fi.lengthSec * fi.bpm / 60.0;
            c.setLength (te::TimeDuration::fromSeconds (beats * 60.0 / projBpm()), true);
            msg << (msg.isEmpty() ? juce::String() : dot) << juce::String (fi.bpm, 1) << arrow << juce::String (projBpm(), 1) << " BPM";
        }
        if (syncKey() && ! fi.drum && fi.key.valid())
        {
            const int st = sync::semitoneShift (fi.key, projKey());
            if (st != 0) { if (c.getTimeStretchMode() == te::TimeStretcher::disabled) c.setTimeStretchMode (te::TimeStretcher::soundtouchBetter); c.setPitchChange ((float) st); }
            msg << (msg.isEmpty() ? juce::String() : dot) << sync::keyName (fi.key) << arrow << sync::keyName (projKey()) << " (" << (st > 0 ? "+" : "") << st << " st)";
        }
        return msg;
    }

    explicit MediaBrowser (AppEngine& a) : app (a)
    {
        thread.startThread (juce::Thread::Priority::low);
        list = std::make_unique<juce::DirectoryContentsList> (&filter, thread);
        tree = std::make_unique<juce::FileTreeComponent> (*list);
        tree->setDragAndDropDescription ("azfile");
        tree->addListener (this);
        tree->setItemHeight (26);
        addAndMakeVisible (*tree);
        const auto saved = app.engine.getPropertyStorage().getPropertiesFile().getValue ("azMediaRoot");
        auto music = juce::File::getSpecialLocation (juce::File::userMusicDirectory);
        setRoot (saved.isNotEmpty() && juce::File (saved).isDirectory() ? juce::File (saved) : music.isDirectory() ? music : AppEngine::projectsFolder());
        for (auto* b : { &bMusic, &bDocs, &bProj, &bPick }) { addAndMakeVisible (b); b->getProperties().set ("radius", 6.0f); }
        bMusic.onClick = [this] { setRoot (juce::File::getSpecialLocation (juce::File::userMusicDirectory)); };
        bDocs.onClick  = [this] { setRoot (juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)); };
        bProj.onClick  = [this] { setRoot (AppEngine::projectsFolder()); };
        bPick.onClick  = [this]
        {
            chooser = std::make_unique<juce::FileChooser> (U ("Sample papkasini tanlang"), root);
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                  [this] (const juce::FileChooser& fc) { if (fc.getResult().isDirectory()) setRoot (fc.getResult()); });
        };
        formats.registerBasicFormats();
        auto& pf = app.engine.getPropertyStorage().getPropertiesFile();
        for (auto* b : { &bpmB, &keyB }) { addAndMakeVisible (b); b->setClickingTogglesState (true); b->getProperties().set ("radius", 5.0f); b->setColour (juce::TextButton::buttonOnColourId, green.withAlpha (0.35f)); }
        bpmB.setButtonText ("BPM SYNC"); keyB.setButtonText ("KEY SYNC");
        bpmB.setTooltip (U ("Loop'larni loyiha tempiga moslab tinglash va trekka qo'yish"));
        keyB.setTooltip (U ("Sample/loop'larni loyiha tonalligiga moslash (perkussiyaga ta'sir qilmaydi)"));
        bpmB.setToggleState (pf.getBoolValue ("azSyncBpm", true), juce::dontSendNotification);
        keyB.setToggleState (pf.getBoolValue ("azSyncKey", true), juce::dontSendNotification);
        bpmB.onClick = [this] { app.engine.getPropertyStorage().getPropertiesFile().setValue ("azSyncBpm", bpmB.getToggleState()); if (previewFile.existsAsFile()) startPreview (previewFile); };
        keyB.onClick = [this] { app.engine.getPropertyStorage().getPropertiesFile().setValue ("azSyncKey", keyB.getToggleState()); if (previewFile.existsAsFile()) startPreview (previewFile); };
        // tinglash balandligi: qo'lda (dB) + AUTO LVL (DAW mix balandligiga moslash)
        addAndMakeVisible (volS); addAndMakeVisible (autoB);
        volS.setSliderStyle (juce::Slider::LinearBar); volS.setRange (-30.0, 12.0, 0.5); volS.setDoubleClickReturnValue (true, 0.0);
        volS.setTextValueSuffix (U (" dB tinglash")); volS.setValue (pf.getDoubleValue ("azPrevVol", 0.0), juce::dontSendNotification);
        volS.setColour (juce::Slider::trackColourId, red.withAlpha (0.3f));
        volS.setTooltip (U ("Kutubxonadan tinglash balandligi (2x bosish = 0 dB)"));
        volS.onValueChange = [this] { app.engine.getPropertyStorage().getPropertiesFile().setValue ("azPrevVol", volS.getValue()); applyGain(); };
        autoB.setButtonText ("AUTO LVL"); autoB.setClickingTogglesState (true); autoB.getProperties().set ("radius", 5.0f);
        autoB.setColour (juce::TextButton::buttonOnColourId, green.withAlpha (0.35f));
        autoB.setToggleState (pf.getBoolValue ("azPrevAuto", true), juce::dontSendNotification);
        autoB.setTooltip (U ("Sample/loop balandligini DAW'dagi mix balandligiga real-time moslash (trekka qo'yganda klip gain ham shunga o'rnatiladi)"));
        autoB.onClick = [this] { app.engine.getPropertyStorage().getPropertiesFile().setValue ("azPrevAuto", autoB.getToggleState()); applyGain(); };
        app.engine.getDeviceManager().deviceManager.addAudioCallback (&player);
        player.setSource (&transport);
        transport.addChangeListener (this);
        startTimerHz (15);
    }
    // ---- AUTO LVL
    bool autoLevel() const { return autoB.getToggleState(); }
    float autoGainDb = 0;            // hozirgi avtomatik tuzatish
    float sampleRmsDb = -200;        // tanlangan sample'ning (faol qismi) RMS
    float masterPeakDb = -200;       // DAW master (silliqlangan)
    // maqsad: mix ishlayotgan bo'lsa — mix RMS'idan 3 dB past, jim bo'lsa — -20 dBFS RMS
    float targetRmsDb() const { return masterPeakDb > -60.0f ? (masterPeakDb - 9.0f) - 3.0f : -20.0f; }
    float computeAutoDb() const { return sampleRmsDb < -100 ? 0.0f : juce::jlimit (-18.0f, 18.0f, targetRmsDb() - sampleRmsDb); }
    void applyGain()
    {
        const float db = (float) volS.getValue() + (autoLevel() ? autoGainDb : 0.0f);
        transport.setGain (juce::Decibels::decibelsToGain (db));
        repaint();
    }
    static float activeRmsDb (const juce::AudioBuffer<float>& b)
    {
        // jim joylarsiz RMS (50 ms bloklar, eng past 40% tashlanadi)
        const int blk = 2400; std::vector<double> e;
        for (int s = 0; s + blk <= b.getNumSamples(); s += blk)
        { double sum = 0; for (int c = 0; c < b.getNumChannels(); ++c) { auto* d = b.getReadPointer (c, s); for (int i = 0; i < blk; ++i) sum += d[i] * d[i]; } e.push_back (sum / (blk * b.getNumChannels())); }
        if (e.empty()) { const float r = b.getNumSamples() > 0 ? b.getRMSLevel (0, 0, b.getNumSamples()) : 0.0f; return juce::Decibels::gainToDecibels (r, -200.0f); }
        std::sort (e.begin(), e.end());
        double sum = 0; int n = 0; for (size_t i = e.size() * 4 / 10; i < e.size(); ++i) { sum += e[i]; ++n; }
        return (float) (10.0 * std::log10 (juce::jmax (1e-12, sum / juce::jmax (1, n))));
    }
    void timerCallback() override
    {
        // master darajasini o'qish
        if (app.edit)
            if (auto* ctx = app.edit->getCurrentPlaybackContext())
            {
                if (measured != &ctx->masterLevels) { if (measured) measured->removeClient (client); ctx->masterLevels.addClient (client); measured = &ctx->masterLevels; }
                float pk = -200; for (int c = 0; c < 2; ++c) pk = juce::jmax (pk, client.getAndClearAudioLevel (c).dB);
                masterPeakDb = pk > masterPeakDb ? masterPeakDb + (pk - masterPeakDb) * 0.3f : juce::jmax (-200.0f, masterPeakDb - 0.6f);
            }
        if (autoLevel())
        {
            const float want = computeAutoDb();
            if (std::abs (want - autoGainDb) > 0.05f) { autoGainDb += (want - autoGainDb) * 0.25f; applyGain(); }
        }
    }
    ~MediaBrowser() override
    {
        stopTimer();
        if (measured) measured->removeClient (client);
        transport.stop(); transport.setSource (nullptr); player.setSource (nullptr);
        app.engine.getDeviceManager().deviceManager.removeAudioCallback (&player);
        tree->removeListener (this);
    }
    void setRoot (const juce::File& f)
    {
        root = f; list->setDirectory (f, true, true);
        app.engine.getPropertyStorage().getPropertiesFile().setValue ("azMediaRoot", f.getFullPathName());
        repaint();
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        r.removeFromTop (24);
        auto row = r.removeFromTop (28); const int w = (row.getWidth() - 18) / 4;
        for (auto* b : { &bMusic, &bDocs, &bProj, &bPick }) { b->setBounds (row.removeFromLeft (w)); row.removeFromLeft (6); }
        r.removeFromTop (26);
        auto prev = r.removeFromBottom (152);
        auto sb = prev.removeFromTop (24); const int bw = (sb.getWidth() - 6) / 2;
        bpmB.setBounds (sb.removeFromLeft (bw)); sb.removeFromLeft (6); keyB.setBounds (sb);
        prev.removeFromTop (4);
        auto vr = prev.removeFromTop (22);
        autoB.setBounds (vr.removeFromLeft (80)); vr.removeFromLeft (6); volS.setBounds (vr);
        tree->setBounds (r);
    }
    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        g.fillAll (bg1);
        g.setColour (line); g.drawVerticalLine (0, 0, (float) getHeight());
        caps (g, "MEDIA BRAUZER", { 14, 12, 200, 12 });
        label (g, root.getFullPathName(), { 14, 70, (float) getWidth() - 28, 16 }, 10, dim2, Justification::centredLeft, 400);
        auto pr = Rectangle<float> (12, (float) getHeight() - 104, (float) getWidth() - 24, 92);
        caps (g, "OLDINDAN TINGLASH", pr.withHeight (12));
        auto box = pr.withTrimmedTop (18);
        panel (g, box, 8);
        if (previewFile.existsAsFile())
        {
            label (g, previewFile.getFileName(), box.reduced (10, 6).withHeight (14), 11, text, Justification::centredLeft, 500);
            const double len = transport.getLengthInSeconds(), pos = transport.getCurrentPosition();
            auto bar = Rectangle<float> (box.getX() + 10, box.getBottom() - 16, box.getWidth() - 20, 4);
            g.setColour (Colours::white.withAlpha (0.07f)); g.fillRoundedRectangle (bar, 2);
            g.setColour (red); g.fillRoundedRectangle (bar.withWidth (len > 0 ? bar.getWidth() * (float) (pos / len) : 0), 2);
            caps (g, transport.isPlaying() ? U ("\xe2\x96\xa0 TO'XTATISH UCHUN BOSING") : U ("2X BOSING: TREKKA QO'SHISH"), box.reduced (10, 0).withTrimmedTop (22).withHeight (12), 8.5f, dim);
            if (syncInfo.isNotEmpty()) label (g, syncInfo, box.reduced (10, 0).withTrimmedTop (36).withHeight (14), 9.5f, green, Justification::centredLeft, 600);
            if (autoLevel() && sampleRmsDb > -100)
                label (g, "AUTO LVL " + String (autoGainDb > 0 ? "+" : "") + String (autoGainDb, 1) + " dB" + (masterPeakDb > -60 ? U (" \xc2\xb7 mix'ga") : U (" \xc2\xb7 -20 dBFS")),
                       box.reduced (10, 0).withTrimmedTop (36).withHeight (14), 9, yellow, Justification::centredRight, 600);
        }
        else label (g, U ("Faylni bir marta bosing \xe2\x80\x94 eshitasiz, 2x \xe2\x80\x94 trekka qo'shiladi, sudrab tashlash ham mumkin"),
                    box.reduced (10, 6), 10.5f, dim, Justification::centredLeft, 400);
    }
    void mouseDown (const juce::MouseEvent&) override { transport.stop(); repaint(); }

private:
    void selectionChanged() override {}
    void fileClicked (const juce::File& f, const juce::MouseEvent&) override
    {
        if (f.isDirectory()) return;
        if (f == previewFile && transport.isPlaying()) { transport.stop(); repaint(); return; }
        startPreview (f);
    }
    void startPreview (const juce::File& f)
    {
        transport.stop(); transport.setSource (nullptr); reader.reset(); memSrc.reset();
        previewFile = f; syncInfo = {};
        std::unique_ptr<juce::AudioFormatReader> r (formats.createReaderFor (f));
        if (r == nullptr) { repaint(); return; }
        const double sr = r->sampleRate, len = (double) r->lengthInSamples / sr;
        if (len < 90.0 && ! f.hasFileExtension ("mid;midi"))
        {
            auto fi = sync::analyse (app.engine, f, projBpm(), &memBuf);
            sampleRmsDb = activeRmsDb (memBuf);
            autoGainDb = computeAutoDb();
            double speed = 1.0, semis = 0.0; juce::String inf;
            const auto arrow = juce::String::fromUTF8 (" \xe2\x86\x92 "), dot = juce::String::fromUTF8 ("  \xc2\xb7  ");
            if (syncBpm() && fi.loop && fi.bpm > 0 && (syncBpm() || syncKey())) { speed = fi.bpm / projBpm(); inf << juce::String (fi.bpm, 1) << (fi.bpmGuessed ? "?" : "") << arrow << juce::String (projBpm(), 1) << " BPM"; }
            if (syncKey() && ! fi.drum && fi.key.valid())
            {
                semis = sync::semitoneShift (fi.key, projKey());
                inf << (inf.isEmpty() ? juce::String() : dot) << sync::keyName (fi.key) << (fi.key.guessed ? "?" : "") << arrow << sync::keyName (projKey())
                    << " (" << (semis > 0 ? "+" : "") << (int) semis << " st)";
            }
            else if (fi.drum) inf << (inf.isEmpty() ? juce::String() : dot) << "perkussiya";
            syncInfo = inf;
            if (std::abs (speed - 1.0) > 1e-3 || std::abs (semis) > 1e-3) memBuf = sync::stretch (memBuf, fi.sampleRate, speed, semis);
            memSrc = std::make_unique<juce::MemoryAudioSource> (memBuf, false, fi.loop);
            transport.setSource (memSrc.get(), 0, nullptr, fi.sampleRate);
        }
        else
        {
            reader = std::make_unique<juce::AudioFormatReaderSource> (r.release(), true);
            transport.setSource (reader.get(), 0, nullptr, sr);
            sampleRmsDb = -200;
        }
        applyGain();
        transport.setPosition (0); transport.start();
        repaint();
    }
    void fileDoubleClicked (const juce::File& f) override { if (! f.isDirectory() && onInsertFile) { transport.stop(); onInsertFile (f); } }
    void browserRootChanged (const juce::File&) override {}
    void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint(); }

    AppEngine& app; juce::File root, previewFile;
    juce::WildcardFileFilter filter { "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg;*.mid;*.midi", "*", "Audio" };
    juce::TimeSliceThread thread { "AZ media" };
    std::unique_ptr<juce::DirectoryContentsList> list;
    std::unique_ptr<juce::FileTreeComponent> tree;
    juce::TextButton bMusic { "Musiqa" }, bDocs { "Hujjatlar" }, bProj { "Loyihalar" }, bPick { "Papka..." };
    std::unique_ptr<juce::FileChooser> chooser;
    juce::AudioFormatManager formats; juce::AudioSourcePlayer player; juce::AudioTransportSource transport;
    std::unique_ptr<juce::AudioFormatReaderSource> reader;
    juce::AudioBuffer<float> memBuf; std::unique_ptr<juce::MemoryAudioSource> memSrc; juce::String syncInfo;
    juce::TextButton bpmB, keyB, autoB; juce::Slider volS;
    te::LevelMeasurer::Client client; te::LevelMeasurer* measured = nullptr;
};

// =============================================================================  Holat qatori
class StatusBar : public juce::Component
{
public:
    explicit StatusBar (AppEngine& a) : app (a) {}
    juce::String message; juce::uint32 messageTime = 0;
    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        g.fillAll (Colour (0xff060608));
        g.setColour (line); g.drawHorizontalLine (0, 0, (float) getWidth());
        auto& dm = app.engine.getDeviceManager();
        auto* dev = dm.deviceManager.getCurrentAudioDevice();
        float x = 16;
        auto item = [&] (const String& s, Colour c)
        {
            const float w = textWidth (font (10, 500, 0.1f), s) + 26;
            label (g, s, { x, 0, w, (float) getHeight() }, 10, c, Justification::centredLeft, 500, 0.1f); x += w;
        };
        if (dev) item (U ("\xe2\x97\x8f ") + dev->getName() + U (" \xc2\xb7 ") + String ((int) dev->getCurrentSampleRate()) + U (" Hz \xc2\xb7 ")
                       + String (dev->getCurrentBufferSizeSamples()) + " smp", green);
        else item (U ("\xe2\x97\x8f Audio qurilma tanlanmagan \xe2\x80\x94 Audio \xe2\x86\x92 Sozlamalar"), red);
        if (app.edit)
        {
            item (U ("Loyiha: ") + app.editFile.getFileName(), dim);
            item (U ("Treklar: ") + String (te::getAudioTracks (*app.edit).size()), dim);
            item (U ("Saqlangan: ") + (app.lastSave.toMilliseconds() > 0 ? app.lastSave.toString (false, true, false, true) : U ("\xe2\x80\x94")), dim);
        }
        if (message.isNotEmpty() && juce::Time::getMillisecondCounter() - messageTime < 7000)
        {
            const float w = textWidth (font (11, 500), message) + 30;
            auto r = Rectangle<float> (jmax (x + 10, (float) getWidth() * 0.5f - w * 0.5f), 3, w, (float) getHeight() - 6);
            g.setColour (red.withAlpha (0.14f)); g.fillRoundedRectangle (r, 6);
            g.setColour (red.withAlpha (0.4f)); g.drawRoundedRectangle (r, 6, 1);
            g.setColour (Colours::white); g.setFont (font (11, 500)); g.drawText (message, r, Justification::centred, true);
            return;
        }
        label (g, "AZ MUS " + String (JUCE_APPLICATION_VERSION_STRING) + U (" \xc2\xb7 Azimxodjayev Music \xc2\xb7 Powered by Tracktion Engine"),
               getLocalBounds().toFloat().withTrimmedRight (16), 10, dim2, Justification::centredRight, 500, 0.08f);
    }
private:
    AppEngine& app;
};
}
