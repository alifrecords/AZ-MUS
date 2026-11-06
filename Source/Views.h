// =============================================================================
//  AZ MUS — panellar: yuqori panel, inspektor, mikser, media brauzer, holat qatori
// =============================================================================
#pragma once
#include "Arrangement.h"
#include "AirwindowsList.h"

namespace az
{
inline juce::TextButton* iconButton (juce::TextButton& b, const char* icon, const char* tip, juce::Colour accent = red)
{
    b.setButtonText (icon); b.setTooltip (juce::String::fromUTF8 (tip));
    b.getProperties().set ("accent", (juce::int64) accent.getARGB());
    return &b;
}

// =============================================================================  LCD
class LcdDisplay : public juce::Component
{
public:
    explicit LcdDisplay (AppEngine& a) : app (a) {}
    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (Colour (0xff050507)); g.fillRoundedRectangle (r, 10);
        g.setColour (Colours::white.withAlpha (0.08f)); g.drawRoundedRectangle (r, 10, 1);
        if (! app.edit) return;
        auto& ts = app.edit->tempoSequence;
        caps (g, "POZITSIYA", { 14, 4, 120, 10 }, 8.5f);
        auto pos = app.positionString();
        g.setFont (font (21, 700, 0.04f));
        float x = 14;
        for (int i = 0; i < pos.length(); ++i)
        {
            const auto ch = String::charToString (pos[i]);
            g.setColour (ch == "." ? red : Colours::white);
            const float w = textWidth (font (21, 700, 0.04f), ch) + 1.5f;
            g.drawText (ch, Rectangle<float> (x, 14, w + 4, 24), Justification::centredLeft); x += w;
        }
        cells.clear();
        const String ks[] = { "VAQT", "TEMP", U ("O'LCHOV"), "TONALLIK" };
        const auto sig = ts.getTimeSig (0);
        const String vs[] = { app.timeString(), String (ts.getTempo (0)->getBpm(), 2), String (sig->numerator) + "/" + String (sig->denominator),
                              app.edit->state.getProperty ("azKey", "A minor").toString() };
        float cx = x + 22;
        for (int i = 0; i < 4; ++i)
        {
            const float w = i == 0 ? 82.0f : i == 3 ? 70.0f : 56.0f;
            caps (g, ks[i], { cx, 4, w, 10 }, 8.5f);
            label (g, vs[i], { cx, 16, w + 10, 20 }, 13, text, Justification::centredLeft, 600);
            cells.add ({ cx, 0, w, (float) getHeight() });
            cx += w + 14;
        }
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! app.edit) return;
        dragBpm = app.edit->tempoSequence.getTempo (0)->getBpm();
        if (cells.size() == 4 && cells[2].contains (e.position)) showSigMenu();
        if (cells.size() == 4 && cells[3].contains (e.position)) showKeyMenu();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! app.edit || cells.size() < 4 || ! cells[1].contains (e.mouseDownPosition)) return;
        const double bpm = juce::jlimit (30.0, 300.0, std::round ((dragBpm - e.getDistanceFromDragStartY() * 0.25) * 10) / 10);
        app.edit->tempoSequence.getTempo (0)->setBpm (bpm); repaint();
    }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (! app.edit || cells.size() < 4 || ! cells[1].contains (e.position)) return;
        auto* w = new juce::AlertWindow ("Temp (BPM)", {}, juce::MessageBoxIconType::NoIcon);
        w->addTextEditor ("b", juce::String (app.edit->tempoSequence.getTempo (0)->getBpm(), 2));
        w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey)); w->addButton (U ("Bekor"), 0);
        w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int r)
        { if (r) app.edit->tempoSequence.getTempo (0)->setBpm (juce::jlimit (30.0, 300.0, w->getTextEditorContents ("b").getDoubleValue())); repaint(); }), true);
    }
    void showSigMenu()
    {
        juce::PopupMenu m; const int sigs[][2] = { { 4, 4 }, { 3, 4 }, { 6, 8 }, { 2, 4 }, { 5, 4 }, { 7, 8 }, { 12, 8 } };
        for (int i = 0; i < 7; ++i) m.addItem (i + 1, juce::String (sigs[i][0]) + "/" + juce::String (sigs[i][1]));
        m.showMenuAsync ({}, [this, sigs] (int r) { if (r) { auto s = app.edit->tempoSequence.getTimeSig (0); s->numerator = sigs[r - 1][0]; s->denominator = sigs[r - 1][1]; repaint(); } });
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
    AppEngine& app; double dragBpm = 120; juce::Array<juce::Rectangle<float>> cells;
};

