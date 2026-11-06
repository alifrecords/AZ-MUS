// =============================================================================
//  AZ MUS — asosiy oyna
// =============================================================================
#pragma once
#include "Views.h"
#include "PianoRoll.h"

namespace az
{
class MainComponent : public juce::Component, public juce::MenuBarModel, private juce::ChangeListener, private juce::Timer
{
public:
    explicit MainComponent (AppEngine& a)
        : app (a), top (a, this), inspector (a, vs), ruler (a, vs), headers (a, vs), lanes (a, vs), mixer (a), media (a), status (a)
    {
        setOpaque (true);
        for (auto* c : std::initializer_list<juce::Component*> { &top, &inspector, &ruler, &headers, &lanes, &mixer, &media, &status, &toolAdd, &toolDraw, &toolSplit, &toolSnap })
            addAndMakeVisible (c);
        iconButton (toolAdd, "#plus", "Trek qo'shish (Ctrl+T)"); iconButton (toolDraw, "#pencil", "MIDI klip (2x bosish)");
        iconButton (toolSplit, "#scissors", "Kursor joyida bo'lish (Ctrl+E)"); iconButton (toolSnap, "#grid", "Snap (G)");
        toolSnap.setToggleState (vs.snap, juce::dontSendNotification);
        toolAdd.onClick = [this] { showAddTrackMenu(); };
        toolDraw.onClick = [this] { if (auto* t = inspector.track()) { auto& ts = app.edit->tempoSequence; auto p = app.edit->getTransport().getPosition();
                                    auto bar = ts.toBarsAndBeats (p).bars; t->insertMIDIClip ("MIDI", { ts.toTime (te::tempo::BarsAndBeats { bar, {} }), ts.toTime (te::tempo::BarsAndBeats { bar + 1, {} }) }, nullptr); lanes.repaint(); } };
        toolSplit.onClick = [this] { splitSelected(); };
        toolSnap.onClick = [this] { vs.snap = ! vs.snap; toolSnap.setToggleState (vs.snap, juce::dontSendNotification); };

        top.playB.onClick  = [this] { togglePlay(); };
        top.stopB.onClick  = [this] { stop(); };
        top.recB.onClick   = [this] { toggleRecord(); };
        top.startB.onClick = [this] { if (app.edit) app.edit->getTransport().setPosition (0s); };
        top.rewB.onClick   = [this] { nudge (-1); };
        top.ffB.onClick    = [this] { nudge (1); };
        top.loopB.onClick  = [this] { if (app.edit) { auto& t = app.edit->getTransport(); ensureLoop(); t.looping = ! t.looping; refreshAll(); } };
        top.clickB.onClick = [this] { if (app.edit) { app.edit->clickTrackEnabled = ! app.edit->clickTrackEnabled; refreshAll(); } };

        lanes.onOpenPianoRoll = [this] (te::MidiClip& c) { openPianoRoll (c); };
        lanes.onSelectionChanged = [this] { inspector.refresh(); headers.repaint(); };
        headers.onSelectionChanged = [this] { inspector.refresh(); lanes.repaint(); };
        mixer.onTrackLevel = [this] (te::EditItemID id, float l) { headers.levels[id] = l; };
        media.onInsertFile = [this] (const juce::File& f) { insertFileAtCursor (f); };

        vs.addChangeListener (this);
        app.addChangeListener (this);
        app.onEditChanged = [this] { editChanged(); };
        setWantsKeyboardFocus (true);
        startTimerHz (30);
        editChanged();
    }
    ~MainComponent() override { app.onEditChanged = nullptr; app.removeChangeListener (this); vs.removeChangeListener (this); pianoRoll.reset(); }

    void editChanged()
    {
        pianoRoll.reset();
        if (! app.edit) return;
        auto ts = te::getAudioTracks (*app.edit);
        if (! ts.isEmpty()) { vs.selectedTrack = ts[0]->itemID; app.routeMidiTo (ts[0]); }
        app.edit->getTransport().addChangeListener (this);
        mixer.rebuild(); inspector.refresh();
        zoomToFit();
        refreshAll();
        if (auto* w = findParentComponentOfClass<juce::DocumentWindow>()) w->setName ("AZ MUS  \xc2\xb7  " + app.editFile.getFileNameWithoutExtension());
    }

