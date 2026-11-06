// =============================================================================
//  AZ MUS — asosiy oyna
// =============================================================================
#pragma once
#include "Views.h"
#include "PianoRoll.h"
#include "Connections.h"

namespace az
{
// Klaviatura xaritasi (yordam oynasi va hujjat uchun yagona manba)
struct KeyHelp { const char* key; const char* what; };
inline const KeyHelp keyHelp[] = {
    { "Space",                "Ijro / to'xtatish" },
    { "Enter (Num)",          "Ijro" },
    { "Num 0",                "To'xtatish (2x \xe2\x80\x94 boshiga)" },
    { "R / Num *",            "Yozish" },
    { "L / Num /",            "Loop yoqish / o'chirish" },
    { "C",                    "Metronom" },
    { "Home / Num .",         "Boshiga" },
    { "End",                  "Loyiha oxiriga" },
    { "\xe2\x86\x90 \xe2\x86\x92 / Num - +", "Bir takt orqaga / oldinga" },
    { "B",                    "Bounce: tanlangan kliplarni bitta audioga" },
    { "P",                    "Locatorlar (loop) = tanlangan kliplar" },
    { "S / Ctrl+E",           "Kursor joyida kesish" },
    { "Ctrl+D",               "Nusxa" },
    { "Del",                  "O'chirish" },
    { "Ctrl+A",               "Hammasini belgilash" },
    { "Esc",                  "Belgilashni bekor qilish" },
    { "1 2 3 4",              "Asbob: tanlash / qaychi / o'chirg'ich / mute" },
    { "H / G  (+ / -)",       "Gorizontal zoom" },
    { "Shift+H / Shift+G",    "Trek balandligi" },
    { "Ctrl+Shift+A",         "Hammasini ekranga sig'dirish" },
    { "J",                    "Snap yoqish / o'chirish" },
    { "F3",                   "Mikser" },
    { "F4",                   "Audio ulanishlar (Inputs, Outputs, FX, Control Room)" },
    { "Alt + sudrash",        "Klipdan nusxa olish" },
    { "Bo'sh joyda sudrash",  "To'rtburchak bilan bir nechta klipni belgilash" },
    { "Ctrl + / Ctrl - / Ctrl 0", "Butun DAW oynasini kattalashtirish / kichiklashtirish / 100%" },
    { "F5",                   "Media brauzer" },
    { "Alt+I",                "Inspektor" },
    { "Ctrl+Z / Ctrl+Y",      "Undo / Redo" },
    { "Ctrl+S / Ctrl+Shift+S","Saqlash / boshqa nom bilan" },
    { "Ctrl+N / Ctrl+O",      "Yangi / ochish" },
    { "Ctrl+T",               "Trek qo'shish" },
    { "Ctrl+I",               "Audio import" },
    { "Ctrl+Shift+E",         "WAV'ga eksport" },
};

class MainComponent : public juce::Component, public juce::MenuBarModel, private juce::ChangeListener, private juce::Timer
{
public:
    explicit MainComponent (AppEngine& a)
        : app (a), top (a, this), inspector (a, vs), ruler (a, vs), headers (a, vs), lanes (a, vs), mixer (a), media (a), status (a)
    {
        setOpaque (true);
        for (auto* c : std::initializer_list<juce::Component*> { &top, &inspector, &ruler, &headers, &lanes, &mixer, &media, &status, &toolAdd, &toolSnap, &toolBounce })
            addAndMakeVisible (c);
        iconButton (toolAdd, "#plus", "Trek qo'shish (Ctrl+T)");
        iconButton (toolSnap, "#grid", "Snap (J)");
        iconButton (toolBounce, "#bounce", "Bounce (B)");
        const char* tIcons[] = { "#arrow", "#scissors", "#eraser", "#mute" };
        const char* tTips[]  = { "Tanlash (1)", "Qaychi (2)", "O'chirg'ich (3)", "Mute (4)" };
        for (int i = 0; i < 4; ++i)
        {
            auto* b = tools.add (new juce::TextButton()); iconButton (*b, tIcons[i], tTips[i]);
            b->onClick = [this, i] { setTool (i); };
            addAndMakeVisible (b);
        }
        setTool (0);
        toolSnap.setToggleState (vs.snap, juce::dontSendNotification);
        toolAdd.onClick = [this] { showAddTrackMenu(); };
        toolSnap.onClick = [this] { vs.snap = ! vs.snap; toolSnap.setToggleState (vs.snap, juce::dontSendNotification); repaint(); };
        toolBounce.onClick = [this] { bounce(); };

        top.playB.onClick  = [this] { togglePlay(); };
        top.stopB.onClick  = [this] { stop(); };
        top.recB.onClick   = [this] { toggleRecord(); };
        top.startB.onClick = [this] { toStart(); };
        top.rewB.onClick   = [this] { nudge (-1); };
        top.ffB.onClick    = [this] { nudge (1); };
        top.loopB.onClick  = [this] { toggleLoop(); };
        top.clickB.onClick = [this] { if (app.edit) { app.edit->clickTrackEnabled = ! app.edit->clickTrackEnabled; refreshAll(); } };
        top.onLanguage = [this] (int l) { setLanguage (l); };
        top.onTogglePanel = [this] (int p) { togglePanel (p); };

        lanes.onOpenPianoRoll = [this] (te::MidiClip& c) { openPianoRoll (c); };
        lanes.onSelectionChanged = [this] { inspector.refresh(); headers.repaint(); };
        headers.onSelectionChanged = [this] { inspector.refresh(); lanes.repaint(); };
        mixer.onTrackLevel = [this] (te::EditItemID id, float l) { headers.levels[id] = l; };
        media.onInsertFile = [this] (const juce::File& f) { insertFileAtCursor (f); };
        inspector.onRoutingChanged = [this] { mixer.rebuild(); lanes.repaint(); headers.repaint(); };
        top.onUiZoom = [this] (int d) { if (onUiZoom) onUiZoom (d); };

        auto& pf = app.engine.getPropertyStorage().getPropertiesFile();
        showInsp = pf.getBoolValue ("azShowInsp", true); showMedia = pf.getBoolValue ("azShowMedia", true); showMixer = pf.getBoolValue ("azShowMixer", true);
        top.setPanels (showInsp, showMedia, showMixer);

        vs.addChangeListener (this);
        app.addChangeListener (this);
        app.onEditChanged = [this] { editChanged(); };
        setWantsKeyboardFocus (true);
        startTimerHz (30);
        editChanged();
    }
    ~MainComponent() override { app.onEditChanged = nullptr; app.removeChangeListener (this); vs.removeChangeListener (this); pianoRoll.reset(); connections.reset(); }

