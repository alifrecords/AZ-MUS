// =============================================================================
//  AZ MUS — dvigatel qatlami: loyiha, treklar, plaginlar, qurilmalar
//  (Tracktion Engine ustida — Personal litsenziya, "Powered by Tracktion Engine")
// =============================================================================
#pragma once
#include "Theme.h"

namespace az
{
// ---------------------------------------------------------------- Plagin oynasi
struct PluginEditorContent : public te::Plugin::EditorComponent
{
    explicit PluginEditorContent (te::ExternalPlugin& p) : plugin (p)
    {
        if (auto pi = plugin.getAudioPluginInstance())
        {
            editor.reset (pi->createEditorAndMakeActive());
            if (editor == nullptr) editor = std::make_unique<juce::GenericAudioProcessorEditor> (*pi);
            addAndMakeVisible (*editor);
        }
        setSize (juce::jmax (80, editor ? editor->getWidth() : 0), juce::jmax (40, editor ? editor->getHeight() : 0));
    }
    bool allowWindowResizing() override { return false; }
    juce::ComponentBoundsConstrainer* getBoundsConstrainer() override { return editor ? editor->getConstrainer() : nullptr; }
    void resized() override { if (editor) editor->setBounds (getLocalBounds()); }
    void childBoundsChanged (juce::Component* c) override
    {
        if (c == editor.get()) { plugin.edit.pluginChanged (plugin); setSize (editor->getWidth(), editor->getHeight()); }
    }
    te::ExternalPlugin& plugin;
    std::unique_ptr<juce::AudioProcessorEditor> editor;
};

class PluginWindow : public juce::DocumentWindow
{
public:
    explicit PluginWindow (te::Plugin& p)
        : DocumentWindow (p.getName() + "  \xc2\xb7  AZ MUS", bg1, DocumentWindow::closeButton, true), plugin (p)
    {
        setUsingNativeTitleBar (true);
        if (auto* ext = dynamic_cast<te::ExternalPlugin*> (&p))
            content = std::make_unique<PluginEditorContent> (*ext);
        else
            content = p.createEditor();
        if (content) setContentNonOwned (content.get(), true);
        setResizable (false, false);
        setBoundsConstrained (getLocalBounds() + plugin.windowState->choosePositionForPluginWindow());
        setVisible (true); toFront (true);
    }
    ~PluginWindow() override { plugin.edit.flushPluginStateIfNeeded (plugin); clearContentComponent(); content.reset(); }
    void closeButtonPressed() override { plugin.windowState->closeWindowExplicitly(); }
    void moved() override { plugin.windowState->lastWindowBounds = getBounds(); }
private:
    te::Plugin& plugin;
    std::unique_ptr<juce::Component> content;
};

inline bool headlessMode = false;

struct UIBehaviour : public te::UIBehaviour
{
    // Render/eksport: GUI'da progress oynasi bilan fon oqimida, testda to'g'ridan-to'g'ri
    void runTaskWithProgressBar (te::ThreadPoolJobWithProgress& t) override
    {
        if (! juce::MessageManager::getInstance()->isThisTheMessageThread())
        {
            while (t.runJob() == juce::ThreadPoolJob::jobNeedsRunningAgain) {}
            return;
        }
        if (headlessMode)   // test rejimi: render fon oqimida, xabar sikli aylanib turadi
        {
            std::atomic<bool> done { false };
            std::thread th ([&] { while (t.runJob() == juce::ThreadPoolJob::jobNeedsRunningAgain) {} done = true; });
            while (! done) juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
            th.join();
            return;
        }
        struct W : juce::ThreadWithProgressWindow
        {
            W (te::ThreadPoolJobWithProgress& j) : juce::ThreadWithProgressWindow (juce::String::fromUTF8 ("Eksport qilinmoqda\xe2\x80\xa6"), true, false), job (j) {}
            void run() override
            {
                while (! threadShouldExit() && job.runJob() == juce::ThreadPoolJob::jobNeedsRunningAgain)
                    setProgress (job.getCurrentTaskProgress());
            }
            te::ThreadPoolJobWithProgress& job;
        } w (t);
        w.runThread();
    }