    // ---------------------------------------------------------------- layout
    void resized() override
    {
        auto r = getLocalBounds();
        top.setBounds (r.removeFromTop (58));
        status.setBounds (r.removeFromBottom (26));
        mixer.setBounds (r.removeFromBottom (mixerH));
        inspector.setBounds (r.removeFromLeft (236));
        media.setBounds (r.removeFromRight (260));
        auto hdr = r.removeFromLeft (250);
        auto tools = hdr.removeFromTop (56);
        int x = tools.getX() + 10;
        for (auto* b : { &toolAdd, &toolDraw, &toolSplit, &toolSnap }) { b->setBounds (x, tools.getCentreY() - 13, 28, 26); x += 34; }
        headers.setBounds (hdr);
        ruler.setBounds (r.removeFromTop (56));
        lanes.setBounds (r);
        if (! fitted && lanes.getWidth() > 100 && app.edit) { fitted = true; zoomToFit(); }
    }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (bg0);
        auto r = juce::Rectangle<int> (236, 58, 250, 56);
        g.setColour (bg2); g.fillRect (r);
        g.setColour (line); g.drawHorizontalLine (r.getBottom() - 1, (float) r.getX(), (float) r.getRight()); g.drawVerticalLine (r.getRight() - 1, (float) r.getY(), (float) r.getBottom());
        caps (g, vs.snap ? "SNAP: AVTO" : "SNAP: O'CHIQ", r.toFloat().withTrimmedRight (12), 9, dim2, juce::Justification::centredRight);
    }

    // ---------------------------------------------------------------- transport
    void togglePlay()
    {
        if (! app.edit) return;
        auto& t = app.edit->getTransport();
        if (t.isPlaying()) { const bool rec = t.isRecording(); t.stop (false, false); if (rec) app.saveIfPossible(); }
        else t.play (false);
        refreshAll();
    }
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
    void splitSelected()
    {
        if (! app.edit) return;
        const auto pos = app.edit->getTransport().getPosition();
        app.edit->getUndoManager().beginNewTransaction ("Bo'lish");
        for (auto* c : app.selection.getItemsOfType<te::Clip>())
            if (c->getEditTimeRange().contains (pos))
                if (auto* t = dynamic_cast<te::ClipTrack*> (c->getTrack())) t->splitClip (*c, pos);
        lanes.repaint();
    }
    void deleteSelected()
    {
        if (! app.edit) return;
        app.edit->getUndoManager().beginNewTransaction (U ("O'chirish"));
        for (auto* c : app.selection.getItemsOfType<te::Clip>()) c->removeFromParent();
        app.selection.deselectAll(); lanes.repaint();
    }

    bool keyPressed (const juce::KeyPress& k) override
    {
        using KP = juce::KeyPress; const auto cmd = juce::ModifierKeys::commandModifier;
        if (k == KP::spaceKey) { togglePlay(); return true; }
        if (k == KP::homeKey || k == KP::returnKey) { if (app.edit) app.edit->getTransport().setPosition (0s); return true; }
        if (k == KP ('r') || k == KP (KP::numberPadMultiply)) { toggleRecord(); return true; }
        if (k == KP ('l') || k == KP (KP::numberPadDivide)) { top.loopB.triggerClick(); return true; }
        if (k == KP ('c')) { top.clickB.triggerClick(); return true; }
        if (k == KP ('g')) { toolSnap.triggerClick(); return true; }
        if (k == KP::deleteKey || k == KP::backspaceKey) { deleteSelected(); return true; }
        if (k == KP ('e', cmd, 0)) { splitSelected(); return true; }
        if (k == KP ('d', cmd, 0)) { for (auto* c : app.selection.getItemsOfType<te::Clip>()) lanes.duplicate (*c); return true; }
        if (k == KP ('z', cmd, 0)) { if (app.edit) app.edit->getUndoManager().undo(); refreshAll(); return true; }
        if (k == KP ('z', cmd | juce::ModifierKeys::shiftModifier, 0) || k == KP ('y', cmd, 0)) { if (app.edit) app.edit->getUndoManager().redo(); refreshAll(); return true; }
        if (k == KP ('s', cmd, 0)) { app.saveIfPossible(); return true; }
        if (k == KP ('t', cmd, 0)) { showAddTrackMenu(); return true; }
        if (k == KP (KP::leftKey)) { nudge (-1); return true; }
        if (k == KP (KP::rightKey)) { nudge (1); return true; }
        if (k == KP ('+') || k == KP ('=')) { vs.pxPerSec = juce::jmin (2000.0, vs.pxPerSec * 1.25); vs.changed(); return true; }
        if (k == KP ('-')) { vs.pxPerSec = juce::jmax (2.0, vs.pxPerSec / 1.25); vs.changed(); return true; }
        if (k == KP ('m', cmd, 0)) { mixerH = mixerH > 0 ? 0 : 236; resized(); return true; }
        return false;
    }