    void editChanged()
    {
        pianoRoll.reset();
        if (! app.edit) return;
        auto ts = te::getAudioTracks (*app.edit);
        if (! ts.isEmpty()) { vs.selectedTrack = ts[0]->itemID; app.routeMidiTo (ts[0]); }
        app.edit->getTransport().addChangeListener (this);
        Routing (app).controlRoom();        // Control Room har doim master oxirida
        mixer.rebuild(); inspector.refresh();
        if (lanes.getWidth() > 100) zoomToFit();
        refreshAll();
        if (auto* w = findParentComponentOfClass<juce::DocumentWindow>()) w->setName ("AZ MUS  \xc2\xb7  " + app.editFile.getFileNameWithoutExtension());
    }

    // ---------------------------------------------------------------- layout
    void resized() override
    {
        auto r = getLocalBounds();
        top.setBounds (r.removeFromTop (TopBar::menuH + TopBar::barH));
        status.setBounds (r.removeFromBottom (26));
        mixer.setVisible (showMixer); inspector.setVisible (showInsp); media.setVisible (showMedia);
        if (showMixer) mixer.setBounds (r.removeFromBottom (juce::jlimit (180, 236, getHeight() * 3 / 10)));
        if (showInsp) inspector.setBounds (r.removeFromLeft (236));
        if (showMedia) media.setBounds (r.removeFromRight (260));
        auto hdr = r.removeFromLeft (250);
        toolArea = hdr.removeFromTop (56);
        int x = toolArea.getX() + 8; const int y = toolArea.getCentreY() - 13;
        toolAdd.setBounds (x, y, 28, 26); x += 34;
        for (auto* b : tools) { b->setBounds (x, y, 28, 26); x += 30; }
        x += 4; toolSnap.setBounds (x, y, 28, 26); x += 30; toolBounce.setBounds (x, y, 28, 26);
        headers.setBounds (hdr);
        ruler.setBounds (r.removeFromTop (56));
        lanes.setBounds (r);
        if (! fitted && lanes.getWidth() > 100 && app.edit) { fitted = true; zoomToFit(); }
    }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (bg0);
        g.setColour (bg2); g.fillRect (toolArea);
        g.setColour (line); g.drawHorizontalLine (toolArea.getBottom() - 1, (float) toolArea.getX(), (float) toolArea.getRight());
        g.drawVerticalLine (toolArea.getRight() - 1, (float) toolArea.getY(), (float) toolArea.getBottom());
    }