    std::unique_ptr<juce::Component> createPluginWindow (te::PluginWindowState& pws) override
    {
        if (auto ws = dynamic_cast<te::Plugin::WindowState*> (&pws))
        {
            if (auto ext = dynamic_cast<te::ExternalPlugin*> (&ws->plugin))
                if (ext->getAudioPluginInstance() == nullptr) return {};
            return std::make_unique<PluginWindow> (ws->plugin);
        }
        return {};
    }
};

// ---------------------------------------------------------------- Ichki plaginlar ro'yxati
struct BuiltIn { const char* name; const char* xmlType; bool synth; };
inline const BuiltIn builtIns[] = {
    { "AZ EQ (4 polosali)",  "4bandEq",       false },
    { "AZ Kompressor",       "compressor",    false },
    { "AZ Reverb (oddiy)",   "reverb",        false },
    { "AZ Delay",            "delay",         false },
    { "AZ Chorus",           "chorus",        false },
    { "AZ Phaser",           "phaser",        false },
    { "AZ Filtr (LP/HP)",    "lowpass",       false },
    { "AZ Pitch Shift",      "pitchShifter",  false },
    { "AZ Synth (4OSC)",     "4osc",          true  },
    { "AZ Sampler",          "sampler",       true  },
};

inline juce::String pluginDisplayName (te::Plugin& p)
{
    const auto type = p.getPluginType();
    for (auto& b : builtIns) if (type == b.xmlType) return juce::String::fromUTF8 (b.name).upToFirstOccurrenceOf (" (", false, false);
    return p.getName();
}

inline void registerAzPlugins (te::Engine&);

// Eksport/bounce paytida Control Room (monitor sozlamalari) miksga ta'sir qilmasin
struct ScopedControlRoomBypass
{
    // Control Room va undan keyingi barcha plaginlar (Monitor Match, CR insertlar) — faqat monitor uchun
    explicit ScopedControlRoomBypass (te::Edit& e)
    {
        bool after = false;
        for (auto p : e.getMasterPluginList().getPlugins())
        {
            if (p->getPluginType() == "azcontrolroom") after = true;
            if (after) { plugins.add (p); was.add (p->isEnabled()); p->setEnabled (false); }
        }
    }
    ~ScopedControlRoomBypass() { for (int i = 0; i < plugins.size(); ++i) plugins[i]->setEnabled (was[i]); }
    juce::ReferenceCountedArray<te::Plugin> plugins; juce::Array<bool> was;
};

// =============================================================================
class AppEngine : public juce::ChangeBroadcaster, private juce::Timer
{
public:
    te::Engine engine { "AZ MUS", std::make_unique<UIBehaviour>(), nullptr };
    te::SelectionManager selection { engine };
    std::unique_ptr<te::Edit> edit;
    juce::File editFile;
    juce::Time lastSave;
    std::function<void()> onEditChanged;      // UI butunlay qayta quriladi

    AppEngine()
    {
        engine.getPropertyStorage().getAppCacheFolder().createDirectory();
        engine.getPluginManager().initialiseAirWindows();
        registerAzPlugins (engine);
        te::setDefaultPanLaw (te::PanLaw2point5dBCenter);   // Pro Tools kabi: -2.5 dB markaz
        startTimer (60 * 1000);   // avto-saqlash
    }

    ~AppEngine() override
    {
        stopTimer();
        if (edit) { saveIfPossible(); edit->getTransport().stop (false, true); }
        edit.reset();
    }

    static juce::File projectsFolder()
    {
        auto d = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("AZ MUS Projects");
        d.createDirectory(); return d;
    }

    // ---------------------------------------------------------------- loyiha
    void newProject (juce::File f = {})
    {
        if (f == juce::File()) f = projectsFolder().getNonexistentChildFile (U ("Yangi qo'shiq"), ".azmus", false);
        f.getParentDirectory().createDirectory();
        setEdit (te::createEmptyEdit (engine, f), f);
        for (auto* t : te::getAudioTracks (*edit)) edit->deleteTrack (t);
        if (auto mv = edit->getMasterVolumePlugin()) mv->setVolumeDb (0.0f);   // Pro Tools kabi: master 0 dB (unity)
        auto& ts = edit->tempoSequence;
        ts.getTempo (0)->setBpm (120.0);
        // standart: 2 audio + 1 instrument trek
        addAudioTrack (U ("Vokal"));
        addAudioTrack ("Audio 2");
        addInstrumentTrack ("Synth");
        edit->getUndoManager().clearUndoHistory();
        saveIfPossible();
    }