// =============================================================================  Yuqori panel
class TopBar : public juce::Component
{
public:
    TopBar (AppEngine& a, juce::MenuBarModel* model) : app (a), menu (model), lcd (a)
    {
        addAndMakeVisible (menu); addAndMakeVisible (lcd);
        iconButton (startB, "#start", "Boshiga (Home)"); iconButton (rewB, "#rew", "Orqaga"); iconButton (ffB, "#ff", "Oldinga");
        iconButton (stopB, "#stop", "To'xtatish"); iconButton (playB, "#play", "Ijro (Space)", green); iconButton (recB, "#rec", "Yozish (R)", red);
        iconButton (loopB, "#loop", "Loop (L)"); iconButton (clickB, "#click", "Metronom (C)");
        for (auto* b : { &startB, &rewB, &ffB, &stopB, &playB, &recB, &loopB, &clickB }) addAndMakeVisible (b);
        recB.getProperties().set ("accent", (juce::int64) red.getARGB());
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (14, 0);
        r.removeFromLeft (196);
        menu.setBounds (r.removeFromLeft (440).withSizeKeepingCentre (440, 30));
        auto right = r.removeFromRight (170);
        juce::ignoreUnused (right);
        lcd.setBounds (r.removeFromRight (480).withSizeKeepingCentre (480, 42));
        r.removeFromRight (14);
        for (auto* b : { &clickB, &loopB, &recB, &playB, &stopB, &ffB, &rewB, &startB })
        { b->setBounds (r.removeFromRight (34).withSizeKeepingCentre (34, 30)); r.removeFromRight (6); }
    }
    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        g.setGradientFill (ColourGradient (Colour (0xff0f0d10), 0, 0, Colour (0xff0a090c), 0, (float) getHeight(), false)); g.fillAll();
        g.setColour (line); g.drawHorizontalLine (getHeight() - 1, 0, (float) getWidth());
        logoMark (g, { 16, (float) getHeight() * 0.5f - 16, 32, 32 });
        g.setFont (font (17, 700, 0.08f)); g.setColour (text); g.drawText ("AZ", Rectangle<float> (58, 10, 40, 20), Justification::centredLeft);
        g.setColour (red); g.drawText ("MUS", Rectangle<float> (58 + textWidth (font (17, 700, 0.08f), "AZ "), 10, 60, 20), Justification::centredLeft);
        label (g, "AZIMXODJAYEV MUSIC", { 58, 31, 150, 12 }, 8, dim, Justification::centredLeft, 500, 0.26f);
        // CPU / kechikish
        const float x = (float) getWidth() - 180;
        caps (g, "CPU", { x, (float) getHeight() * 0.5f - 6, 30, 12 }, 9.5f, dim);
        auto bar = Rectangle<float> (x + 32, (float) getHeight() * 0.5f - 2.5f, 42, 5);
        g.setColour (Colours::white.withAlpha (0.07f)); g.fillRoundedRectangle (bar, 2.5f);
        const float cpu = jlimit (0.0f, 1.0f, app.engine.getDeviceManager().getCpuUsage());
        g.setColour (cpu < 0.5f ? green : cpu < 0.8f ? yellow : red); g.fillRoundedRectangle (bar.withWidth (jmax (3.0f, 42 * cpu)), 2.5f);
        auto& dm = app.engine.getDeviceManager();
        const double lat = dm.getBlockSizeMs();
        auto* dev = dm.deviceManager.getCurrentAudioDevice();
        const String type = dev ? dm.deviceManager.getCurrentAudioDeviceType() : "Audio yo'q";
        caps (g, type.toUpperCase() + (dev ? U (" \xc2\xb7 ") + String (lat, 1) + " ms" : String()), { x + 82, (float) getHeight() * 0.5f - 7, 110, 14 }, 9.5f, dev ? green : red);
    }
    void refreshState()
    {
        if (! app.edit) return;
        auto& t = app.edit->getTransport();
        playB.setToggleState (t.isPlaying(), juce::dontSendNotification);
        recB.setToggleState (t.isRecording(), juce::dontSendNotification);
        loopB.setToggleState (t.looping, juce::dontSendNotification);
        clickB.setToggleState (app.edit->clickTrackEnabled, juce::dontSendNotification);
        lcd.repaint(); repaint (getWidth() - 190, 0, 190, getHeight());
    }
    juce::TextButton startB, rewB, ffB, stopB, playB, recB, loopB, clickB;