    // ---------------------------------------------------------------- panellar va til
    void togglePanel (int p)
    {
        if (p == 0) showInsp = ! showInsp;
        if (p == 1) showMedia = ! showMedia;
        if (p == 2) showMixer = ! showMixer;
        auto& pf = app.engine.getPropertyStorage().getPropertiesFile();
        pf.setValue ("azShowInsp", showInsp); pf.setValue ("azShowMedia", showMedia); pf.setValue ("azShowMixer", showMixer);
        top.setPanels (showInsp, showMedia, showMixer);
        resized(); repaint(); menuItemsChanged();
    }
    void setLanguage (int l)
    {
        az::setLanguage (l);
        top.refreshLang(); menuItemsChanged();
        inspector.refresh();
        std::function<void (juce::Component&)> rp = [&rp] (juce::Component& c) { c.repaint(); for (auto* ch : c.getChildren()) rp (*ch); };
        rp (*this);
        toast (U ("Til: O'zbekcha"));
    }
    void setTool (int t)
    {
        vs.tool = t;
        for (int i = 0; i < tools.size(); ++i) tools[i]->setToggleState (i == t, juce::dontSendNotification);
    }
    void toast (const juce::String& m) { status.message = m; status.messageTime = juce::Time::getMillisecondCounter(); status.repaint(); }

    // ---------------------------------------------------------------- transport
    void togglePlay()
    {
        if (! app.edit) return;
        auto& t = app.edit->getTransport();
        if (t.isPlaying()) { const bool rec = t.isRecording(); t.stop (false, false); lanes.recStart.reset(); if (rec) app.saveIfPossible(); }
        else t.play (false);
        refreshAll();
    }
    void play() { if (app.edit && ! app.edit->getTransport().isPlaying()) app.edit->getTransport().play (false); refreshAll(); }
    void stop()
    {
        if (! app.edit) return;
        auto& t = app.edit->getTransport();
        if (! t.isPlaying()) t.setPosition (0s);
        const bool rec = t.isRecording();
        t.stop (false, false); lanes.recStart.reset();
        if (rec) app.saveIfPossible();
        refreshAll();
    }
    void toStart() { if (app.edit) app.edit->getTransport().setPosition (0s); refreshAll(); }
    void toEnd()   { if (app.edit) app.edit->getTransport().setPosition (te::TimePosition() + app.edit->getLength()); refreshAll(); }
    void toggleRecord()
    {
        if (! app.edit) return;
        auto& t = app.edit->getTransport();
        if (t.isRecording()) { t.stop (false, false); lanes.recStart.reset(); app.saveIfPossible(); }
        else
        {
            bool anyArmed = false;
            for (auto* tr : te::getAudioTracks (*app.edit)) anyArmed |= app.isArmed (*tr);
            if (! anyArmed) if (auto* sel = inspector.track()) app.setArmed (*sel, true);
            lanes.recStart.reset();
            t.record (false);
        }
        refreshAll();
    }
    void toggleLoop()
    {
        if (! app.edit) return;
        auto& t = app.edit->getTransport(); ensureLoop(); t.looping = ! t.looping; refreshAll();
    }
    void nudge (int dir)
    {
        if (! app.edit) return;
        auto& ts = app.edit->tempoSequence; auto& t = app.edit->getTransport();
        auto bar = ts.toBarsAndBeats (t.getPosition()).bars + dir;
        t.setPosition (ts.toTime (te::tempo::BarsAndBeats { juce::jmax (0, bar), {} }));
    }
    void ensureLoop()
    {
        auto& t = app.edit->getTransport();
        if (t.getLoopRange().getLength() <= 0s)
        {
            auto& ts = app.edit->tempoSequence; auto bar = ts.toBarsAndBeats (t.getPosition()).bars;
            t.setLoopRange ({ ts.toTime (te::tempo::BarsAndBeats { bar, {} }), ts.toTime (te::tempo::BarsAndBeats { bar + 4, {} }) });
        }
    }