    bool openProject (const juce::File& f)
    {
        if (! f.existsAsFile()) return false;
        auto e = te::loadEditFromFile (engine, f);
        if (e == nullptr) return false;
        setEdit (std::move (e), f);
        return true;
    }

    void setEdit (std::unique_ptr<te::Edit> e, juce::File f)
    {
        selection.deselectAll();
        if (edit) { saveIfPossible(); edit->getTransport().stop (false, true); }
        edit = std::move (e);
        editFile = f;
        edit->playInStopEnabled = true;
        engine.getPropertyStorage().getPropertiesFile().setValue ("azLastEdit", f.getFullPathName());
        setupInputs();
        if (onEditChanged) onEditChanged();
        sendChangeMessage();
    }

    void saveIfPossible()
    {
        if (edit == nullptr || editFile == juce::File()) return;
        edit->editFileRetriever = [f = editFile] { return f; };
        te::EditFileOperations (*edit).save (true, true, false);
        lastSave = juce::Time::getCurrentTime();
        sendChangeMessage();
    }

    bool saveAs (const juce::File& f)
    {
        if (edit == nullptr) return false;
        editFile = f.withFileExtension ("azmus");
        edit->editFileRetriever = [ef = editFile] { return ef; };
        te::EditFileOperations (*edit).saveAs (editFile, true, nullptr);
        const bool ok = editFile.existsAsFile();
        lastSave = juce::Time::getCurrentTime();
        sendChangeMessage();
        return ok;
    }

    bool exportWav (const juce::File& out, int bits = 24)
    {
        if (edit == nullptr) return false;
        edit->getTransport().stop (false, true);
        const auto len = edit->getLength();
        if (len <= 0s) return false;
        auto& dm = engine.getDeviceManager();
        te::Renderer::Parameters r (*edit);
        r.destFile = out;
        r.audioFormat = engine.getAudioFileFormatManager().getWavFormat();
        r.bitDepth = bits == 32 ? 32 : 24;
        r.ditheringEnabled = bits != 32;        // 24-bit: TPDF dither (kvantlash shovqini o'rniga tekis shovqin)
        r.sampleRateForAudio = dm.getSampleRate() > 7000 ? dm.getSampleRate() : 48000.0;   // qurilma yo'q bo'lsa ham ishlaydi
        r.blockSizeForAudio  = dm.getBlockSize() > 0 ? dm.getBlockSize() : 512;
        r.time = { 0s, te::TimePosition() + len + 2s };                                     // +2 s reverb dumi
        r.usePlugins = true; r.useMasterPlugins = true;
        r.tracksToDo = te::toBitSet (te::getAllTracks (*edit));
        out.deleteFile();
        ScopedControlRoomBypass crb (*edit);
        te::Renderer::renderToFile ("AZ MUS eksport", r);
        return out.existsAsFile() && out.getSize() > 1000;
    }

    // ---------------------------------------------------------------- treklar
    te::AudioTrack* lastTrack() const
    {
        auto ts = te::getAudioTracks (*edit);
        return ts.isEmpty() ? nullptr : ts.getLast();
    }

    te::AudioTrack* addAudioTrack (const juce::String& name)
    {
        auto t = edit->insertNewAudioTrack (te::TrackInsertPoint (nullptr, lastTrack()), nullptr);
        if (t == nullptr) return nullptr;
        t->setName (name);
        t->setColour (trackColours[(te::getAudioTracks (*edit).size() - 1) % 8]);
        return t.get();
    }

