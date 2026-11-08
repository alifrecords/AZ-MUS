// =============================================================================
//  AZ MUS — asosiy oyna
// =============================================================================
#pragma once
#include "Views.h"
#include "Dop.h"
#include "AudioEditor.h"
#include "PianoRoll.h"
#include "Connections.h"
#include "Hub.h"

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
    { "F7",                   "Direct Offline Processing (Envelope, AUTO balandlik, Gain, Normalize, Fade ...)" },
    { "Q",                    "Quantize (tanlangan kliplar / MIDI notalar)" },
    { "Ctrl+Alt+B",           "Render Selection (Dry / Channel / Complete Signal Path)" },
    { "Mikser \xe2\x86\x97 / chetidan sudrash", "Mikserni alohida oynaga ajratish" },
    { "Alt (bosib turish)",   "Qaychi: bosilgan joyda kesish (zarba boshiga magnit), sudrash \xe2\x80\x94 nusxa (Cubase)" },
    { "Alt + insert/send",    "Plaginni yoqish / o'chirish (bypass)" },
    { "Tap, so'ng Probel",    "Temp sanash; ijroda Enter yoki to'xtatilganda qo'llanadi" },
    { "Trek nomini sudrash",  "Treklar tartibini almashtirish (mikserda ham)" },
};