    // ---------------------------------------------------------------- tahrir
    juce::Array<te::Clip*> selectedClips() { return app.selection.getItemsOfType<te::Clip>(); }
    void splitAtCursor()
    {
        if (! app.edit) return;
        const auto pos = app.edit->getTransport().getPosition();
        auto list = selectedClips();
        auto inside = [pos] (te::Clip* c) { return c->getEditTimeRange().getStart() + 0.01s < pos && pos < c->getEditTimeRange().getEnd() - 0.01s; };
        juce::Array<te::Clip*> todo;
        for (auto* c : list) if (inside (c)) todo.add (c);
        if (todo.isEmpty())    // Daftar kabi: tanlov bo'lmasa — kursor ostidagi barcha kliplar
            for (auto* t : te::getAudioTracks (*app.edit)) for (auto* c : t->getClips()) if (inside (c)) todo.add (c);
        if (todo.isEmpty()) { toast (U ("Kesish uchun kursorni klip ustiga qo'ying.")); return; }
        app.edit->getUndoManager().beginNewTransaction (U ("Bo'lish"));
        for (auto* c : todo) if (auto* t = dynamic_cast<te::ClipTrack*> (c->getTrack())) t->splitClip (*c, pos);
        lanes.repaint();
    }
    void deleteSelected()
    {
        if (! app.edit) return;
        app.edit->getUndoManager().beginNewTransaction (U ("O'chirish"));
        for (auto* c : selectedClips()) c->removeFromParent();
        app.selection.deselectAll(); lanes.repaint();
    }
    void duplicateSelected() { for (auto* c : selectedClips()) lanes.duplicate (*c); lanes.repaint(); }
    void selectAll()
    {
        if (! app.edit) return;
        app.selection.deselectAll();
        int n = 0;
        for (auto* t : te::getAudioTracks (*app.edit)) for (auto* c : t->getClips()) { app.selection.addToSelection (c); ++n; }
        toast (U ("Hammasi belgilandi: ") + juce::String (n) + U (" klip")); lanes.repaint();
    }
    void bounce()
    {
        if (app.edit && app.edit->getTransport().isPlaying()) app.edit->getTransport().stop (false, false);
        toast (app.bounceSelection (selectedClips()));
        app.selection.deselectAll(); mixer.rebuild(); refreshAll();
    }
    void locators() { toast (app.locatorsToSelection (selectedClips())); refreshAll(); }
    void zoomH (double f)
    {
        const double centre = vs.xToSec ((float) lanes.getWidth() * 0.5f);
        vs.pxPerSec = juce::jlimit (2.0, 2000.0, vs.pxPerSec * f);
        vs.startSec = juce::jmax (0.0, centre - lanes.getWidth() * 0.5 / vs.pxPerSec); vs.changed();
    }
    void zoomTracks (double f) { vs.trackH = juce::jlimit (36, 220, (int) std::round (vs.trackH * f)); vs.changed(); }
    void zoomToFit()
    {
        if (! app.edit) return;
        const double len = juce::jmax (30.0, app.edit->getLength().inSeconds() + 6.0);
        vs.startSec = 0; vs.pxPerSec = juce::jlimit (4.0, 400.0, (double) juce::jmax (600, lanes.getWidth()) / len);
        vs.changed();
    }
    void fitAll()
    {
        zoomToFit();
        const int n = juce::jmax (1, te::getAudioTracks (*app.edit).size());
        vs.trackH = juce::jlimit (36, 220, (lanes.getHeight() - 10) / n); vs.scrollY = 0; vs.changed();
        toast (U ("Hamma narsa ekranga sig'dirildi"));
    }

    bool keyPressed (const juce::KeyPress& k) override
    {
        using KP = juce::KeyPress;
        const auto m = k.getModifiers();
        const bool ctrl = m.isCommandDown(), shift = m.isShiftDown(), alt = m.isAltDown();
        const int code = k.getKeyCode();
        const auto ch = juce::CharacterFunctions::toLowerCase (k.getTextCharacter());
        auto plainKey = [&] (juce_wchar c) { return ! ctrl && ! alt && (code == (int) juce::CharacterFunctions::toUpperCase (c) || ch == c); };

        // F klavishlar (Cubase)
        if (code == KP::F3Key) { togglePanel (2); return true; }
        if (code == KP::F4Key) { showConnections(); return true; }
        if (ctrl && (k.getTextCharacter() == '=' || k.getTextCharacter() == '+' || code == KP::numberPadAdd)) { if (onUiZoom) onUiZoom (1); return true; }
        if (ctrl && (k.getTextCharacter() == '-' || code == KP::numberPadSubtract)) { if (onUiZoom) onUiZoom (-1); return true; }
        if (ctrl && code == '0') { if (onUiZoom) onUiZoom (0); return true; }
        if (code == KP::F5Key) { togglePanel (1); return true; }
        if (alt && ! ctrl && code == 'I') { togglePanel (0); return true; }
        // Numpad (Cubase)
        if (code == KP::numberPadMultiply || (! ctrl && k.getTextCharacter() == '*')) { toggleRecord(); return true; }
        if (code == KP::numberPadDivide   || (! ctrl && k.getTextCharacter() == '/')) { toggleLoop(); return true; }
        if (code == KP::numberPad0)        { stop(); return true; }
        if (code == KP::deleteKey || code == KP::backspaceKey) { deleteSelected(); return true; }   // (Windows'da Num Del bilan bir xil kod)
        if (code == KP::numberPadDecimalPoint) { toStart(); return true; }
        if (code == KP::numberPadAdd)      { nudge (1); return true; }
        if (code == KP::numberPadSubtract) { nudge (-1); return true; }
        if (code == KP::returnKey && ! ctrl) { play(); return true; }
        // transport
        if (code == KP::spaceKey) { togglePlay(); return true; }
        if (code == KP::homeKey)  { toStart(); return true; }
        if (code == KP::endKey)   { toEnd(); return true; }
        if (code == KP::leftKey && ! ctrl)  { nudge (-1); return true; }
        if (code == KP::rightKey && ! ctrl) { nudge (1); return true; }
        if (code == KP::escapeKey) { app.selection.deselectAll(); lanes.repaint(); return true; }
        if (code == KP::deleteKey || code == KP::backspaceKey) { deleteSelected(); return true; }
        // Ctrl birikmalari
        if (ctrl && shift && code == 'A') { fitAll(); return true; }
        if (ctrl && shift && code == 'S') { menuItemSelected (4, 0); return true; }
        if (ctrl && shift && code == 'E') { exportMix(); return true; }
        if (ctrl && ! shift && code == 'A') { selectAll(); return true; }
        if (ctrl && code == 'Z' && ! shift) { if (app.edit) app.edit->getUndoManager().undo(); refreshAll(); return true; }
        if (ctrl && (code == 'Y' || (code == 'Z' && shift))) { if (app.edit) app.edit->getUndoManager().redo(); refreshAll(); return true; }
        if (ctrl && code == 'S') { app.saveIfPossible(); toast (U ("Saqlandi")); return true; }
        if (ctrl && code == 'D') { duplicateSelected(); return true; }
        if (ctrl && code == 'E') { splitAtCursor(); return true; }
        if (ctrl && code == 'T') { showAddTrackMenu(); return true; }
        if (ctrl && code == 'N') { menuItemSelected (1, 0); return true; }
        if (ctrl && code == 'O') { menuItemSelected (2, 0); return true; }
        if (ctrl && code == 'I') { menuItemSelected (5, 0); return true; }
        if (ctrl && code == KP::downKey) { zoomTracks (1.25); return true; }
        if (ctrl && code == KP::upKey)   { zoomTracks (1 / 1.25); return true; }
        // bitta harfli (Daftar DAW)
        if (shift && ! ctrl && code == 'H') { zoomTracks (1.25); return true; }
        if (shift && ! ctrl && code == 'G') { zoomTracks (1 / 1.25); return true; }
        if (! shift && plainKey ('b')) { bounce(); return true; }
        if (! shift && plainKey ('p')) { locators(); return true; }
        if (! shift && plainKey ('s')) { splitAtCursor(); return true; }
        if (! shift && plainKey ('r')) { toggleRecord(); return true; }
        if (! shift && plainKey ('l')) { toggleLoop(); return true; }
        if (! shift && plainKey ('c')) { top.clickB.triggerClick(); return true; }
        if (! shift && plainKey ('j')) { toolSnap.triggerClick(); return true; }
        if (! shift && plainKey ('h')) { zoomH (1.25); return true; }
        if (! shift && plainKey ('g')) { zoomH (0.8); return true; }
        if (k.getTextCharacter() == '+' || k.getTextCharacter() == '=') { zoomH (1.25); return true; }
        if (k.getTextCharacter() == '-') { zoomH (0.8); return true; }
        if (! ctrl && ! alt && k.getTextCharacter() >= '1' && k.getTextCharacter() <= '4') { setTool (k.getTextCharacter() - '1'); return true; }
        return false;
    }