    te::AudioTrack* addInstrumentTrack (const juce::String& name, const juce::PluginDescription* desc = nullptr)
    {
        auto* t = addAudioTrack (name);
        if (t == nullptr) return nullptr;
        te::Plugin::Ptr p = desc != nullptr ? edit->getPluginCache().createNewPlugin (te::ExternalPlugin::xmlTypeName, *desc)
                                            : edit->getPluginCache().createNewPlugin ("4osc", {});
        if (p) t->pluginList.insertPlugin (p, 0, nullptr);
        return t;
    }

    static bool isInstrumentTrack (te::AudioTrack& t)
    {
        for (auto p : t.pluginList.getPlugins()) if (p->isSynth()) return true;
        return false;
    }

    static bool isUserPlugin (te::Plugin* p)
    {
        return dynamic_cast<te::VolumeAndPanPlugin*> (p) == nullptr && dynamic_cast<te::LevelMeterPlugin*> (p) == nullptr
            && dynamic_cast<te::AuxSendPlugin*> (p) == nullptr && dynamic_cast<te::AuxReturnPlugin*> (p) == nullptr;
    }
    // insert joyi: volume (fader) plaginidan oldin = pre-fader
    static int userPluginCount (te::AudioTrack& t)
    {
        const int v = t.pluginList.getPlugins().indexOf (t.getVolumePlugin());
        return v >= 0 ? v : t.pluginList.size();
    }

    static juce::Array<te::Plugin*> userPlugins (te::AudioTrack& t)
    {
        juce::Array<te::Plugin*> r;
        for (auto p : t.pluginList.getPlugins()) if (isUserPlugin (p)) r.add (p);
        return r;
    }

    te::Plugin::Ptr insertPlugin (te::AudioTrack& t, const juce::String& xmlType, const juce::PluginDescription& d)
    {
        auto p = edit->getPluginCache().createNewPlugin (xmlType, d);
        if (p == nullptr) return {};
        edit->getUndoManager().beginNewTransaction ("Plagin");
        t.pluginList.insertPlugin (p, userPluginCount (t), nullptr);
        return p;
    }

    te::WaveAudioClip::Ptr importAudio (te::AudioTrack& t, const juce::File& f, te::TimePosition at)
    {
        te::AudioFile af (engine, f);
        if (! af.isValid()) return {};
        edit->getUndoManager().beginNewTransaction ("Import");
        return t.insertWaveClip (f.getFileNameWithoutExtension(), f, { { at, at + te::TimeDuration::fromSeconds (af.getLength()) }, {} }, false);
    }

    te::MidiClip::Ptr importMidi (te::AudioTrack& t, const juce::File& f, te::TimePosition at)
    {
        juce::FileInputStream in (f);
        juce::MidiFile mf;
        if (! in.openedOk() || ! mf.readFrom (in)) return {};
        mf.convertTimestampTicksToSeconds();
        juce::MidiMessageSequence seq;
        for (int i = 0; i < mf.getNumTracks(); ++i) seq.addSequence (*mf.getTrack (i), 0);
        seq.updateMatchedPairs();
        const double len = juce::jmax (1.0, seq.getEndTime());
        edit->getUndoManager().beginNewTransaction ("MIDI import");
        auto clip = t.insertMIDIClip (f.getFileNameWithoutExtension(), { at, at + te::TimeDuration::fromSeconds (len) }, nullptr);
        if (clip == nullptr) return {};
        auto& ts = edit->tempoSequence;
        const auto startBeat = ts.toBeats (at);
        for (auto* e : seq)
            if (e->message.isNoteOn() && e->noteOffObject != nullptr)
            {
                const auto b1 = ts.toBeats (at + te::TimeDuration::fromSeconds (e->message.getTimeStamp())) - startBeat;
                const auto b2 = ts.toBeats (at + te::TimeDuration::fromSeconds (e->noteOffObject->message.getTimeStamp())) - startBeat;
                clip->getSequence().addNote (e->message.getNoteNumber(), te::BeatPosition() + b1, juce::jmax (te::BeatDuration::fromBeats (0.05), b2 - b1),
                                             e->message.getVelocity(), 0, nullptr);
            }
        return clip;
    }