class MainComponent : public juce::Component, public juce::MenuBarModel, public juce::DragAndDropContainer, private juce::ChangeListener, private juce::Timer
{
public:
    explicit MainComponent (AppEngine& a)
        : app (a), top (a, this), gridBar (a, vs), inspector (a, vs), ruler (a, vs), headers (a, vs), lanes (a, vs), mixer (a), media (a), status (a), editor (a, vs)
    {
        setOpaque (true);
        for (auto* c : std::initializer_list<juce::Component*> { &top, &gridBar, &inspector, &ruler, &headers, &lanes, &mixer, &media, &status, &toolAdd, &toolSnap, &toolBounce })
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
        top.onTheme = [this] { if (onThemeToggle) onThemeToggle(); };
        top.onTogglePanel = [this] (int p) { togglePanel (p); };

        lanes.onOpenPianoRoll = [this] (te::MidiClip& c) { openPianoRoll (c); };
        lanes.onSelectionChanged = [this] { inspector.refresh(); headers.repaint(); };
        headers.onSelectionChanged = [this] { inspector.refresh(); lanes.repaint(); };
        mixer.onTrackLevel = [this] (te::EditItemID id, float l) { headers.levels[id] = l; };
        media.onInsertFile = [this] (const juce::File& f) { insertFileAtCursor (f); };
        inspector.onRoutingChanged = [this] { mixer.rebuild(); lanes.repaint(); headers.repaint(); };
        top.onUiZoom = [this] (int d) { if (onUiZoom) onUiZoom (d); };
        top.getLcd().positionText = [this] { return app.edit ? formatTime (*app.edit, app.edit->getTransport().getPosition(), vs, sampleRate()) : juce::String(); };
        top.getLcd().onFormatClick = [this] { gridBar.showFormatMenu(); };
        top.getTempo().onChanged = [this] { refreshAll(); };
        gridBar.onQuantize = [this] { quantizeSelected(); };
        gridBar.onChanged = [this] { toolSnap.setToggleState (vs.snap, juce::dontSendNotification); savePrefs(); refreshAll(); };
        lanes.onDop = [this] { openDop(); };
        lanes.onRenderSel = [this] { openRender(); };
        lanes.onBounce = [this] { bounce(); };
        lanes.onQuantize = [this] { quantizeSelected(); };
        mixer.onDetach = [this] { toggleMixerWindow(); };
        // pastki zona: MIKSER | EDITOR (Cubase kabi)
        addChildComponent (editor);
        for (auto* b : { &lowMixB, &lowEdB }) { addAndMakeVisible (b); b->setClickingTogglesState (true); b->setRadioGroupId (601); b->getProperties().set ("radius", 4.0f); b->setWantsKeyboardFocus (false); }
        lowMixB.setButtonText (U ("MIKSER (F3)")); lowEdB.setButtonText (U ("EDITOR (Enter)"));
        lowMixB.setToggleState (true, juce::dontSendNotification);
        lowMixB.onClick = [this] { editorOpen = false; showMixer = true; resized(); };
        lowEdB.onClick = [this] { editorOpen = true; resized(); };
        editor.onClose = [this] { editorOpen = false; lowMixB.setToggleState (true, juce::dontSendNotification); resized(); };
        editor.onChanged = [this] { lanes.repaint(); mixer.rebuild(); refreshAll(); };
        editor.onToast = [this] (const juce::String& m) { toast (m); };
        lanes.onOpenAudioEditor = [this] (te::WaveAudioClip& c) { openAudioEditor (c); };
        // plagin oynalari ochiq bo'lsa ham transport ishlasin
        pluginWindowTransport = [this] (int c) { if (c == 0) togglePlay(); if (c == 1) stop(); if (c == 2 && app.edit) { app.edit->getTransport().setPosition (te::TimePosition()); refreshAll(); } };
        pluginWindowKeys = [this] (const juce::KeyPress& k) { return keyPressed (k); };
        pluginWindowIsPlaying = [this] { return app.edit && app.edit->getTransport().isPlaying(); };
        pluginWindowPinDefault = app.engine.getPropertyStorage().getPropertiesFile().getBoolValue ("azPluginPin", true);
        mixer.onRoutingChanged = [this] { inspector.refresh(); headers.repaint(); lanes.repaint(); };
        headers.onAddTrack = [this] { showAddTrackMenu(); };
        headers.onTracksReordered = [this] { mixer.rebuild(); refreshAll(); };
        mixer.onTracksReordered = [this] { refreshAll(); };
        inspector.onPluginToggled = [this] { mixer.rebuildIfNeeded(); };
        lanes.onAudioImported = [this] (te::WaveAudioClip& c, const juce::File& f) { if (auto m = media.applySync (c, f); m.isNotEmpty()) toast (U ("Sinxron: ") + m); else rateHint (f); };
        lanes.onRestoreOriginal = [this] (te::WaveAudioClip& c)
        {
            juce::Array<te::WaveAudioClip*> cl; for (auto* x : selectedAudioClips()) cl.addIfNotAlreadyThere (x); cl.addIfNotAlreadyThere (&c);
            toast (restoreClips (cl));
        };
        lanes.onConvertRate = [this] (te::WaveAudioClip& c, double rate)
        {
            // tanlangan barcha audio kliplar (bosilgan klip ham) birga o'giriladi
            juce::Array<te::WaveAudioClip*> cl;
            for (auto* x : selectedAudioClips()) cl.addIfNotAlreadyThere (x);
            cl.addIfNotAlreadyThere (&c);
            toast (convertRates (cl, rate > 0 ? rate : sampleRate()));
        };

        auto& pf = app.engine.getPropertyStorage().getPropertiesFile();
        showInsp = pf.getBoolValue ("azShowInsp", true); showMedia = pf.getBoolValue ("azShowMedia", true); showMixer = pf.getBoolValue ("azShowMixer", true);
        vs.gridMode = pf.getIntValue ("azGridMode", ViewState::gridDivision); vs.division = pf.getDoubleValue ("azGridDiv", 0.5);
        vs.snapType = pf.getIntValue ("azSnapType", 0); vs.timeFormat = pf.getIntValue ("azTimeFmt", 0); vs.fps = pf.getIntValue ("azFps", 25);
        vs.quantize = pf.getDoubleValue ("azQuant", 0.5); vs.linkQ = pf.getBoolValue ("azLinkQ", true);
        vs.transientMagnet = pf.getBoolValue ("azMagnet", true);
        gridBar.refresh();
        top.setPanels (showInsp, showMedia, showMixer);

        vs.addChangeListener (this);
        app.addChangeListener (this);
        app.onEditChanged = [this] { editChanged(); };
        setWantsKeyboardFocus (true);
        startTimerHz (30);
        editChanged();
    }
    ~MainComponent() override
    {
        app.engine.getPropertyStorage().getPropertiesFile().setValue ("azPluginPin", pluginWindowPinDefault);
        pluginWindowTransport = nullptr; pluginWindowKeys = nullptr; pluginWindowIsPlaying = nullptr;
        app.onEditChanged = nullptr; app.removeChangeListener (this); vs.removeChangeListener (this);
        pianoRoll.reset(); connections.reset(); dopWindow.reset(); renderWindow.reset();
        if (mixerWindow) { mixerWindow->clearContentComponent(); mixerWindow.reset(); }
    }
    // Bosh sahifa bilan aloqa (MainWindow o'rnatadi)
    std::function<void()> onRequestNew, onRequestHome, onThemeToggle;
    std::function<void (juce::File)> onRequestOpen;
    // Bosh sahifa kartasi uchun: aranjirovka surati + BPM/tonallik/uzunlik
    void saveProjectSnapshot()
    {
        if (! app.edit || app.editFile == juce::File()) return;
        const auto key = app.edit->state.getProperty ("azKey", "A minor").toString();
        hub::writeMeta (app.editFile, app.edit->tempoSequence.getTempo (0)->getBpm(), key, app.edit->getLength().inSeconds(), te::getAudioTracks (*app.edit).size());
        // aranjirovka xaritasi: har trek — qator, har klip — trek rangidagi blok (ichida to'lqin/nota izi)
        auto tracks = te::getAudioTracks (*app.edit);
        int clips = 0; for (auto* t : tracks) clips += t->getClips().size();
        if (clips == 0) { hub::thumbFile (app.editFile).deleteFile(); return; }    // bo'sh loyiha: namunaviy rasm qoladi
        const int W = 480, H = 394;
        juce::Image img (juce::Image::ARGB, W, H, true);
        juce::Graphics g (img);
        g.fillAll (juce::Colour (0xff141218));
        const double len = juce::jmax (8.0, app.edit->getLength().inSeconds());
        const int rows = juce::jlimit (1, 10, tracks.size());
        const float pad = 14, rh = (H - 2 * pad) / (float) rows;
        g.setColour (juce::Colours::white.withAlpha (0.04f));
        for (int i = 1; i < 8; ++i) g.drawVerticalLine (juce::roundToInt (pad + (W - 2 * pad) * i / 8.0f), pad, H - pad);
        for (int r = 0; r < rows; ++r)
        {
            auto* t = tracks[r];
            const auto col = t->getColour().isTransparent() ? trackColours[r % 8] : t->getColour();
            for (auto* c : t->getClips())
            {
                const auto pos = c->getPosition();
                const float x0 = pad + (float) (pos.getStart().inSeconds() / len) * (W - 2 * pad);
                const float x1 = pad + (float) (pos.getEnd().inSeconds() / len) * (W - 2 * pad);
                auto rc = juce::Rectangle<float> (x0, pad + r * rh + 3, juce::jmax (4.0f, x1 - x0 - 2), rh - 6);
                g.setColour (col.withAlpha (0.55f)); g.fillRoundedRectangle (rc, 4);
                g.setColour (col.withAlpha (0.95f)); g.fillRoundedRectangle (rc.withHeight (juce::jmin (5.0f, rc.getHeight() * 0.2f)), 2);
                juce::Random rnd (c->itemID.getRawID() & 0x7fffffff);
                g.setColour (juce::Colours::white.withAlpha (0.55f));
                const bool midi = dynamic_cast<te::MidiClip*> (c) != nullptr;
                for (float x = rc.getX() + 2; x < rc.getRight() - 2; x += midi ? 7.0f : 2.0f)
                {
                    if (midi) { const float y = rc.getY() + 7 + rnd.nextFloat() * (rc.getHeight() - 12); g.fillRect (x, y, 5.0f, 2.0f); }
                    else { const float a = (0.15f + 0.85f * rnd.nextFloat()) * (rc.getHeight() - 10) * 0.45f; g.fillRect (x, rc.getCentreY() + 2 - a, 1.0f, a * 2); }
                }
            }
        }
        hub::saveImage (img, hub::thumbFile (app.editFile));
    }
    // ---------------------------------------------------------------- sample rate moslashtirish
    juce::String convertRates (const juce::Array<te::WaveAudioClip*>& clips, double toRate)
    {
        if (! app.edit || clips.isEmpty()) return U ("Audio klip yo'q");
        app.edit->getUndoManager().beginNewTransaction ("Sample rate");
        int done = 0, skipped = 0, failed = 0; juce::String err;
        juce::MouseCursor::showWaitCursor();
        for (auto* c : clips)
        {
            const double fr = dop::fileRate (app.engine, c->getSourceFileReference().getFile());
            if (std::abs (fr - toRate) < 0.5) { ++skipped; continue; }
            if (dop::convertClipRate (*c, toRate, &err)) ++done; else ++failed;
        }
        juce::MouseCursor::hideWaitCursor();
        lanes.repaint();
        juce::String m = U ("Format: ") + juce::String (done) + U (" ta audio ") + juce::String (toRate, 0) + U (" Hz / 24-bit WAV'ga o'girildi");
        if (skipped > 0) m << U (" \xc2\xb7 ") << skipped << U (" tasi allaqachon mos");
        if (failed > 0) m << U (" \xc2\xb7 ") << failed << U (" tasi xato: ") << err;
        return m;
    }
    juce::String restoreClips (const juce::Array<te::WaveAudioClip*>& clips)
    {
        if (! app.edit || clips.isEmpty()) return U ("Audio klip yo'q");
        app.edit->getUndoManager().beginNewTransaction ("Asl holiga");
        int n = 0; for (auto* c : clips) if (AppEngine::restoreOriginal (*c)) ++n;
        lanes.repaint();
        return n > 0 ? U ("Asl holiga qaytarildi: ") + juce::String (n) + U (" ta audio (asl tempi, tonalligi, tezligi)") : U ("Audio allaqachon asl holida");
    }
    void restoreAll()
    {
        juce::Array<te::WaveAudioClip*> cl;
        if (app.edit) for (auto* t : te::getAudioTracks (*app.edit)) for (auto* c : t->getClips()) if (auto* w = dynamic_cast<te::WaveAudioClip*> (c)) cl.add (w);
        toast (restoreClips (cl));
    }
    void convertAllToDaw()
    {
        juce::Array<te::WaveAudioClip*> cl;
        if (app.edit) for (auto* t : te::getAudioTracks (*app.edit)) for (auto* c : t->getClips()) if (auto* w = dynamic_cast<te::WaveAudioClip*> (c)) cl.add (w);
        toast (convertRates (cl, sampleRate()));
    }
    void rateHint (const juce::File& f)
    {
        const double fr = dop::fileRate (app.engine, f), dr = app.engine.getDeviceManager().getSampleRate();
        if (fr > 0 && dr > 0 && std::abs (fr - dr) > 0.5)
            toast (U ("Fayl ") + juce::String (fr, 0) + U (" Hz, DAW ") + juce::String (dr, 0) + U (" Hz \xe2\x80\x94 o'ng tugma \xe2\x86\x92 Formatni moslashtirish"));
    }
    double sampleRate() const { auto sr = app.engine.getDeviceManager().getSampleRate(); return sr > 0 ? sr : 48000.0; }
    void savePrefs()
    {
        auto& pf = app.engine.getPropertyStorage().getPropertiesFile();
        pf.setValue ("azGridMode", vs.gridMode); pf.setValue ("azGridDiv", vs.division); pf.setValue ("azSnapType", vs.snapType);
        pf.setValue ("azTimeFmt", vs.timeFormat); pf.setValue ("azFps", vs.fps); pf.setValue ("azQuant", vs.quantize); pf.setValue ("azLinkQ", vs.linkQ); pf.setValue ("azMagnet", vs.transientMagnet);
    }