    // ---------------------------------------------------------------- menyular
    juce::StringArray getMenuBarNames() override { return { "Fayl", "Tahrir", "Loyiha", "Audio", "MIDI", "Ko'rinish", "Plaginlar", "Til", "Yordam" }; }

    juce::PopupMenu getMenuForIndex (int idx, const juce::String&) override
    {
        juce::PopupMenu m;
        auto add = [&m] (int id, const char* t, const char* sc = "", bool ticked = false)
        { juce::PopupMenu::Item it (U (t)); it.itemID = id; it.shortcutKeyDescription = juce::String::fromUTF8 (sc); it.isTicked = ticked; m.addItem (std::move (it)); };
        switch (idx)
        {
            case 0: add (1, "Yangi loyiha", "Ctrl+N"); add (2, "Ochish...", "Ctrl+O"); m.addSeparator();
                    add (3, "Saqlash", "Ctrl+S"); add (4, "Boshqa nom bilan saqlash...", "Ctrl+Shift+S"); m.addSeparator();
                    add (5, "Audio import...", "Ctrl+I"); add (6, "WAV'ga eksport (mixdown)...", "Ctrl+Shift+E"); m.addSeparator(); add (9, "Chiqish"); break;
            case 1: add (10, "Bekor qilish (Undo)", "Ctrl+Z"); add (11, "Qaytarish (Redo)", "Ctrl+Y"); m.addSeparator();
                    add (12, "Kursor joyida kesish", "S"); add (13, "Nusxa", "Ctrl+D"); add (14, "O'chirish", "Del"); m.addSeparator();
                    add (15, "Bounce Selection", "B"); add (16, "Locatorlar = tanlov", "P"); add (17, "Hammasini belgilash", "Ctrl+A"); m.addSeparator();
                    add (18, "Tanlash", "1", vs.tool == 0); add (19, "Qaychi", "2", vs.tool == 1); add (20, "O'chirg'ich", "3", vs.tool == 2); add (21, "Mute", "4", vs.tool == 3); break;
            case 2: add (30, "Audio trek qo'shish", "Ctrl+T"); add (31, "Instrument trek qo'shish"); add (32, "Marker qo'shish (kursor joyida)"); break;
            case 3: add (42, "Audio ulanishlar (Inputs, Outputs, FX, Control Room)...", "F4"); add (40, "Audio qurilma sozlamalari (ASIO, drayver, bufer)...");
                    add (43, "FX kanal qo'shish..."); add (41, "Yozish uchun kirishlarni yangilash"); break;
            case 4: add (50, "MIDI import..."); add (51, "MIDI qurilmalarni qayta skanerlash"); break;
            case 5: add (60, "Inspektor", "Alt+I", showInsp); add (61, "Media brauzer", "F5", showMedia); add (62, "Mikser", "F3", showMixer); m.addSeparator();
                    add (63, "Hammasini ekranga sig'dirish", "Ctrl+Shift+A"); add (64, "Kattalashtirish", "H"); add (65, "Kichiklashtirish", "G"); break;
            case 6: add (70, "Plagin qo'shish (tanlangan trekka)..."); add (72, "Master insert qo'shish (mastering)..."); m.addSeparator();
                    add (71, "VST3 plaginlarni skanerlash va boshqarish..."); break;
            case 7: { juce::PopupMenu::Item a ("O'zbekcha"); a.itemID = 80; a.isTicked = lang == 0; m.addItem (std::move (a));
                      juce::PopupMenu::Item b (juce::String::fromUTF8 ("Русский")); b.itemID = 81; b.isTicked = lang == 1; m.addItem (std::move (b));
                      juce::PopupMenu::Item c ("English"); c.itemID = 82; c.isTicked = lang == 2; m.addItem (std::move (c)); break; }
            case 8: add (90, "Klaviatura qisqa tugmalari"); add (91, "AZ MUS haqida"); break;
        }
        return m;
    }