private:
    AppEngine& app; juce::MenuBarComponent menu; LcdDisplay lcd;
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
        startTimerHz (4);
    }
    te::AudioTrack* track() const { return app.edit ? findTrack (*app.edit, vs.selectedTrack) : nullptr; }

    void refresh()
    {
        auto* t = track();
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

    void resized() override
    {
        const int y = 112 + 32 * slotCount() + 40;
        vol.setBounds (18, y + 20, 56, 56); pan.setBounds (88, y + 20, 56, 56);
        input.setBounds (12, y + 128, getWidth() - 24, 28);
    }
    int slotCount() const { auto* t = track(); return t ? AppEngine::userPluginCount (*t) + 1 : 0; }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        g.fillAll (bg1);
        g.setColour (line); g.drawVerticalLine (getWidth() - 1, 0, (float) getHeight());
        caps (g, "INSPEKTOR", { 14, 12, 200, 12 });
        auto* t = track();
        if (! t) { label (g, U ("Trek tanlang"), { 14, 40, 200, 20 }, 12, dim); return; }
        auto nr = Rectangle<float> (12, 32, (float) getWidth() - 24, 38);
        g.setColour (t->getColour().withAlpha (0.1f)); g.fillRoundedRectangle (nr, 9);
        g.setColour (t->getColour().withAlpha (0.4f)); g.drawRoundedRectangle (nr, 9, 1);
        g.setColour (t->getColour()); g.fillRoundedRectangle (nr.getX() + 10, nr.getCentreY() - 4, 8, 8, 2);
        label (g, t->getName(), nr.withTrimmedLeft (26).withTrimmedRight (70), 13, text, Justification::centredLeft, 700);
        caps (g, AppEngine::isInstrumentTrack (*t) ? "INSTR" : "AUDIO", nr.withTrimmedRight (10), 9, dim, Justification::centredRight);

        caps (g, "INSERTLAR", { 14, 84, 200, 12 });
        auto plugs = AppEngine::userPlugins (*t);
        slots.clear();
        for (int i = 0; i <= plugs.size(); ++i)
        {
            auto r = Rectangle<float> (12, 104.0f + (float) i * 32, (float) getWidth() - 24, 28);
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
        const float y = 112.0f + 32.0f * (float) slotCount() + 40;
        caps (g, "KANAL", { 14, y, 200, 12 });
        caps (g, "VOLUME", { 10, y + 80, 72, 12 }, 8.5f, dim, Justification::centred);
        caps (g, "PAN", { 80, y + 80, 72, 12 }, 8.5f, dim, Justification::centred);
        label (g, String (vol.getValue(), 1) + " dB", { 150, y + 30, 80, 14 }, 12, text, Justification::centredLeft, 600);
        const double pv = pan.getValue();
        label (g, std::abs (pv) < 0.01 ? String ("C") : (pv < 0 ? "L" : "R") + String (roundToInt (std::abs (pv) * 100)), { 150, y + 48, 80, 14 }, 12, dim, Justification::centredLeft, 500);
        if (input.isVisible()) caps (g, "AUDIO KIRISH", { 14, y + 110, 200, 12 });
        else label (g, U ("MIDI: barcha klaviaturalar \xe2\x86\x92 shu trek"), { 14, y + 110, (float) getWidth() - 20, 14 }, 10.5f, dim);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        auto* t = track(); if (! t) return;
        auto plugs = AppEngine::userPlugins (*t);
        for (int i = 0; i < slots.size(); ++i)
            if (slots[i].contains (e.position))
            {
                if (i == plugs.size()) { showPluginMenu (app, *t, [this] { refresh(); }); return; }
                te::Plugin::Ptr p (plugs[i]);
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
private:
    void timerCallback() override { const int n = slotCount(); if (n != lastSlots) { lastSlots = n; refresh(); } }
    AppEngine& app; ViewState& vs;
    juce::Slider vol, pan; juce::ComboBox input;
    juce::Array<juce::Rectangle<float>> slots; int lastSlots = -1;
};

// =============================================================================  Mikser
class ChannelStrip : public juce::Component
{
public:
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

    void resized() override
    {
        pan.setBounds (getWidth() / 2 - 17, 64, 34, 34);
        fader.setBounds (8, 104, 30, getHeight() - 150);
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
        if (ch) repaint (40, 104, 30, getHeight() - 150);
    }
    float level() const { return juce::jmax (lv[0], lv[1]); }

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
                auto s = Rectangle<float> (6, 6.0f + (float) i * 18, r.getWidth() - 12, 15);
                const bool has = i < plugs.size();
                const bool az = has && pluginDisplayName (*plugs[i]).startsWithIgnoreCase ("AZ");
                g.setColour (az ? red.withAlpha (0.16f) : Colours::white.withAlpha (0.04f)); g.fillRoundedRectangle (s, 4);
                label (g, has ? pluginDisplayName (*plugs[i]) : U ("\xe2\x80\x94"), s.reduced (5, 0), 8.5f, az ? redSoft : has ? Colour (0xffa49c9e) : dim2, Justification::centredLeft, 400);
            }
        }
        else caps (g, "STEREO OUT", { 6, 8, r.getWidth() - 12, 14 }, 8.5f, redSoft, Justification::centred);
        // metr
        for (int c = 0; c < 2; ++c)
        {
            auto m = Rectangle<float> (46.0f + (float) c * 9, 104, 5, (float) getHeight() - 150);
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
};

class Mixer : public juce::Component, private juce::Timer
{
public:
    std::function<void (te::EditItemID, float)> onTrackLevel;
    explicit Mixer (AppEngine& a) : app (a) { addAndMakeVisible (vp); vp.setViewedComponent (&holder, false); vp.setScrollBarsShown (false, true); startTimerHz (25); }
    void rebuild()
    {
        strips.clear(); master.reset();
        if (! app.edit) return;
        for (auto* t : te::getAudioTracks (*app.edit)) { auto* s = strips.add (new ChannelStrip (app, t)); holder.addAndMakeVisible (s); ids.add (t->itemID); }
        master = std::make_unique<ChannelStrip> (app, nullptr); addAndMakeVisible (*master);
        resized();
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (10, 8);
        r.removeFromLeft (18);
        if (master) master->setBounds (r.removeFromRight (104));
        r.removeFromRight (10);
        vp.setBounds (r);
        holder.setSize (juce::jmax (r.getWidth(), strips.size() * 98), r.getHeight() - 8);
        for (int i = 0; i < strips.size(); ++i) strips[i]->setBounds (i * 98, 0, 92, holder.getHeight());
    }
    void paint (juce::Graphics& g) override
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0e0c10), 0, 0, juce::Colour (0xff09080b), 0, (float) getHeight(), false)); g.fillAll();
        g.setColour (line); g.drawHorizontalLine (0, 0, (float) getWidth());
        juce::Graphics::ScopedSaveState s (g);
        g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, 18, (float) getHeight() * 0.5f));
        caps (g, "MIKSER", { 18 - 60, (float) getHeight() * 0.5f - 8, 120, 12 }, 9.5f, dim2, juce::Justification::centred);
    }