    // ---------------------------------------------------------------- Quantize (Q)
    juce::String quantizeClips (const juce::Array<te::Clip*>& clips)
    {
        if (! app.edit || clips.isEmpty()) return U ("Quantize: avval klip(lar)ni tanlang.");
        auto& ts = app.edit->tempoSequence;
        const double q = gridBar.quantizeBeats();
        app.edit->getUndoManager().beginNewTransaction ("Quantize");
        int notes = 0, moved = 0;
        for (auto* c : clips)
        {
            if (auto* mc = dynamic_cast<te::MidiClip*> (c))
            {
                auto& seq = mc->getSequence();
                for (auto* n : seq.getNotes())
                {
                    const double abs = ts.toBeats (mc->getTimeOfContentBeat (n->getStartBeat())).inBeats();
                    const double qa = std::round (abs / q) * q;
                    const auto newStart = mc->getContentBeatAtTime (ts.toTime (te::BeatPosition::fromBeats (qa)));
                    if (std::abs (newStart.inBeats() - n->getStartBeat().inBeats()) > 1e-6) { n->setStartAndLength (newStart, n->getLengthBeats(), &app.edit->getUndoManager()); ++notes; }
                }
            }
            else
            {
                const auto st = snapToGrid (*app.edit, c->getPosition().getStart().inSeconds(), q);
                if (st != c->getPosition().getStart()) { c->setStart (st, false, true); ++moved; }
            }
        }
        return U ("Quantize ") + ViewState::divisionName (q) + ": " + juce::String (moved) + U (" klip, ") + juce::String (notes) + U (" nota");
    }
    void quantizeSelected() { toast (quantizeClips (selectedClips())); refreshAll(); }