    void menuItemSelected (int id, int) override
    {
        switch (id)
        {
            case 1: app.newProject(); break;
            case 2: chooseFile (U ("Loyihani ochish"), "*.azmus;*.tracktionedit", false, [this] (juce::File f) { if (! app.openProject (f)) juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "AZ MUS", U ("Loyihani ochib bo'lmadi")); }); break;
            case 3: app.saveIfPossible(); toast (U ("Saqlandi")); break;
            case 4: chooseFile (U ("Boshqa nom bilan saqlash"), "*.azmus", true, [this] (juce::File f) { app.saveAs (f); editChanged(); }); break;
            case 5: chooseFile (U ("Audio import"), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg", false, [this] (juce::File f) { insertFileAtCursor (f); }); break;
            case 6: exportMix(); break;
            case 9: juce::JUCEApplication::getInstance()->systemRequestedQuit(); break;
            case 10: if (app.edit) app.edit->getUndoManager().undo(); break;
            case 11: if (app.edit) app.edit->getUndoManager().redo(); break;
            case 12: splitAtCursor(); break;
            case 13: duplicateSelected(); break;
            case 14: deleteSelected(); break;
            case 15: bounce(); break;
            case 16: locators(); break;
            case 17: selectAll(); break;
            case 18: case 19: case 20: case 21: setTool (id - 18); break;
            case 30: addTrack (false); break;
            case 31: addTrack (true); break;
            case 32: if (app.edit) { auto p = app.edit->getTransport().getPosition(); app.edit->getMarkerManager().createMarker (-1, p, 8s, nullptr); ruler.repaint(); } break;
            case 40: showAudioSettings(); break;
            case 42: showConnections(); break;
            case 43: showConnections(); break;
            case 41: if (app.edit) app.setupInputs(); inspector.refresh(); break;
            case 50: chooseFile (U ("MIDI import"), "*.mid;*.midi", false, [this] (juce::File f) { insertFileAtCursor (f); }); break;
            case 51: app.engine.getDeviceManager().rescanMidiDeviceList(); if (app.edit) app.setupInputs(); break;
            case 60: togglePanel (0); break;
            case 61: togglePanel (1); break;
            case 62: togglePanel (2); break;
            case 63: fitAll(); break;
            case 64: zoomH (1.25); break;
            case 65: zoomH (0.8); break;
            case 70: if (auto* t = inspector.track())
                     {
                         te::Track::Ptr tp (t);
                         showPluginPicker (app, localAreaToGlobal (getLocalBounds().withSizeKeepingCentre (10, 10)), [this, tp] (const PickItem& it)
                         { if (auto* at = dynamic_cast<te::AudioTrack*> (tp.get())) if (auto p = insertPicked (app, *at, it)) p->showWindowExplicitly(); inspector.refresh(); mixer.rebuild(); });
                     } break;
            case 71: showPluginManager(); break;
            case 72: if (! showMixer) togglePanel (2);
                     showPluginPicker (app, localAreaToGlobal (getLocalBounds().withSizeKeepingCentre (10, 10)), [this] (const PickItem& it)
                     { if (auto p = insertMaster (app, it)) p->showWindowExplicitly(); mixer.repaint(); });
                     break;
            case 80: case 81: case 82: setLanguage (id - 80); break;
            case 90: showShortcuts(); break;
            case 91: showAbout(); break;
            default: break;
        }
        refreshAll();
    }