private:
    void timerCallback() override
    {
        for (int i = 0; i < strips.size(); ++i) { strips[i]->tick(); if (onTrackLevel) onTrackLevel (ids[i], strips[i]->level()); }
        if (master) master->tick();
    }
    AppEngine& app; juce::Viewport vp; juce::Component holder;
    juce::OwnedArray<ChannelStrip> strips; juce::Array<te::EditItemID> ids; std::unique_ptr<ChannelStrip> master;
};

// =============================================================================  Media brauzer
class MediaBrowser : public juce::Component, private juce::FileBrowserListener, private juce::ChangeListener
{
public:
    std::function<void (const juce::File&)> onInsertFile;

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
        app.engine.getDeviceManager().deviceManager.addAudioCallback (&player);
        player.setSource (&transport);
        transport.addChangeListener (this);
    }
    ~MediaBrowser() override
    {
        transport.setSource (nullptr); player.setSource (nullptr);
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
        auto prev = r.removeFromBottom (86); juce::ignoreUnused (prev);
        tree->setBounds (r);
    }
    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        g.fillAll (bg1);
        g.setColour (line); g.drawVerticalLine (0, 0, (float) getHeight());
        caps (g, "MEDIA BRAUZER", { 14, 12, 200, 12 });
        label (g, root.getFullPathName(), { 14, 70, (float) getWidth() - 28, 16 }, 10, dim2, Justification::centredLeft, 400);
        auto pr = Rectangle<float> (12, (float) getHeight() - 92, (float) getWidth() - 24, 80);
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
        transport.stop(); transport.setSource (nullptr); reader.reset();
        previewFile = f;
        if (auto* r = formats.createReaderFor (f))
        {
            reader = std::make_unique<juce::AudioFormatReaderSource> (r, true);
            transport.setSource (reader.get(), 0, nullptr, r->sampleRate);
            transport.setPosition (0); transport.start();
        }
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
};

// =============================================================================  Holat qatori
class StatusBar : public juce::Component
{
public:
    explicit StatusBar (AppEngine& a) : app (a) {}
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
        label (g, "AZ MUS " + String (JUCE_APPLICATION_VERSION_STRING) + U (" \xc2\xb7 Azimxodjayev Music \xc2\xb7 Powered by Tracktion Engine"),
               getLocalBounds().toFloat().withTrimmedRight (16), 10, dim2, Justification::centredRight, 500, 0.08f);
    }
private:
    AppEngine& app;
};
}