    // ---------------------------------------------------------------- Audio Editor (AudioWarp / AZ Pitch / Xor / tozalash)
    void openAudioEditor (te::WaveAudioClip& c)
    {
        editorOpen = true; lowEdB.setToggleState (true, juce::dontSendNotification);
        editor.setClip (&c);
        resized();
    }
    AudioEditor& getAudioEditor() { return editor; }

    // ---------------------------------------------------------------- Direct Offline Processing (F7)
    juce::Array<te::WaveAudioClip*> selectedAudioClips()
    {
        juce::Array<te::WaveAudioClip*> r;
        for (auto* c : selectedClips()) if (auto* w = dynamic_cast<te::WaveAudioClip*> (c)) r.add (w);
        return r;
    }
    void openDop()
    {
        auto cl = selectedAudioClips();
        if (cl.isEmpty()) { toast (U ("Direct Offline Processing: avval audio klip(lar)ni tanlang.")); return; }
        dopWindow = std::make_unique<DopWindow> (app, cl, [this] { lanes.repaint(); });
        dopWindow->onClose = [this] { juce::MessageManager::callAsync ([this] { dopWindow.reset(); }); };
    }
    // ---------------------------------------------------------------- Render Selection
    void openRender()
    {
        if (selectedClips().isEmpty()) { toast (U ("Render Selection: avval klip(lar)ni tanlang.")); return; }
        renderWindow = std::make_unique<RenderWindow> (app, [this] (const RenderOptions& o)
        {
            if (app.edit && app.edit->getTransport().isPlaying()) app.edit->getTransport().stop (false, false);
            toast (renderSelection (app, selectedClips(), o));
            app.selection.deselectAll(); mixer.rebuild(); refreshAll();
            juce::MessageManager::callAsync ([this] { renderWindow.reset(); });
        }, [this] { renderWindow.reset(); });
    }
    // ---------------------------------------------------------------- mikserni alohida oynaga ajratish
    struct MixerWindow : public juce::DocumentWindow
    {
        MixerWindow() : juce::DocumentWindow (juce::String::fromUTF8 ("AZ MUS  \xc2\xb7  ") + U ("Mikser"), bg0, juce::DocumentWindow::allButtons) {}
        std::function<void()> onClose;
        void closeButtonPressed() override { if (onClose) onClose(); }
    };
    void toggleMixerWindow()
    {
        if (mixerWindow)
        {
            auto& pf = app.engine.getPropertyStorage().getPropertiesFile();
            pf.setValue ("azMixerWin", mixerWindow->getWindowStateAsString());
            mixerWindow->clearContentComponent(); mixerWindow.reset();
            mixer.detached = false; addAndMakeVisible (mixer); showMixer = true;
            top.setPanels (showInsp, showMedia, showMixer); resized(); mixer.repaint();
            return;
        }
        auto global = mixer.isShowing() ? mixer.getScreenBounds() : juce::Rectangle<int> (100, 100, 1200, 260);
        mixerWindow = std::make_unique<MixerWindow>();
        mixerWindow->setUsingNativeTitleBar (true);
        removeChildComponent (&mixer);
        mixer.detached = true; mixer.setVisible (true);
        mixer.setSize (juce::jmax (900, global.getWidth()), juce::jmax (240, global.getHeight()));
        mixerWindow->setContentNonOwned (&mixer, true);
        mixerWindow->setResizable (true, false); mixerWindow->setResizeLimits (600, 200, 8000, 1400);
        const auto saved = app.engine.getPropertyStorage().getPropertiesFile().getValue ("azMixerWin");
        if (saved.isEmpty() || ! mixerWindow->restoreWindowStateFromString (saved)) mixerWindow->setTopLeftPosition (global.getX(), juce::jmax (40, global.getY() - 40));
        mixerWindow->onClose = [this] { juce::MessageManager::callAsync ([this] { if (mixerWindow) toggleMixerWindow(); }); };
        mixerWindow->setVisible (true); mixerWindow->toFront (true);
        resized(); repaint();
        toast (U ("Mikser alohida oynada: istalgan displeyga olib o'ting. Yopsangiz joyiga qaytadi."));
    }

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
        if (auto* w = findParentComponentOfClass<juce::DocumentWindow>()) w->setName (juce::String::fromUTF8 ("AZ MUS  \xc2\xb7  ") + app.editFile.getFileNameWithoutExtension());
    }

    // ---------------------------------------------------------------- layout
    void resized() override
    {
        auto r = getLocalBounds();
        top.setBounds (r.removeFromTop (TopBar::menuH + TopBar::barH));
        status.setBounds (r.removeFromBottom (26));
        inspector.setVisible (showInsp); media.setVisible (showMedia);
        editor.setVisible (editorOpen);
        const bool lowerVisible = editorOpen || (showMixer && ! mixerWindow);
        lowMixB.setVisible (lowerVisible); lowEdB.setVisible (lowerVisible);
        if (editorOpen)
        {
            if (! mixerWindow) mixer.setVisible (false);
            auto lz = r.removeFromBottom (juce::jlimit (320, 560, getHeight() * 46 / 100));
            auto tb = lz.removeFromTop (24).reduced (8, 2);
            lowMixB.setBounds (tb.removeFromLeft (110)); tb.removeFromLeft (4); lowEdB.setBounds (tb.removeFromLeft (120));
            editor.setBounds (lz);
        }
        else if (! mixerWindow)
        {
            mixer.setVisible (showMixer);
            if (showMixer)
            {
                auto lz = r.removeFromBottom (juce::jlimit (236, 300, getHeight() * 34 / 100) + 24);
                auto tb = lz.removeFromTop (24).reduced (8, 2);
                lowMixB.setBounds (tb.removeFromLeft (110)); tb.removeFromLeft (4); lowEdB.setBounds (tb.removeFromLeft (120));
                mixer.setBounds (lz);
            }
        }
        if (showInsp) inspector.setBounds (r.removeFromLeft (236));
        if (showMedia) media.setBounds (r.removeFromRight (260));
        gridBar.setBounds (r.removeFromTop (32));
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
        if (p == 2 && mixerWindow) { mixerWindow->setVisible (! mixerWindow->isVisible()); if (mixerWindow->isVisible()) mixerWindow->toFront (true); return; }
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
        int code = k.getKeyCode();
        if (code >= 'a' && code <= 'z') code -= 'a' - 'A';       // Linux/macOS: Ctrl+harf kichik harf kodi bilan kelishi mumkin
        const auto ch = juce::CharacterFunctions::toLowerCase (k.getTextCharacter());
        auto plainKey = [&] (juce_wchar c) { return ! ctrl && ! alt && (code == (int) juce::CharacterFunctions::toUpperCase (c) || ch == c); };

        // F klavishlar (Cubase)
        if (code == KP::F3Key) { togglePanel (2); return true; }
        if (code == KP::F4Key) { showConnections(); return true; }
        if (code == KP::F7Key) { openDop(); return true; }
        if (code == KP::returnKey && ! top.getTempo().tapMode && ! ctrl && ! alt)
            if (auto cl = selectedAudioClips(); ! cl.isEmpty()) { openAudioEditor (*cl[0]); return true; }
        // Tap rejimi: Probel = tap, Enter = qo'llash, Esc = chiqish
        if (auto& tb = top.getTempo(); tb.tapMode)
        {
            if (code == KP::spaceKey) { tb.tap(); return true; }
            if (code == KP::returnKey) { tb.applyPending(); tb.exitTapMode(); return true; }
            if (code == KP::escapeKey) { tb.pendingBpm = 0; tb.exitTapMode(); return true; }
        }
        if (ctrl && alt && code == 'B') { openRender(); return true; }
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
        if (ctrl && shift && code == 'W') { menuItemSelected (8, 0); return true; }
        if (ctrl && shift && code == 'O')
        {
            auto cl = selectedAudioClips();
            if (cl.isEmpty()) restoreAll(); else toast (restoreClips (cl));
            return true;
        }
        if (ctrl && code == 'O') { menuItemSelected (2, 0); return true; }
        if (ctrl && code == 'I') { menuItemSelected (5, 0); return true; }
        if (ctrl && code == KP::downKey) { zoomTracks (1.25); return true; }
        if (ctrl && code == KP::upKey)   { zoomTracks (1 / 1.25); return true; }
        // bitta harfli (Daftar DAW)
        if (shift && ! ctrl && code == 'H') { zoomTracks (1.25); return true; }
        if (shift && ! ctrl && code == 'G') { zoomTracks (1 / 1.25); return true; }
        if (! shift && plainKey ('b')) { bounce(); return true; }
        if (! shift && plainKey ('q')) { quantizeSelected(); return true; }
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
            case 0: add (1, "Yangi loyiha...", "Ctrl+N"); add (2, "Ochish...", "Ctrl+O"); add (8, "Bosh sahifa (loyihani yopish)", "Ctrl+Shift+W"); m.addSeparator();
                    add (3, "Saqlash", "Ctrl+S"); add (4, "Boshqa nom bilan saqlash...", "Ctrl+Shift+S"); m.addSeparator();
                    add (5, "Audio import...", "Ctrl+I"); add (6, "WAV'ga eksport (mixdown)...", "Ctrl+Shift+E"); add (7, "WAV 32-bit float eksport (mastering uchun)..."); m.addSeparator(); add (9, "Chiqish"); break;
            case 1: add (10, "Bekor qilish (Undo)", "Ctrl+Z"); add (11, "Qaytarish (Redo)", "Ctrl+Y"); m.addSeparator();
                    add (12, "Kursor joyida kesish", "S"); add (13, "Nusxa", "Ctrl+D"); add (14, "O'chirish", "Del"); m.addSeparator();
                    add (15, "Bounce Selection", "B"); add (16, "Locatorlar = tanlov", "P"); add (17, "Hammasini belgilash", "Ctrl+A"); m.addSeparator();
                    add (18, "Tanlash", "1", vs.tool == 0); add (19, "Qaychi", "2", vs.tool == 1); add (20, "O'chirg'ich", "3", vs.tool == 2); add (21, "Mute", "4", vs.tool == 3); break;
            case 2: add (30, "Audio trek qo'shish", "Ctrl+T"); add (31, "Instrument trek qo'shish"); add (32, "Marker qo'shish (kursor joyida)"); break;
            case 3: add (42, "Audio ulanishlar (Inputs, Outputs, FX, Control Room)...", "F4"); add (40, "Audio qurilma sozlamalari (ASIO, drayver, bufer)...");
                    add (43, "FX kanal qo'shish..."); add (41, "Yozish uchun kirishlarni yangilash"); m.addSeparator();
                    add (47, "Barcha audiolarni DAW formatiga moslashtirish (sample rate)"); add (48, "Barcha audiolarni asl holiga qaytarish (tempo, ton)"); m.addSeparator(); add (44, "Direct Offline Processing...", "F7"); add (45, "Render Selection...", "Ctrl+Alt+B"); add (46, "Quantize", "Q"); break;
            case 4: add (50, "MIDI import..."); add (51, "MIDI qurilmalarni qayta skanerlash"); break;
            case 5: add (60, "Inspektor", "Alt+I", showInsp); add (61, "Media brauzer", "F5", showMedia); add (62, "Mikser", "F3", showMixer); add (66, "Mikserni alohida oynaga ajratish / qaytarish", "", mixerWindow != nullptr); m.addSeparator();
                    add (63, "Hammasini ekranga sig'dirish", "Ctrl+Shift+A"); add (64, "Kattalashtirish", "H"); add (65, "Kichiklashtirish", "G"); break;
            case 6: add (70, "Plagin qo'shish (tanlangan trekka)..."); add (72, "Master insert qo'shish (mastering)..."); add (73, "Control Room insert qo'shish (faqat monitor)..."); add (74, "Monitor Match: monitor o'lchov faylini yuklash (REW)..."); m.addSeparator();
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
            // loyiha almashtirish faqat MainWindow orqali (UI avval yopiladi -> keyin Edit almashadi; DAW yiqilmaydi)
            case 1: if (onRequestNew) onRequestNew(); break;
            case 2: chooseFile (U ("Loyihani ochish"), "*.azmus;*.tracktionedit", false, [this] (juce::File f) { if (onRequestOpen) onRequestOpen (f); }); break;
            case 8: if (onRequestHome) onRequestHome(); break;
            case 3: app.saveIfPossible(); saveProjectSnapshot(); toast (U ("Saqlandi")); break;
            case 4: chooseFile (U ("Boshqa nom bilan saqlash"), "*.azmus", true, [this] (juce::File f) { app.saveAs (f); editChanged(); }); break;
            case 5: chooseFile (U ("Audio import"), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg", false, [this] (juce::File f) { insertFileAtCursor (f); }); break;
            case 6: exportMix(); break;
            case 7: exportMix (32); break;
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
            case 44: openDop(); break;
            case 47: convertAllToDaw(); break;
            case 48: restoreAll(); break;
            case 45: openRender(); break;
            case 46: quantizeSelected(); break;
            case 66: toggleMixerWindow(); break;
            case 72: if (! showMixer) togglePanel (2);
                     showPluginPicker (app, localAreaToGlobal (getLocalBounds().withSizeKeepingCentre (10, 10)), [this] (const PickItem& it)
                     { if (auto p = insertMaster (app, it)) p->showWindowExplicitly(); mixer.repaint(); });
                     break;
            case 73: if (! showMixer) togglePanel (2);
                     showPluginPicker (app, localAreaToGlobal (getLocalBounds().withSizeKeepingCentre (10, 10)), [this] (const PickItem& it)
                     { if (auto p = insertControlRoom (app, it)) p->showWindowExplicitly(); mixer.rebuild(); });
                     break;
            case 74: chooseFile (U ("Monitor o'lchovi (REW → Export measurement as text)"), "*.txt;*.frd;*.csv", false, [this] (juce::File f)
                     {
                         auto* mmp = Routing (app).monitorMatch (true);
                         const bool ok = mmp != nullptr && mmp->loadMeasurement (f);
                         toast (ok ? U ("Monitor Match: o'lchov yuklandi — ") + f.getFileName() : U ("O'lchov faylini o'qib bo'lmadi"));
                         mixer.rebuild();
                     }); break;
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
        m.addItem (3, U ("AZ BEAT \xe2\x80\x94 beat yaratish (padlar + sekvenser)"));
        juce::PopupMenu vi; auto types = app.engine.getPluginManager().knownPluginList.getTypes();
        for (int i = 0; i < types.size(); ++i) if (types[i].isInstrument) vi.addItem (100 + i, types[i].name);
        m.addSubMenu (U ("Instrument trek (VST3i)"), vi, vi.getNumItems() > 0);
        m.showMenuAsync ({}, [this, types] (int r)
        {
            if (r == 1) addTrack (false);
            if (r == 2) addTrack (true);
            if (r == 3) addBeat();
            if (r >= 100) { app.edit->getUndoManager().beginNewTransaction ("Trek"); auto d = types[r - 100]; if (auto* t = app.addInstrumentTrack (d.name, &d)) selectTrack (t); }
        });
    }
    void addBeat()
    {
        if (! app.edit) return;
        app.edit->getUndoManager().beginNewTransaction ("AZ BEAT");
        if (auto* t = addBeatTrack (app))
        {
            selectTrack (t); mixer.rebuild();
            if (auto* bp = beatPluginOf (*t)) bp->showWindowExplicitly();
            toast (U ("AZ BEAT: padlarga sample sudrab tashlang, sekvenserda bosing \xe2\x80\x94 Play"));
        }
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
        if (midi) app.importMidi (*t, f, at);
        else if (auto c = app.importAudio (*t, f, at)) { if (auto m = media.applySync (*c, f); m.isNotEmpty()) toast (U ("Sinxron: ") + m); else rateHint (f); }
        selectTrack (t);
    }
    void openPianoRoll (te::MidiClip& c)
    {
        pianoRoll = std::make_unique<PianoRollWindow> (c, [this] { juce::MessageManager::callAsync ([this] { pianoRoll.reset(); lanes.repaint(); }); });
    }
    void exportMix (int bits = 24)
    {
        chooseFile (U ("WAV'ga eksport"), "*.wav", true, [this, bits] (juce::File f)
        {
            const bool ok = app.exportWav (f.withFileExtension ("wav"), bits);
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
    TopBar& getTopBar() { return top; }
    ViewState& getViewState() { return vs; }
    Mixer& getMixer() { return mixer; }
    MediaBrowser& getMedia() { return media; }
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
    TopBar top; GridBar gridBar; Inspector inspector;
    bool editorOpen = false; juce::TextButton lowMixB, lowEdB; TimeRuler ruler; TrackHeaders headers; Lanes lanes; Mixer mixer; MediaBrowser media; StatusBar status; AudioEditor editor;
    juce::TextButton toolAdd, toolSnap, toolBounce;
    juce::OwnedArray<juce::TextButton> tools;
    juce::Rectangle<int> toolArea;
    std::unique_ptr<PianoRollWindow> pianoRoll;
    std::unique_ptr<ConnectionsWindow> connections;
    std::unique_ptr<DopWindow> dopWindow;
    std::unique_ptr<RenderWindow> renderWindow;
    std::unique_ptr<MixerWindow> mixerWindow;
    std::unique_ptr<juce::FileChooser> chooser;
    bool showInsp = true, showMedia = true, showMixer = true, fitted = false;
    int recTick = 0, slow = 0; double lastPos = -1;
};
}