    // ---------------------------------------------------------------- amallar
    void showAddTrackMenu()
    {
        juce::PopupMenu m;
        m.addItem (1, U ("Audio trek (vokal, gitara, sample)"));
        m.addItem (2, U ("Instrument trek (AZ Synth)"));
        juce::PopupMenu vi; auto types = app.engine.getPluginManager().knownPluginList.getTypes();
        for (int i = 0; i < types.size(); ++i) if (types[i].isInstrument) vi.addItem (100 + i, types[i].name);
        m.addSubMenu (U ("Instrument trek (VST3i)"), vi, vi.getNumItems() > 0);
        m.showMenuAsync ({}, [this, types] (int r)
        {
            if (r == 1) addTrack (false);
            if (r == 2) addTrack (true);
            if (r >= 100) { app.edit->getUndoManager().beginNewTransaction ("Trek"); auto d = types[r - 100]; if (auto* t = app.addInstrumentTrack (d.name, &d)) selectTrack (t); }
        });
    }
    void addTrack (bool instrument)
    {
        if (! app.edit) return;
        app.edit->getUndoManager().beginNewTransaction ("Trek");
        const int n = te::getAudioTracks (*app.edit).size() + 1;
        auto* t = instrument ? app.addInstrumentTrack ("Synth " + juce::String (n)) : app.addAudioTrack ("Audio " + juce::String (n));
        selectTrack (t);
    }
    void selectTrack (te::AudioTrack* t)
    {
        if (! t) return;
        vs.selectedTrack = t->itemID; app.routeMidiTo (t);
        mixer.rebuild(); inspector.refresh(); vs.changed();
    }
    void insertFileAtCursor (const juce::File& f)
    {
        if (! app.edit) return;
        auto* t = inspector.track();
        const bool midi = f.hasFileExtension ("mid;midi");
        if (t == nullptr || (midi != AppEngine::isInstrumentTrack (*t) && t->getClips().size() > 0))
            t = midi ? app.addInstrumentTrack (f.getFileNameWithoutExtension()) : app.addAudioTrack (f.getFileNameWithoutExtension());
        const auto at = app.edit->getTransport().getPosition();
        if (midi) app.importMidi (*t, f, at); else app.importAudio (*t, f, at);
        selectTrack (t);
    }
    void openPianoRoll (te::MidiClip& c)
    {
        pianoRoll = std::make_unique<PianoRollWindow> (c, [this] { juce::MessageManager::callAsync ([this] { pianoRoll.reset(); lanes.repaint(); }); });
    }
    void exportMix()
    {
        chooseFile (U ("WAV'ga eksport"), "*.wav", true, [this] (juce::File f)
        {
            const bool ok = app.exportWav (f.withFileExtension ("wav"));
            juce::AlertWindow::showMessageBoxAsync (ok ? juce::MessageBoxIconType::InfoIcon : juce::MessageBoxIconType::WarningIcon, "AZ MUS",
                                                    ok ? U ("Eksport tayyor:") + "\n" + f.withFileExtension ("wav").getFullPathName() : U ("Eksport qilib bo'lmadi (loyiha bo'sh?)"));
        });
    }
    void chooseFile (const juce::String& title, const juce::String& pattern, bool save, std::function<void (juce::File)> cb)
    {
        chooser = std::make_unique<juce::FileChooser> (title, save ? app.editFile.getParentDirectory() : AppEngine::projectsFolder(), pattern);
        const int flags = save ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting : juce::FileBrowserComponent::openMode;
        chooser->launchAsync (flags | juce::FileBrowserComponent::canSelectFiles, [cb] (const juce::FileChooser& fc) { if (fc.getResult() != juce::File()) cb (fc.getResult()); });
    }
    void showAudioSettings()
    {
        juce::DialogWindow::LaunchOptions o;
        o.dialogTitle = U ("Audio sozlamalari") + "  \xc2\xb7  AZ MUS";
        o.dialogBackgroundColour = bg1;
        auto* c = new juce::AudioDeviceSelectorComponent (app.engine.getDeviceManager().deviceManager, 0, 64, 2, 64, true, false, true, false);
        c->setSize (560, 520); o.content.setOwned (c); o.useNativeTitleBar = true; o.resizable = true;
        o.launchAsync();
    }
    void showConnections()
    {
        if (connections) { connections->toFront (true); return; }
        connections = std::make_unique<ConnectionsWindow> (app,
            [this] { juce::MessageManager::callAsync ([this] { connections.reset(); }); },
            [this] { showAudioSettings(); },
            [this] { mixer.rebuild(); inspector.refresh(); lanes.repaint(); headers.repaint(); });
    }
    std::function<void (int)> onUiZoom;
    Lanes& getLanes() { return lanes; }
    void setUiZoomLabel (float s) { top.uiScale = s; top.repaint(); }