    // ---------------------------------------------------------------- Bounce Selection (B)
    // Audio kliplar: bitta audioga birlashtiriladi (klip ovozi/fade qo'llanadi, trek Fx trekda qoladi)
    // MIDI kliplar: instrument + Fx bilan audioga aylantiriladi va yangi trekka qo'yiladi (asl klip mute)
    juce::String bounceSelection (const juce::Array<te::Clip*>& clips)
    {
        if (edit == nullptr || clips.isEmpty()) return U ("Avval klip(lar)ni tanlang (Bounce Selection \xe2\x80\x94 B).");
        std::map<te::AudioTrack*, juce::Array<te::Clip*>> byTrack;
        for (auto* c : clips) if (auto* t = dynamic_cast<te::AudioTrack*> (c->getTrack())) byTrack[t].add (c);
        auto dir = editFile.getParentDirectory().getChildFile ("Audio"); dir.createDirectory();
        auto& dm = engine.getDeviceManager();
        int made = 0, nClips = 0;
        edit->getUndoManager().beginNewTransaction ("Bounce");
        for (auto& [t, cl] : byTrack)
        {
            auto a = cl[0]->getPosition().getStart(), b = cl[0]->getPosition().getEnd();
            bool midi = false;
            for (auto* c : cl) { a = juce::jmin (a, c->getPosition().getStart()); b = juce::jmax (b, c->getPosition().getEnd()); midi |= dynamic_cast<te::MidiClip*> (c) != nullptr; }
            if (midi) b = b + 1.5s;                                       // synth "release" dumi
            const auto name = (cl.size() == 1 ? cl[0]->getName() : t->getName()) + " (bounce)";
            const auto file = dir.getNonexistentChildFile (juce::File::createLegalFileName (name), ".wav");
            te::Renderer::Parameters r (*edit);
            r.destFile = file; r.audioFormat = engine.getAudioFileFormatManager().getWavFormat(); r.bitDepth = 32;   // bounce: 32-bit float, headroom yo'qolmaydi
            r.sampleRateForAudio = dm.getSampleRate() > 7000 ? dm.getSampleRate() : 48000.0;
            r.blockSizeForAudio  = dm.getBlockSize() > 0 ? dm.getBlockSize() : 512;
            r.time = { a, b };
            juce::BigInteger bits; bits.setBit (te::getAllTracks (*edit).indexOf (t)); r.tracksToDo = bits;
            r.allowedClips = cl;
            r.usePlugins = midi; r.useMasterPlugins = false;
            ScopedControlRoomBypass crb (*edit);
            te::Renderer::renderToFile ("Bounce", r);
            if (! file.existsAsFile()) continue;
            te::AudioFile af (engine, file);
            const auto len = te::TimeDuration::fromSeconds (af.getLength());
            if (midi)
            {
                auto nt = edit->insertNewAudioTrack (te::TrackInsertPoint (nullptr, t), nullptr);
                if (nt == nullptr) continue;
                nt->setName (name); nt->setColour (t->getColour());
                nt->insertWaveClip (name, file, { { a, a + len }, {} }, false);
                for (auto* c : cl) c->setMuted (true);
            }
            else
            {
                for (auto* c : cl) c->removeFromParent();
                t->insertWaveClip (name, file, { { a, a + len }, {} }, false);
            }
            ++made; nClips += cl.size();
        }
        if (made == 0) return U ("Bounce xatosi: render bajarilmadi");
        return U ("Bounce: ") + juce::String (made) + U (" trek \xe2\x80\x94 ") + juce::String (nClips) + U (" klip bitta audioga birlashtirildi. Ctrl+Z \xe2\x80\x94 qaytarish.");
    }