    // ---------------------------------------------------------------- menyular
    juce::StringArray getMenuBarNames() override { return { "Fayl", "Tahrir", "Loyiha", "Audio", "MIDI", "Mikser", "Plaginlar", "Yordam" }; }

    juce::PopupMenu getMenuForIndex (int idx, const juce::String&) override
    {
        juce::PopupMenu m;
        auto add = [&m] (int id, const char* t, const char* sc = "") { juce::PopupMenu::Item it (U (t)); it.itemID = id; it.shortcutKeyDescription = U (sc); m.addItem (std::move (it)); };
        switch (idx)
        {
            case 0: add (1, "Yangi loyiha", "Ctrl+N"); add (2, "Ochish...", "Ctrl+O"); m.addSeparator();
                    add (3, "Saqlash", "Ctrl+S"); add (4, "Boshqa nom bilan saqlash..."); m.addSeparator();
                    add (5, "Audio import...", "Ctrl+I"); add (6, "WAV'ga eksport (mixdown)...", "Ctrl+Shift+E"); m.addSeparator(); add (9, "Chiqish"); break;
            case 1: add (10, "Bekor qilish (Undo)", "Ctrl+Z"); add (11, "Qaytarish (Redo)", "Ctrl+Y"); m.addSeparator();
                    add (12, "Kursor joyida bo'lish", "Ctrl+E"); add (13, "Nusxa", "Ctrl+D"); add (14, "O'chirish", "Del"); break;
            case 2: add (20, "Audio trek qo'shish", "Ctrl+T"); add (21, "Instrument trek qo'shish"); add (22, "Marker qo'shish (kursor joyida)");
                    break;
            case 3: add (30, "Audio sozlamalari (ASIO, drayver, bufer)..."); add (31, "Yozish uchun kirishlarni yangilash"); break;
            case 4: add (40, "MIDI import...", ""); add (41, "MIDI qurilmalarni qayta skanerlash"); break;
            case 5: add (50, mixerH > 0 ? "Mikserni yashirish" : "Mikserni ko'rsatish", "Ctrl+M"); break;
            case 6: add (60, "Plagin qo'shish (tanlangan trekka)..."); add (61, "VST3 plaginlarni skanerlash va boshqarish..."); break;
            case 7: add (70, "Klaviatura qisqa tugmalari"); add (71, "AZ MUS haqida"); break;
        }
        return m;
    }