    void showPluginManager()
    {
        juce::DialogWindow::LaunchOptions o;
        o.dialogTitle = U ("Plaginlar menejeri") + "  \xc2\xb7  AZ MUS";
        o.dialogBackgroundColour = bg1;
        auto& pm = app.engine.getPluginManager();
        auto* v = new juce::PluginListComponent (pm.pluginFormatManager, pm.knownPluginList,
                                                 app.engine.getTemporaryFileManager().getTempFile ("PluginScanDeadMansPedal"),
                                                 std::addressof (app.engine.getPropertyStorage().getPropertiesFile()), true);
        v->setSize (820, 560); o.content.setOwned (v); o.useNativeTitleBar = true; o.resizable = true;
        o.launchAsync();
    }
    void showShortcuts()
    {
        struct List : juce::Component
        {
            List() { setSize (560, 24 + (int) std::size (keyHelp) * 24); }
            void paint (juce::Graphics& g) override
            {
                g.fillAll (bg1);
                for (int i = 0; i < (int) std::size (keyHelp); ++i)
                {
                    const float y = 12.0f + (float) i * 24;
                    if (i % 2 == 0) { g.setColour (juce::Colours::white.withAlpha (0.025f)); g.fillRect (8.0f, y, (float) getWidth() - 16, 24.0f); }
                    label (g, juce::String::fromUTF8 (keyHelp[i].key), { 18, y, 170, 24 }, 12, redSoft, juce::Justification::centredLeft, 600);
                    label (g, U (keyHelp[i].what), { 196, y, (float) getWidth() - 210, 24 }, 12, text, juce::Justification::centredLeft, 400);
                }
            }
        };
        juce::DialogWindow::LaunchOptions o;
        o.dialogTitle = U ("Klaviatura qisqa tugmalari") + "  \xc2\xb7  AZ MUS";
        o.dialogBackgroundColour = bg1; o.useNativeTitleBar = true;
        auto* vp = new juce::Viewport(); vp->setViewedComponent (new List(), true); vp->setSize (580, 640);
        o.content.setOwned (vp); o.launchAsync();
    }
    void showAbout()
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, "AZ MUS " + juce::String (JUCE_APPLICATION_VERSION_STRING),
            "Digital Audio Workstation\nAzimxodjayev Music \xc2\xb7 Alif Records\n\nPowered by Tracktion Engine (Personal licence)\nJUCE \xc2\xb7 Airwindows (MIT)\n\n\xc2\xa9 2026 Azimxodjayev Music");
    }

    void refreshAll() { top.refreshState(); ruler.repaint(); lanes.repaint(); headers.repaint(); status.repaint(); }

private:
    void changeListenerCallback (juce::ChangeBroadcaster* src) override
    {
        if (src == &vs) { ruler.repaint(); lanes.repaint(); headers.repaint(); inspector.repaint(); }
        else refreshAll();
    }
    void timerCallback() override
    {
        if (! app.edit) return;
        auto& t = app.edit->getTransport();
        const double pos = t.getPosition().inSeconds();
        if (t.isPlaying())
        {
            const double right = vs.xToSec ((float) lanes.getWidth());
            if (pos > right - 1.0 || pos < vs.startSec) { vs.startSec = juce::jmax (0.0, pos - 2.0); vs.changed(); }
        }
        if (pos != lastPos || t.isRecording())
        {
            const float x1 = vs.secToX (lastPos), x2 = vs.secToX (pos);
            if (t.isRecording() && ++recTick % 3 == 0) lanes.repaint();
            else lanes.repaint ((int) juce::jmin (x1, x2) - 8, 0, (int) std::abs (x2 - x1) + 18, lanes.getHeight());
            ruler.repaint (0, ruler.getHeight() - 12, ruler.getWidth(), 12);
            lastPos = pos;
            top.refreshState();
        }
        if (++slow % 15 == 0) { top.refreshState(); status.repaint(); headers.repaint(); }
        if (slow % 3 == 0) headers.repaint (headers.getWidth() - 12, 0, 12, headers.getHeight());
    }

    AppEngine& app;
    ViewState vs;
    TopBar top; Inspector inspector; TimeRuler ruler; TrackHeaders headers; Lanes lanes; Mixer mixer; MediaBrowser media; StatusBar status;
    juce::TextButton toolAdd, toolSnap, toolBounce;
    juce::OwnedArray<juce::TextButton> tools;
    juce::Rectangle<int> toolArea;
    std::unique_ptr<PianoRollWindow> pianoRoll;
    std::unique_ptr<ConnectionsWindow> connections;
    std::unique_ptr<juce::FileChooser> chooser;
    bool showInsp = true, showMedia = true, showMixer = true, fitted = false;
    int recTick = 0, slow = 0; double lastPos = -1;
};
}