    // P: loop (locator) chegaralari = tanlangan kliplar
    juce::String locatorsToSelection (const juce::Array<te::Clip*>& clips)
    {
        if (edit == nullptr || clips.isEmpty()) return U ("Avval klipni tanlang \xe2\x80\x94 loop chegaralari shu klipga qo'yiladi.");
        auto a = clips[0]->getPosition().getStart(), b = clips[0]->getPosition().getEnd();
        for (auto* c : clips) { a = juce::jmin (a, c->getPosition().getStart()); b = juce::jmax (b, c->getPosition().getEnd()); }
        edit->getTransport().setLoopRange ({ a, b }); edit->getTransport().looping = true;
        auto& ts = edit->tempoSequence;
        auto bb = [&ts] (te::TimePosition p) { auto x = ts.toBarsAndBeats (p); return juce::String (x.bars + 1) + "." + juce::String (x.getWholeBeats() + 1); };
        return U ("Locatorlar tanlovga o'rnatildi: ") + bb (a) + U (" \xe2\x86\x92 ") + bb (b);
    }

    // ---------------------------------------------------------------- kirishlar (yozish)
    void setupInputs()
    {
        auto& dm = engine.getDeviceManager();
        for (int i = 0; i < dm.getNumWaveInDevices(); ++i)
            if (auto w = dm.getWaveInDevice (i)) { w->setMonitorMode (te::InputDevice::MonitorMode::automatic); w->setEnabled (true); }
        for (auto& m : dm.getMidiInDevices()) { m->setMonitorMode (te::InputDevice::MonitorMode::automatic); m->setEnabled (true); }
        edit->getTransport().ensureContextAllocated();
    }

    // Tanlangan instrument trek MIDI klaviaturadan chalinadi (Cubase kabi)
    void routeMidiTo (te::AudioTrack* t)
    {
        if (edit == nullptr || t == nullptr) return;
        for (auto* inst : edit->getAllInputDevices())
            if (inst->getInputDevice().getDeviceType() == te::InputDevice::physicalMidiDevice)
            {
                const bool armed = isArmed (*t);
                for (auto id : inst->getTargets()) if (id != t->itemID) (void) inst->removeTarget (id, nullptr);
                if (isInstrumentTrack (*t)) { (void) inst->setTarget (t->itemID, false, nullptr, 0); inst->setRecordingEnabled (t->itemID, armed); }
            }
        edit->restartPlayback();
    }

    bool isArmed (te::AudioTrack& t) const
    {
        for (auto* inst : edit->getAllInputDevices())
            if (te::isOnTargetTrack (*inst, t, 0) && inst->isRecordingEnabled (t.itemID)) return true;
        return false;
    }

    void setArmed (te::AudioTrack& t, bool arm)
    {
        const bool midi = isInstrumentTrack (t);
        bool assigned = false;
        for (auto* inst : edit->getAllInputDevices())
        {
            const auto type = inst->getInputDevice().getDeviceType();
            const bool matches = midi ? type == te::InputDevice::physicalMidiDevice : type == te::InputDevice::waveDevice;
            if (! matches) continue;
            if (te::isOnTargetTrack (*inst, t, 0)) { inst->setRecordingEnabled (t.itemID, arm); assigned = true; continue; }
            if (arm && (midi || ! assigned) && inst->getTargets().isEmpty())
            {
                if (inst->setTarget (t.itemID, false, nullptr, 0)) { inst->setRecordingEnabled (t.itemID, true); assigned = true; }
            }
        }
        edit->restartPlayback();
        sendChangeMessage();
    }

    // ---------------------------------------------------------------- yordamchi
    juce::String positionString() const
    {
        if (! edit) return "001.1.1.000";
        auto& ts = edit->tempoSequence;
        const auto pos = edit->getTransport().getPosition();
        const auto bb = ts.toBarsAndBeats (pos);
        const int bar = bb.bars + 1, beat = bb.getWholeBeats() + 1;
        const double frac = bb.getFractionalBeats().inBeats();
        const int six = (int) (frac * 4.0) + 1, tick = (int) ((frac * 4.0 - (six - 1)) * 240.0);
        return juce::String::formatted ("%03d.%d.%d.%03d", bar, beat, six, tick);
    }

    juce::String timeString() const
    {
        const double s = edit ? edit->getTransport().getPosition().inSeconds() : 0.0;
        return juce::String::formatted ("%02d:%02d.%03d", (int) s / 60, (int) s % 60, (int) ((s - std::floor (s)) * 1000));
    }

private:
    void timerCallback() override { if (edit && ! edit->getTransport().isRecording()) saveIfPossible(); }
};
}