    void menuItemSelected (int id, int) override
    {
        switch (id)
        {
            case 1: app.newProject(); break;
            case 2: chooseFile (U ("Loyihani ochish"), "*.azmus;*.tracktionedit", false, [this] (juce::File f) { if (! app.openProject (f)) juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "AZ MUS", U ("Loyihani ochib bo'lmadi")); }); break;
            case 3: app.saveIfPossible(); break;
            case 4: chooseFile (U ("Boshqa nom bilan saqlash"), "*.azmus", true, [this] (juce::File f) { app.saveAs (f); editChanged(); }); break;
            case 5: chooseFile (U ("Audio import"), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg", false, [this] (juce::File f) { insertFileAtCursor (f); }); break;
            case 6: exportMix(); break;
            case 9: juce::JUCEApplication::getInstance()->systemRequestedQuit(); break;
            case 10: if (app.edit) app.edit->getUndoManager().undo(); break;
            case 11: if (app.edit) app.edit->getUndoManager().redo(); break;
            case 12: splitSelected(); break;
            case 13: for (auto* c : app.selection.getItemsOfType<te::Clip>()) lanes.duplicate (*c); break;
            case 14: deleteSelected(); break;
            case 20: addTrack (false); break;
            case 21: addTrack (true); break;
            case 22: if (app.edit) { auto p = app.edit->getTransport().getPosition(); app.edit->getMarkerManager().createMarker (-1, p, 8s, nullptr); ruler.repaint(); } break;
            case 30: showAudioSettings(); break;
            case 31: if (app.edit) app.setupInputs(); inspector.refresh(); break;
            case 40: chooseFile (U ("MIDI import"), "*.mid;*.midi", false, [this] (juce::File f) { insertFileAtCursor (f); }); break;
            case 41: app.engine.getDeviceManager().rescanMidiDeviceList(); if (app.edit) app.setupInputs(); break;
            case 50: mixerH = mixerH > 0 ? 0 : 236; resized(); break;
            case 60: if (auto* t = inspector.track()) showPluginMenu (app, *t, [this] { inspector.refresh(); mixer.rebuild(); }); break;
            case 61: showPluginManager(); break;
            case 70: showShortcuts(); break;
            case 71: showAbout(); break;
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
                                                    ok ? U ("Eksport tayyor:\n") + f.withFileExtension ("wav").getFullPathName() : U ("Eksport qilib bo'lmadi (loyiha bo'sh?)"));
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
        o.dialogTitle = U ("Audio sozlamalari \xc2\xb7 AZ MUS");
        o.dialogBackgroundColour = bg1;
        auto* c = new juce::AudioDeviceSelectorComponent (app.engine.getDeviceManager().deviceManager, 0, 64, 2, 64, true, false, true, false);
        c->setSize (560, 520); o.content.setOwned (c); o.useNativeTitleBar = true; o.resizable = true;
        o.launchAsync();
    }
    void showPluginManager()
    {
        juce::DialogWindow::LaunchOptions o;
        o.dialogTitle = U ("Plaginlar menejeri \xc2\xb7 AZ MUS");
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
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, U ("Qisqa tugmalar"), U (
            "Space \xe2\x80\x94 ijro / to'xtatish\nR yoki * \xe2\x80\x94 yozish\nL \xe2\x80\x94 loop\nC \xe2\x80\x94 metronom\nHome / Enter \xe2\x80\x94 boshiga\n"
            "\xe2\x86\x90 \xe2\x86\x92 \xe2\x80\x94 bir takt\nG \xe2\x80\x94 snap\nCtrl+E \xe2\x80\x94 bo'lish\nCtrl+D \xe2\x80\x94 nusxa\nDel \xe2\x80\x94 o'chirish\n"
            "Ctrl+Z / Ctrl+Y \xe2\x80\x94 undo / redo\nCtrl+S \xe2\x80\x94 saqlash\nCtrl+T \xe2\x80\x94 trek qo'shish\nCtrl+M \xe2\x80\x94 mikser\n"
            "+ / - yoki Ctrl+g'ildirak \xe2\x80\x94 zoom\nChizg'ichni sudrash \xe2\x80\x94 loop zonasi\nChizg'ichga 2x \xe2\x80\x94 marker (bo'lim)\n"
            "Bo'sh joyga 2x \xe2\x80\x94 MIDI klip + piano roll\nKlip chetini sudrash \xe2\x80\x94 uzunlik\n\nPiano roll: 1-4 grid, Q kvantlash, o'ng tugma o'chirish"));
    }
    void showAbout()
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, "AZ MUS " + juce::String (JUCE_APPLICATION_VERSION_STRING), U (
            "Digital Audio Workstation\nAzimxodjayev Music \xc2\xb7 Alif Records\n\n"
            "Powered by Tracktion Engine (Personal licence)\nJUCE \xc2\xb7 Airwindows (MIT)\n\n\xc2\xa9 2026 Azimxodjayev Music"));
    }

    void zoomToFit()
    {
        const double len = juce::jmax (30.0, app.edit->getLength().inSeconds() + 6.0);
        vs.startSec = 0; vs.pxPerSec = juce::jlimit (4.0, 400.0, (double) juce::jmax (600, lanes.getWidth()) / len); vs.changed();
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
        // kursor sahifadan chiqsa — avtomatik siljitish
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
    juce::TextButton toolAdd, toolDraw, toolSplit, toolSnap;
    std::unique_ptr<PianoRollWindow> pianoRoll;
    std::unique_ptr<juce::FileChooser> chooser;
    int mixerH = 236, recTick = 0, slow = 0; double lastPos = -1; bool fitted = false;
};
}
