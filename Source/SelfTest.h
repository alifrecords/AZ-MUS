// =============================================================================
//  AZ MUS — avtomatik test (--selftest) va demo loyiha (--demo)
// =============================================================================
#pragma once
#include "AppEngine.h"
#include "Routing.h"
using az::U;

namespace testgen
{
// Sintetik "vokal": formantli impulslar + vibrato, iboralar bilan
inline juce::File makeVocal (const juce::File& f, double bpm, int bars)
{
    const int sr = 48000; const double beat = 60.0 / bpm; const int n = (int) (bars * 4 * beat * sr);
    juce::AudioBuffer<float> b (2, n); b.clear();
    const double notes[] = { 220, 246.9, 261.6, 293.7, 329.6, 293.7, 261.6, 246.9 };
    double ph = 0; float s1[3] {}, s2[3] {};
    for (int i = 0; i < n; ++i)
    {
        const double t = (double) i / sr, inBar = std::fmod (t, beat * 4);
        const bool sing = inBar < beat * 3.2;
        const double f0 = notes[(int) (t / (beat * 2)) % 8] * (1 + 0.01 * std::sin (2 * juce::MathConstants<double>::pi * 5.5 * t));
        ph += f0 / sr; float src = 0; if (ph >= 1) { ph -= 1; src = 1; }
        const double F[3] = { 700, 1150, 2900 };
        float v = 0;
        for (int k = 0; k < 3; ++k)
        {
            const double w = 2 * juce::MathConstants<double>::pi * F[k] / sr, rr = 0.97;
            const float y = (float) (src + 2 * rr * std::cos (w) * s1[k] - rr * rr * s2[k]); s2[k] = s1[k]; s1[k] = y;
            v += y * (k == 0 ? 1.0f : k == 1 ? 0.55f : 0.25f);
        }
        const float env = sing ? (float) juce::jmin (1.0, inBar / 0.04) * (float) juce::jmin (1.0, (beat * 3.2 - inBar) / 0.08) : 0.0f;
        b.setSample (0, i, v * 0.03f * env); b.setSample (1, i, v * 0.03f * env);
    }
    juce::WavAudioFormat wav; f.deleteFile();
    if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 24, {}, 0)))
        w->writeFromAudioSampleBuffer (b, 0, n);
    return f;
}

// Sintetik baraban: kick / snare / hat
inline juce::File makeDrums (const juce::File& f, double bpm, int bars)
{
    const int sr = 48000; const double step = 60.0 / bpm / 4; const int n = (int) (bars * 16 * step * sr);
    juce::AudioBuffer<float> b (2, n); b.clear(); juce::Random r (3);
    for (int s = 0; s < bars * 16; ++s)
    {
        const int at = (int) (s * step * sr), p = s % 16;
        const bool kick = p == 0 || p == 7 || p == 10, snare = p == 4 || p == 12;
        for (int i = 0; i < (int) (0.35 * sr) && at + i < n; ++i)
        {
            const double t = (double) i / sr; float v = 0;
            if (kick)  v += 0.8f * (float) (std::sin (2 * juce::MathConstants<double>::pi * (45 + 90 * std::exp (-t * 30)) * t) * std::exp (-t * 9));
            if (snare) v += 0.35f * (r.nextFloat() * 2 - 1) * (float) std::exp (-t * 22) + 0.2f * (float) (std::sin (2 * juce::MathConstants<double>::pi * 190 * t) * std::exp (-t * 25));
            if (p % 2 == 0) v += 0.08f * (r.nextFloat() * 2 - 1) * (float) std::exp (-t * 90);
            b.addSample (0, at + i, v); b.addSample (1, at + i, v);
        }
    }
    juce::WavAudioFormat wav; f.deleteFile();
    if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 24, {}, 0)))
        w->writeFromAudioSampleBuffer (b, 0, n);
    return f;
}

inline te::Plugin::Ptr findAndInsertVst3 (az::AppEngine& app, te::AudioTrack& t, const juce::String& nameContains)
{
    for (auto& d : app.engine.getPluginManager().knownPluginList.getTypes())
        if (d.name.containsIgnoreCase (nameContains))
            return app.insertPlugin (t, te::ExternalPlugin::xmlTypeName, d);
    return {};
}
}

// Demo loyiha (skrinshot va birinchi tanishuv uchun)
inline void makeDemo (az::AppEngine& app)
{
    using namespace testgen;
    auto& e = *app.edit;
    e.tempoSequence.getTempo (0)->setBpm (96);
    e.state.setProperty ("azKey", "A minor", nullptr);
    for (auto* t : te::getAudioTracks (e)) e.deleteTrack (t);
    auto dir = app.editFile.getParentDirectory().getChildFile ("Audio"); dir.createDirectory();
    const double bar = 4 * 60.0 / 96;
    auto* vox = app.addAudioTrack ("Lead Vocal");
    auto vf = makeVocal (dir.getChildFile ("Lead Vocal.wav"), 96, 8);
    app.importAudio (*vox, vf, te::TimePosition::fromSeconds (bar * 4));
    app.importAudio (*vox, vf, te::TimePosition::fromSeconds (bar * 14));
    if (! findAndInsertVst3 (app, *vox, "AZ VERB")) app.insertPlugin (*vox, "reverb", {});
    app.insertPlugin (*vox, "airwindows_pressure4", {});
    auto* bv = app.addAudioTrack ("Back Vocal");
    app.importAudio (*bv, vf, te::TimePosition::fromSeconds (bar * 8))->setGainDB (-6);
    bv->getVolumePlugin()->setVolumeDb (-4);
    auto* dr = app.addAudioTrack ("AZ Drum");
    auto df = makeDrums (dir.getChildFile ("Drums 96.wav"), 96, 4);
    for (int k = 0; k < 5; ++k) app.importAudio (*dr, df, te::TimePosition::fromSeconds (bar * 4 * k));
    auto* bass = app.addInstrumentTrack ("808 Bass");
    auto* pad = app.addInstrumentTrack ("Pad - Sand Air");
    auto& ts = e.tempoSequence;
    auto mk = [&] (te::AudioTrack* t, int fromBar, int bars) { return t->insertMIDIClip ("MIDI", { ts.toTime (te::tempo::BarsAndBeats { fromBar, {} }), ts.toTime (te::tempo::BarsAndBeats { fromBar + bars, {} }) }, nullptr); };
    if (auto c = mk (bass, 4, 16))
    {
        c->setName ("808 - Am");
        const int roots[] = { 45, 41, 48, 43 };
        for (int b = 0; b < 16; ++b) for (int s : { 0, 3, 6 })
            c->getSequence().addNote (roots[(b / 2) % 4] - 12, te::BeatPosition::fromBeats (b * 4 + s * 0.5), te::BeatDuration::fromBeats (s == 0 ? 1.4 : 0.9), 110, 0, nullptr);
    }
    if (auto c = mk (pad, 0, 20))
    {
        c->setName ("Pad Am - F - C - G");
        const int ch[4][3] = { { 57, 60, 64 }, { 53, 57, 60 }, { 55, 60, 64 }, { 55, 59, 62 } };
        for (int b = 0; b < 20; b += 2) for (int k = 0; k < 3; ++k)
            c->getSequence().addNote (ch[(b / 2) % 4][k], te::BeatPosition::fromBeats (b * 4), te::BeatDuration::fromBeats (7.8), 80, 0, nullptr);
    }
    pad->getVolumePlugin()->setVolumeDb (-9); bass->getVolumePlugin()->setVolumeDb (-5);
    auto& mm = e.getMarkerManager();
    const struct { int b, len; const char* n; juce::Colour c; } ms[] = { { 0, 4, "Intro", az::dim2 }, { 4, 8, "Kuplet 1", az::blue }, { 12, 8, "Naqarot", az::red }, { 20, 4, "Outro", az::violet } };
    for (auto& m : ms)
        if (auto mc = mm.createMarker (-1, ts.toTime (te::tempo::BarsAndBeats { m.b, {} }), ts.toTime (te::tempo::BarsAndBeats { m.b + m.len, {} }) - ts.toTime (te::tempo::BarsAndBeats { m.b, {} }), nullptr))
        { mc->setName (m.n); mc->setColour (m.c); }
    e.getTransport().setLoopRange ({ ts.toTime (te::tempo::BarsAndBeats { 12, {} }), ts.toTime (te::tempo::BarsAndBeats { 20, {} }) });
    e.getTransport().setPosition (ts.toTime (te::tempo::BarsAndBeats { 14, te::BeatDuration::fromBeats (1) }));
    app.saveIfPossible();
}

// ---------------------------------------------------------------- avtomatik test
inline int runSelfTest (const juce::StringArray& args)
{
    using namespace testgen;
    int fails = 0;
    auto check = [&fails] (bool ok, const juce::String& what) { std::printf ("  [%s] %s\n", ok ? "OK " : "XATO", what.toRawUTF8()); std::fflush (stdout); if (! ok) ++fails; };
    const auto dir = juce::File (args[args.indexOf ("--selftest") + 1].isNotEmpty() ? args[args.indexOf ("--selftest") + 1] : "/tmp/azmus_test");
    dir.deleteRecursively(); dir.createDirectory();
    std::printf ("AZ MUS selftest -> %s\n", dir.getFullPathName().toRawUTF8());

    auto app = std::make_unique<az::AppEngine>();
    check (true, "Dvigatel ishga tushdi");

    // tashqi VST3 (masalan AZ VERB) — ko'rsatilgan bo'lsa
    const int vi = args.indexOf ("--vst3");
    if (vi >= 0)
    {
        juce::OwnedArray<juce::PluginDescription> found;
        for (auto* fmt : app->engine.getPluginManager().pluginFormatManager.getFormats())
            if (fmt->getName() == "VST3")
            {
                std::printf ("       VST3: mightContain=%d exists=%d\n", (int) fmt->fileMightContainThisPluginType (args[vi + 1]), (int) juce::File (args[vi + 1]).exists());
                app->engine.getPluginManager().knownPluginList.scanAndAddFile (args[vi + 1], false, found, *fmt);
                juce::OwnedArray<juce::PluginDescription> direct; fmt->findAllTypesForFile (direct, args[vi + 1]);
                std::printf ("       VST3 direct: %d\n", direct.size()); std::fflush (stdout);
            }
        check (found.size() > 0, "VST3 skan: " + (found.isEmpty() ? juce::String ("topilmadi") : found[0]->name + " (" + found[0]->manufacturerName + ")"));
    }

    const auto f = dir.getChildFile (U ("Test qo'shiq.azmus"));
    app->newProject (f);
    check (te::getAudioTracks (*app->edit).size() == 3, "Yangi loyiha: 3 ta standart trek (" + juce::String (te::getAudioTracks (*app->edit).size()) + ")");
    { juce::String fm; for (auto* f2 : app->engine.getPluginManager().pluginFormatManager.getFormats()) fm << f2->getName() << " "; std::printf ("       formatlar: %s\n", fm.toRawUTF8()); }
    makeDemo (*app);
    auto tracks = te::getAudioTracks (*app->edit);
    check (tracks.size() == 5, "Demo: 5 ta trek (" + juce::String (tracks.size()) + ")");
    int clips = 0, notes = 0, plugs = 0, vst3 = 0;
    for (auto* t : tracks)
    {
        clips += t->getClips().size();
        for (auto* c : t->getClips()) if (auto* mc = dynamic_cast<te::MidiClip*> (c)) notes += mc->getSequence().getNumNotes();
        for (auto* p : az::AppEngine::userPlugins (*t)) { ++plugs; if (dynamic_cast<te::ExternalPlugin*> (p)) ++vst3; }
    }
    check (clips >= 9, "Kliplar: " + juce::String (clips));
    check (notes > 50, "MIDI notalar: " + juce::String (notes));
    check (plugs >= 4, "Plaginlar (synth + effektlar): " + juce::String (plugs) + ", shundan VST3: " + juce::String (vst3));
    check (app->edit->getMarkerManager().getMarkers().size() == 4, "Markerlar (Intro / Kuplet / Naqarot / Outro)");

    // undo / redo
    const int before = te::getAudioTracks (*app->edit).size();
    app->edit->getUndoManager().beginNewTransaction();
    app->addAudioTrack ("Undo test");
    app->edit->getUndoManager().beginNewTransaction();
    app->edit->getUndoManager().undo();
    check (te::getAudioTracks (*app->edit).size() == before, "Undo trek qo'shishni bekor qiladi");

    // saqlash -> qayta ochish
    app->saveIfPossible();
    check (f.existsAsFile() && f.getSize() > 1000, "Loyiha saqlandi (" + juce::String (f.getSize()) + " bayt)");
    std::printf ("       qayta ochish...\n"); std::fflush (stdout);
    auto app2 = std::make_unique<az::AppEngine>();
    if (vi >= 0) { juce::OwnedArray<juce::PluginDescription> fd; for (auto* fmt : app2->engine.getPluginManager().pluginFormatManager.getFormats()) if (fmt->getName() == "VST3") app2->engine.getPluginManager().knownPluginList.scanAndAddFile (args[vi + 1], true, fd, *fmt); }
    check (app2->openProject (f), "Loyiha qayta ochildi");
    int clips2 = 0, plugs2 = 0;
    for (auto* t : te::getAudioTracks (*app2->edit)) { clips2 += t->getClips().size(); plugs2 += az::AppEngine::userPlugins (*t).size(); }
    check (clips2 == clips && plugs2 == plugs, "Qayta ochilgan loyiha bir xil (kliplar " + juce::String (clips2) + ", plaginlar " + juce::String (plugs2) + ")");
    check (std::abs (app2->edit->tempoSequence.getTempo (0)->getBpm() - 96.0) < 0.01, "Temp saqlangan: 96 BPM");

    // Bounce (B): AZ Drum trekidagi 2 ta klip -> 1 ta audio
    {
        auto ts2 = te::getAudioTracks (*app2->edit);
        te::AudioTrack* drum = nullptr; te::AudioTrack* pad = nullptr;
        for (auto* t : ts2) { if (t->getName() == "AZ Drum") drum = t; if (t->getName().startsWith ("Pad")) pad = t; }
        if (drum != nullptr && drum->getClips().size() >= 2)
        {
            const int before2 = drum->getClips().size();
            juce::Array<te::Clip*> sel { drum->getClips()[0], drum->getClips()[1] };
            const auto expectEnd = sel[1]->getPosition().getEnd();
            const auto msg = app2->bounceSelection (sel);
            std::printf ("       %s\n", msg.toRawUTF8());
            bool lenOk = false;
            for (auto* c : drum->getClips()) if (c->getName().contains ("bounce")) lenOk = std::abs ((c->getPosition().getEnd() - expectEnd).inSeconds()) < 0.05;
            check (drum->getClips().size() == before2 - 1 && lenOk, "Bounce (audio): 2 klip -> 1 audio, uzunlik to'g'ri");
            app2->edit->getUndoManager().undo();
            check (drum->getClips().size() == before2, "Bounce Ctrl+Z bilan qaytarildi");
        }
        else check (false, "Bounce: AZ Drum trek topilmadi");
        if (pad != nullptr && pad->getClips().size() > 0)
        {
            const int nTracks = te::getAudioTracks (*app2->edit).size();
            juce::Array<te::Clip*> sel { pad->getClips()[0] };
            app2->bounceSelection (sel);
            const auto ts3 = te::getAudioTracks (*app2->edit);
            bool newTrackOk = ts3.size() == nTracks + 1 && ts3[ts3.indexOf (pad) + 1]->getClips().size() == 1;
            check (newTrackOk && pad->getClips()[0]->isMuted(), "Bounce (MIDI): synth audioga aylandi, yangi trek, asl klip mute");
            app2->edit->getUndoManager().undo();
        }
        // P: locatorlar
        if (drum != nullptr)
        {
            juce::Array<te::Clip*> sel { drum->getClips()[1] };
            app2->locatorsToSelection (sel);
            const auto lr = app2->edit->getTransport().getLoopRange();
            check (lr.getStart() == sel[0]->getPosition().getStart() && lr.getEnd() == sel[0]->getPosition().getEnd() && app2->edit->getTransport().looping,
                   "Locatorlar (P) = tanlangan klip");
        }
        // tillar
        const int keep = az::lang;
        az::lang = 1; const bool ru = az::tr ("Saqlash") == juce::String::fromUTF8 ("Сохранить");
        az::lang = 2; const bool en = az::tr ("Saqlash") == "Save";
        az::lang = keep;
        check (ru && en, "Tarjima: Saqlash -> Сохранить / Save");
    }

    // FX kanal + send, Control Room (eksportga ta'sir qilmasligi)
    {
        az::Routing rt (*app2);
        auto* fx = rt.addFxChannel ("Reverb", "reverb", {});
        te::AudioTrack* vox = nullptr;
        for (auto* t : te::getAudioTracks (*app2->edit)) if (t->getName() == "Lead Vocal") vox = t;
        auto* snd = (fx && vox) ? rt.addSend (*vox, az::Routing::fxBus (*fx), -6.0f) : nullptr;
        check (fx != nullptr && snd != nullptr && rt.fxTracks().size() == 1 && std::abs (snd->getGainDb() + 6.0f) < 0.1f, "FX kanal (Reverb) + Lead Vocal'dan send -6 dB");
        if (auto* cr = rt.controlRoom()) { cr->mute = true; cr->level = -30.0f; }
        check (rt.controlRoom() != nullptr, "Control Room master oxirida");
    }
    // Delete tugmasi (Windows'da Num Del bilan bir xil kod) haqiqatan o'chiradi
    {
        auto mc = std::make_unique<az::MainComponent> (*app2);
        mc->setBounds (0, 0, 1600, 900);
        te::AudioTrack* drum = nullptr;
        for (auto* t : te::getAudioTracks (*app2->edit)) if (t->getName() == "AZ Drum") drum = t;
        if (drum != nullptr)
        {
            const int n0 = drum->getClips().size();
            app2->selection.selectOnly (drum->getClips().getLast());
            mc->keyPressed (juce::KeyPress (juce::KeyPress::deleteKey));
            check (drum->getClips().size() == n0 - 1, "Delete tugmasi tanlangan klipni o'chiradi");
            app2->edit->getUndoManager().undo();
            check (drum->getClips().size() == n0, "Ctrl+Z o'chirilgan klipni qaytaradi");

            // sichqoncha hodisalari (haqiqiy MouseEvent)
            auto& ln = mc->getLanes();
            auto src = juce::Desktop::getInstance().getMainMouseSource();
            auto ev = [&] (juce::Point<float> p, juce::Point<float> down, juce::ModifierKeys m, bool dragged)
            { return juce::MouseEvent (src, p, m, 0, 0, 0, 0, 0, &ln, &ln, juce::Time::getCurrentTime(), down, juce::Time::getCurrentTime(), 1, dragged); };
            const auto lm = juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier);
            // to'rtburchak: birinchi 3 ta baraban klipini qamrab olish
            auto r0 = ln.clipRect (*drum->getClips()[0]), r2 = ln.clipRect (*drum->getClips()[2]);
            const juce::Point<float> a (r0.getX() + 2, r0.getY() - 2), b (r2.getCentreX(), r2.getBottom() - 4);
            ln.mouseDown (ev (a, a, lm, false)); ln.mouseDrag (ev (b, a, lm, true)); ln.mouseUp (ev (b, a, juce::ModifierKeys(), true));
            check (app2->selection.getItemsOfType<te::Clip>().size() == 3, "Sichqoncha bilan to'rtburchak: 3 ta klip belgilandi (" + juce::String (app2->selection.getItemsOfType<te::Clip>().size()) + ")");
            // Alt + sudrash = nusxa
            app2->selection.deselectAll();
            const int before = drum->getClips().size();
            auto* last = drum->getClips().getLast();
            const auto origStart = last->getPosition().getStart();
            auto rc = ln.clipRect (*last);
            const juce::Point<float> c1 (rc.getCentreX(), rc.getCentreY()), c2 (rc.getCentreX() + rc.getWidth() + 2, rc.getCentreY());
            const auto alt = juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::altModifier);
            ln.mouseDown (ev (c1, c1, alt, false)); ln.mouseDrag (ev (c2, c1, alt, true)); ln.mouseUp (ev (c2, c1, juce::ModifierKeys(), true));
            bool origStays = false; for (auto* cc : drum->getClips()) if (cc->getPosition().getStart() == origStart) origStays = true;
            check (drum->getClips().size() == before + 1 && origStays, "Alt + sudrash: nusxa yaratildi, asl klip joyida qoldi");
            // Alt + bosish (sudramasdan) = qaychi (Cubase)
            app2->selection.deselectAll();
            auto* c0 = drum->getClips()[0]; const int nSplit = drum->getClips().size();
            auto rr = ln.clipRect (*c0); const juce::Point<float> pm (rr.getX() + rr.getWidth() * 0.5f, rr.getCentreY());
            ln.mouseDown (ev (pm, pm, alt, false)); ln.mouseUp (ev (pm, pm, juce::ModifierKeys(), false));
            check (drum->getClips().size() == nSplit + 1, "Alt + bosish: qaychi \xe2\x80\x94 klip shu joyda bo'lindi (Cubase)");
        }
        mc.reset();
    }

    // Master insert (mastering): limiter master zanjirida, Control Room'dan oldin, saqlanadi
    {
        az::PickItem it; it.kind = 0; it.index = 1;   // AZ Kompressor
        auto p = az::insertMaster (*app2, it);
        auto ins = az::masterInserts (*app2->edit);
        auto& ml = app2->edit->getMasterPluginList();
        const bool beforeCR = ml.getPlugins().getLast()->getPluginType() == "azcontrolroom";
        check (p != nullptr && ins.size() == 1 && beforeCR, "Master insert: AZ Kompressor master zanjirida (Control Room'dan oldin)");
        app2->saveIfPossible();
        auto app3 = std::make_unique<az::AppEngine>(); app3->openProject (f);
        check (az::masterInserts (*app3->edit).size() == 1, "Master insert loyiha bilan saqlanadi");
        app3.reset();
    }

    // render (mixdown)
    const auto out = dir.getChildFile ("mixdown.wav");
    std::printf ("       render...\n"); std::fflush (stdout);
    const bool rendered = app2->exportWav (out);
    check (rendered && out.existsAsFile(), "WAV eksport (mixdown)");
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    if (auto r = std::unique_ptr<juce::AudioFormatReader> (fm.createReaderFor (out)))
    {
        juce::AudioBuffer<float> b ((int) r->numChannels, (int) r->lengthInSamples);
        r->read (&b, 0, (int) r->lengthInSamples, 0, true, true);
        const float rms = b.getRMSLevel (0, 0, b.getNumSamples()), peak = b.getMagnitude (0, b.getNumSamples());
        const double secs = (double) r->lengthInSamples / r->sampleRate;
        std::printf ("       mixdown: %.1f s, %d Hz, RMS %.1f dBFS, cho'qqi %.1f dBFS\n", secs, (int) r->sampleRate, juce::Decibels::gainToDecibels (rms), juce::Decibels::gainToDecibels (peak));
        check (secs > 30 && rms > 0.005f && peak <= 1.5f, "Mixdown tovushli va me'yorida");
        // L/R farqi (stereo reverb ishlaganmi)
        float diff = 0; for (int i = 0; i < b.getNumSamples(); ++i) diff += std::abs (b.getSample (0, i) - b.getSample (1, i));
        check (diff / (float) b.getNumSamples() > 1.0e-4f, "Stereo maydon bor (reverb ishlagan)");
    }
    else check (false, "Mixdown faylini o'qish");

    // Pro Tools uslubi: master 0 dB, pan law -2.5 dB
    {
        auto a4 = std::make_unique<az::AppEngine>(); a4->newProject (dir.getChildFile ("pt.azmus"));
        check (std::abs (a4->edit->getMasterVolumePlugin()->getVolumeDb()) < 0.05f && te::getDefaultPanLaw() == te::PanLaw2point5dBCenter,
               "Pro Tools uslubi: master 0 dB (unity), pan law -2.5 dB");
    }

    // Control Room · Monitor Match: monitorda ishlaydi, eksportga TUSHMAYDI
    {
        auto rmsOf = [&fm] (const juce::File& file)
        {
            std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (file));
            if (! r) return -200.0f;
            juce::AudioBuffer<float> b ((int) r->numChannels, (int) r->lengthInSamples);
            r->read (&b, 0, (int) r->lengthInSamples, 0, true, true);
            return juce::Decibels::gainToDecibels (b.getRMSLevel (0, 0, b.getNumSamples()), -200.0f);
        };
        const auto outB = dir.getChildFile ("mixdown_b.wav");
        app2->exportWav (outB);
        const float base = rmsOf (outB);
        std::printf ("       takroriy eksport farqi: %.3f dB\n", base - rmsOf (out));
        az::Routing rt (*app2);
        auto* mmp = rt.monitorMatch (true);
        if (mmp) { mmp->source = 0; mmp->target = 7; }   // KRK Rokit 6 → Smartfon (eng keskin farq)
        auto inCR = az::Routing::controlRoomInserts (*app2->edit);
        check (mmp != nullptr && inCR.size() == 1 && az::masterInserts (*app2->edit).size() == 1,
               "Monitor Match Control Room zanjirida (Master'dan keyin, alohida)");
        const auto out2 = dir.getChildFile ("mixdown_cr.wav");
        app2->exportWav (out2);
        const float withCR = rmsOf (out2);
        std::printf ("       eksport RMS: %.2f dB (Monitor Match yoqiq: %.2f dB)\n", base, withCR);
        check (std::abs (withCR - base) < 0.15f, "Monitor Match eksport/mixdown'ga ta'sir qilmaydi (faqat monitor)");

        // DSP haqiqatan ishlaydimi: vaqtincha master insert sifatida render qilamiz
        auto p = app2->edit->getPluginCache().createNewPlugin (az::MonitorMatchPlugin::xmlTypeName, {});
        if (auto* tm = dynamic_cast<az::MonitorMatchPlugin*> (p.get())) { tm->source = 0; tm->target = 7; }
        app2->edit->getMasterPluginList().insertPlugin (p, 0, nullptr);
        const auto out3 = dir.getChildFile ("mixdown_mm.wav");
        app2->exportWav (out3);
        const float dsp = rmsOf (out3);
        std::printf ("       Monitor Match DSP (KRK 6 → Smartfon) render: %.2f dB\n", dsp);
        check (std::abs (dsp - base) > 1.0f && dsp > -60.0f, "Monitor Match DSP ovozni o'zgartiradi (KRK 6 → Smartfon)");
        p->deleteFromParent();
        app2->saveIfPossible();
        auto a5 = std::make_unique<az::AppEngine>(); a5->openProject (f);
        auto* m5 = az::Routing (*a5).monitorMatch (false);
        check (m5 != nullptr && m5->target.get() == 7, "Monitor Match sozlamalari loyiha bilan saqlanadi");
        // 32-bit float eksport
        const auto out4 = dir.getChildFile ("mixdown32.wav");
        a5->exportWav (out4, 32);
        std::unique_ptr<juce::AudioFormatReader> r4 (fm.createReaderFor (out4));
        check (r4 && r4->bitsPerSample == 32 && r4->usesFloatingPointData, "WAV 32-bit float eksport");
    }

    // ================= 0.5: Grid · Quantize · DOP (Envelope AUTO) · Render Selection
    {
        auto a6 = std::make_unique<az::AppEngine>(); a6->newProject (dir.getChildFile ("t05.azmus"));
        auto& e6 = *a6->edit; auto& ts6 = e6.tempoSequence; ts6.getTempo (0)->setBpm (120.0);
        az::ViewState v6; v6.pxPerSec = 100;
        // Grid: 1/8 triplet (1/3 beat = 0.1667 s @120)
        v6.gridMode = az::ViewState::gridDivision; v6.division = 0.5 * 2.0 / 3.0;
        const double s1 = az::snapTime (e6, 0.30, v6).inSeconds();
        v6.division = 0.75;   // 1/8 dotted = 0.375 s
        const double s2 = az::snapTime (e6, 0.40, v6).inSeconds();
        check (std::abs (s1 - 1.0 / 3.0) < 1e-6 && std::abs (s2 - 0.375) < 1e-6 && az::ViewState::divisionName (0.5 * 2.0 / 3.0) == "1/8T" && az::ViewState::divisionName (0.75) == "1/8.",
               "Grid: 1/8 Triplet va 1/8 Dotted to'g'ri snap qilinadi");
        v6.division = 0.5; v6.snapType = az::ViewState::snapGridRelative;
        const double rel = az::snapMove (e6, te::TimePosition::fromSeconds (0.10), 0.27, v6).inSeconds();   // +0.27 s -> +0.25 s
        check (std::abs (rel - 0.35) < 1e-6, "Snap: Grid Relative (klipning gridga nisbatan joyi saqlanadi)");
        v6.timeFormat = az::ViewState::fmtTimecode; v6.fps = 25;
        const auto tc = az::formatTime (e6, te::TimePosition::fromSeconds (61.5), v6);
        v6.timeFormat = az::ViewState::fmtSamples;
        const auto smp = az::formatTime (e6, te::TimePosition::fromSeconds (1.0), v6, 48000.0);
        check (tc == "00:01:01:12" && smp == "48000", "Vaqt formati: Timecode va Samples");

        // DOP uchun test audio: baland va past iboralar (-12 / -30 dBFS)
        const auto wf = dir.getChildFile ("uneven_vocal.wav");
        {
            const int sr = 48000, n = sr * 8; juce::AudioBuffer<float> b (2, n);
            for (int i = 0; i < n; ++i)
            {
                const double t = (double) i / sr; const int ph = (int) (t / 1.0);
                const float amp = ph % 4 == 3 ? 0.0f : (ph % 2 == 0 ? 0.35f : 0.045f);
                const float v = amp * (float) std::sin (2 * juce::MathConstants<double>::pi * 220 * t) * (float) juce::jmin (1.0, std::fmod (t, 1.0) / 0.02);
                b.setSample (0, i, v); b.setSample (1, i, v);
            }
            juce::WavAudioFormat wav; wf.deleteFile();
            if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (new juce::FileOutputStream (wf), sr, 2, 24, {}, 0))) w->writeFromAudioSampleBuffer (b, 0, n);
        }
        auto* vt = te::getAudioTracks (e6)[0];
        auto clip = vt->insertWaveClip ("Vokal take", wf, { te::TimeRange (te::TimePosition(), te::TimePosition::fromSeconds (8.0)), {} }, false);
        auto* wc = dynamic_cast<te::WaveAudioClip*> (clip.get());
        auto spread = [&] (const juce::File& file)
        {
            juce::AudioBuffer<float> b; double sr = 48000; az::dop::readFile (a6->engine, file, b, sr);
            std::vector<float> lv;
            for (int ph = 0; ph < 8; ++ph) { if (ph % 4 == 3) continue; const int a0 = (int) ((ph + 0.3) * sr); lv.push_back (juce::Decibels::gainToDecibels (b.getRMSLevel (0, a0, (int) (0.4 * sr)))); }
            return *std::max_element (lv.begin(), lv.end()) - *std::min_element (lv.begin(), lv.end());
        };
        const float before = spread (wf);
        auto st = az::dop::stack (*wc, true);
        auto envP = az::dop::makeProc ("envelope"); envP.setProperty ("auto", true, nullptr); envP.setProperty ("target", 0, nullptr); envP.setProperty ("range", 18.0, nullptr);
        st.addChild (envP, -1, nullptr);
        az::dop::refreshAuto (*wc, st, a6->engine, nullptr);
        const bool rendered = az::dop::render (*wc, a6->engine, a6->editFile.getParentDirectory().getChildFile ("Audio"));
        const auto newFile = wc->getSourceFileReference().getFile();
        const float after = spread (newFile);
        std::printf ("       DOP Envelope AUTO: iboralar farqi %.1f dB -> %.1f dB (%d nuqta)\n", before, after, (int) az::dop::points (envP).size());
        check (rendered && newFile != wf && newFile.getFileName().contains ("_DOP_") && after < before * 0.35f,
               "Direct Offline Processing: Envelope AUTO past/baland iboralarni tenglashtirdi");
        // qo'lda nuqta qo'shish
        auto pts = az::dop::points (envP); pts.push_back ({ 0.5, -6.0 });
        std::sort (pts.begin(), pts.end(), [] (auto& x, auto& y) { return x.x < y.x; });
        az::dop::setPoints (envP, pts, nullptr); envP.setProperty ("auto", false, nullptr);
        az::dop::render (*wc, a6->engine, a6->editFile.getParentDirectory().getChildFile ("Audio"));
        check (wc->getSourceFileReference().getFile() != newFile, "DOP: qo'lda tahrir qilingan envelope qayta qo'llanadi");
        // Gain + Normalize steki va asliga qaytarish
        st.addChild (az::dop::makeProc ("normalize"), -1, nullptr);
        az::dop::render (*wc, a6->engine, a6->editFile.getParentDirectory().getChildFile ("Audio"));
        { juce::AudioBuffer<float> b; double sr; az::dop::readFile (a6->engine, wc->getSourceFileReference().getFile(), b, sr);
          check (std::abs (juce::Decibels::gainToDecibels (b.getMagnitude (0, b.getNumSamples())) + 1.0f) < 0.05f, "DOP: Normalize -1 dBFS"); }
        st.removeAllChildren (nullptr);
        az::dop::render (*wc, a6->engine, a6->editFile.getParentDirectory().getChildFile ("Audio"));
        check (wc->getSourceFileReference().getFile() == wf, "DOP: barcha jarayonlar olib tashlansa asl faylga qaytadi");

        // Render Selection: Channel Settings -> yangi trek, asl klip mute
        az::RenderOptions ro; ro.processing = 1; ro.bitDepth = 32; ro.source = 0;
        const int tracksBefore = te::getAudioTracks (e6).size();
        const auto msg = az::renderSelection (*a6, juce::Array<te::Clip*> { clip.get() }, ro);
        auto tracksNow = te::getAudioTracks (e6);
        bool rendTrack = false; for (auto* t : tracksNow) if (t->getName().endsWith ("(Render)") && t->getClips().size() == 1) rendTrack = true;
        check (tracksNow.size() == tracksBefore + 1 && rendTrack && clip->isMuted(), "Render Selection (Channel Settings): yangi trek, asl event mute");
        // Dry + Remove: o'rniga qo'yiladi
        clip->setMuted (false);
        az::RenderOptions ro2; ro2.processing = 0; ro2.source = 2; ro2.customName = true; ro2.name = "Azimxodjayev Music - Take";
        az::renderSelection (*a6, juce::Array<te::Clip*> { clip.get() }, ro2);
        bool replaced = false; for (auto* c : vt->getClips()) if (c->getName().startsWith ("Azimxodjayev Music - Take")) replaced = true;
        check (replaced && vt->getClips().size() == 1, "Render Selection (Dry, Custom Name, Remove Source): event almashtirildi");

        // Quantize: MIDI notalar 1/16 ga
        auto mc6 = std::make_unique<az::MainComponent> (*a6);
        auto* it = a6->addInstrumentTrack ("Q test");
        auto midiClip = it->insertMIDIClip (te::TimeRange (te::TimePosition(), te::TimePosition::fromSeconds (4.0)), nullptr);
        midiClip->getSequence().addNote (60, te::BeatPosition::fromBeats (0.13), te::BeatDuration::fromBeats (0.5), 100, 0, nullptr);
        midiClip->getSequence().addNote (62, te::BeatPosition::fromBeats (1.40), te::BeatDuration::fromBeats (0.5), 100, 0, nullptr);
        auto& gv = mc6->getViewState(); gv.linkQ = false; gv.quantize = 0.25;
        mc6->quantizeClips (juce::Array<te::Clip*> { midiClip.get() });
        auto& ns = midiClip->getSequence().getNotes();
        check (ns.size() == 2 && std::abs (ns[0]->getStartBeat().inBeats() - 0.25) < 1e-6 && std::abs (ns[1]->getStartBeat().inBeats() - 1.5) < 1e-6,
               "Quantize: MIDI notalar 1/16 gridga tushdi");
        // Tap tempo / temp qutisi
        az::TempoBox tb (*a6); tb.setBpm (96.0004);
        check (std::abs (ts6.getTempo (0)->getBpm() - 96.0) < 1e-3, "Temp qutisi: 96.000 BPM o'rnatildi");
        mc6.reset();
    }

    // ================= 0.6: AZ BEAT · sample sync
    {
        check (az::sync::bpmFromName ("Trap_Loop_140bpm_F#m") == 140.0 && az::sync::bpmFromName ("Piano 92 BPM") == 92.0 && az::sync::bpmFromName ("kick_01") == 0.0,
               "Sync: BPM fayl nomidan o'qiladi");
        auto k1 = az::sync::keyFromName ("Trap_Loop_140bpm_F#m"), k2 = az::sync::keyFromName ("Guitar_Cmaj_90"), k3 = az::sync::keyFromName ("Pad Bb minor");
        check (k1.root == 6 && k1.minor && k2.root == 0 && ! k2.minor && k3.root == 10 && k3.minor, "Sync: tonallik fayl nomidan o'qiladi (F#m, Cmaj, Bb minor)");
        check (az::sync::semitoneShift ({ 9, true }, { 0, false }) == 0 && az::sync::semitoneShift ({ 9, true }, { 11, true }) == 2
               && az::sync::semitoneShift ({ 0, false }, { 7, false }) == -5, "Sync: yarim ton siljishi (nisbiy minor/major hisobga olinadi)");
        // audiodan tonallik: A minor akkord (A-C-E) va bas A
        {
            const double sr = 44100; juce::AudioBuffer<float> b (1, (int) sr * 6);
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                const double t = i / sr; double v = 0;
                for (double f : { 110.0, 220.0, 261.63, 329.63, 440.0, 523.25 }) v += std::sin (juce::MathConstants<double>::twoPi * f * t);
                if (std::fmod (t, 2.0) > 1.0) v += 0.6 * std::sin (juce::MathConstants<double>::twoPi * 293.66 * t) + 0.6 * std::sin (juce::MathConstants<double>::twoPi * 349.23 * t);  // Dm
                b.setSample (0, i, (float) (v * 0.08));
            }
            auto k = az::sync::detectKey (b, sr);
            std::printf ("       audiodan tonallik: %s\n", az::sync::keyName (k).toRawUTF8());
            check (k.valid() && ((k.root == 9 && k.minor) || (k.root == 0 && ! k.minor)), "Sync: audiodan tonallik aniqlanadi (Am / nisbiy C)");
            auto st = az::sync::stretch (b, sr, 1.25, 0);
            std::printf ("       time-stretch: %.2f s -> %.2f s\n", b.getNumSamples() / sr, st.getNumSamples() / sr);
            check (std::abs (st.getNumSamples() / sr - 7.5) < 0.1 && st.getMagnitude (0, st.getNumSamples()) > 0.1f, "Sync: time-stretch (SoundTouch) 120 \xe2\x86\x92 96 BPM uzunligi to'g'ri");
        }

        auto a7 = std::make_unique<az::AppEngine>(); a7->newProject (dir.getChildFile ("beat.azmus"));
        a7->edit->tempoSequence.getTempo (0)->setBpm (100.0);
        for (auto* t : te::getAudioTracks (*a7->edit)) a7->edit->deleteTrack (t);
        auto* bt = az::addBeatTrack (*a7);
        auto* bp = bt ? az::beatPluginOf (*bt) : nullptr;
        check (bt != nullptr && bt->getName() == "AZ BEAT" && bp != nullptr && juce::File (bp->pad (0)["file"].toString()).existsAsFile() && juce::File (bp->pad (15)["file"].toString()).existsAsFile(),
               "AZ BEAT treki: 16 padli standart kit yuklandi");
        int hits = 0; for (int p = 0; p < 16; ++p) for (int st = 0; st < 16; ++st) hits += bp && bp->stepVel (p, st) > 0 ? 1 : 0;
        check (hits >= 10 && bp->stepRatchet (3, 15) == 3, "AZ BEAT: boshlang'ich pattern (kick/clap/hat + ratchet x3)");
        a7->edit->getTransport().setLoopRange ({ te::TimePosition(), te::TimePosition::fromSeconds (4.8) });
        // 2 takt bo'sh MIDI klip — eksport uzunligi uchun
        bt->insertMIDIClip (te::TimeRange (te::TimePosition(), te::TimePosition::fromSeconds (4.8)), nullptr);
        const auto bo = dir.getChildFile ("beat.wav");
        a7->exportWav (bo, 32);
        float rmsDb = -200, peakDb = -200; int onsets = 0;
        {
            juce::AudioBuffer<float> b; double sr = 48000; az::dop::readFile (a7->engine, bo, b, sr);
            if (b.getNumSamples() > 0)
            {
                rmsDb = juce::Decibels::gainToDecibels (b.getRMSLevel (0, 0, b.getNumSamples()), -200.0f);
                peakDb = juce::Decibels::gainToDecibels (b.getMagnitude (0, b.getNumSamples()), -200.0f);
                onsets = az::beat::detectOnsets (b, sr, 64).size();
            }
        }
        std::printf ("       AZ BEAT render: RMS %.1f dBFS, cho'qqi %.1f dBFS, %d zarba\n", rmsDb, peakDb, onsets);
        check (rmsDb > -40 && peakDb < 6 && onsets >= 8, "AZ BEAT sekvenseri chaladi (eksportda zarbalar bor)");

        // Slice: 4 zarbali loop -> 4 pad + pattern
        const auto lf = dir.getChildFile ("break_100bpm.wav");
        {
            const int sr = 48000, n = (int) (sr * 2.4); juce::AudioBuffer<float> b (2, n); b.clear(); juce::Random r (1);
            for (int hit : { 0, 3, 6, 10 })
            {
                const int at = (int) (hit * 0.15 * sr);
                for (int i = 0; i < sr / 8 && at + i < n; ++i) { const float v = (r.nextFloat() * 2 - 1) * 0.6f * std::exp (-i / (sr * 0.03f)); b.setSample (0, at + i, v); b.setSample (1, at + i, v); }
            }
            juce::WavAudioFormat wav; lf.deleteFile();
            if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (new juce::FileOutputStream (lf), sr, 2, 24, {}, 0))) w->writeFromAudioSampleBuffer (b, 0, n);
        }
        const auto sm = bp->sliceLoop (lf, 100.0);
        std::printf ("       Slice: %s\n", sm.toRawUTF8());
        check (bp->stepVel (0, 0) > 0 && bp->stepVel (1, 3) > 0 && bp->stepVel (2, 6) > 0 && bp->stepVel (3, 10) > 0 && bp->pad (4)["file"].toString().isEmpty(),
               "AZ BEAT Slice: loop 4 bo'lakka bo'lindi, pattern 1/16 ga yozildi (Nerve)");
        // Pattern -> MIDI
        {
            az::BeatEditor ed (*bp);
            const int clipsBefore = bt->getClips().size();
            a7->edit->getTransport().setPosition (te::TimePosition());
            ed.patternToMidi();
            auto* mc = bt->getClips().size() > clipsBefore ? dynamic_cast<te::MidiClip*> (bt->getClips().getLast()) : nullptr;
            check (mc != nullptr && mc->getSequence().getNumNotes() == 4 && ! (bool) bp->state["seqOn"], "AZ BEAT: Pattern \xe2\x86\x92 MIDI klip (4 nota, ichki SEQ o'chdi)");
        }
        // saqlash / qayta ochish
        a7->saveIfPossible();
        auto a8 = std::make_unique<az::AppEngine>(); a8->openProject (a7->editFile);
        az::AzBeatPlugin* bp8 = nullptr; for (auto* t : te::getAudioTracks (*a8->edit)) if (auto* x = az::beatPluginOf (*t)) bp8 = x;
        check (bp8 != nullptr && bp8->stepVel (3, 10) > 0 && bp8->pad (0)["file"].toString() == lf.getFullPathName(), "AZ BEAT holati loyiha bilan saqlanadi");
    }

    // ================= 0.7: Alt-bypass · chap chetdan qirqish · zarba magniti · trek tartibi · spektrogramma · Tap
    {
        auto a9 = std::make_unique<az::AppEngine>(); a9->newProject (dir.getChildFile ("t07.azmus"));
        auto& e9 = *a9->edit; e9.tempoSequence.getTempo (0)->setBpm (120.0);
        for (auto* t : te::getAudioTracks (e9)) e9.deleteTrack (t);
        auto* tA = a9->addAudioTrack ("A"); auto* tB = a9->addAudioTrack ("B"); auto* tC = a9->addAudioTrack ("C");
        // zarbali audio: 0.50, 1.25, 2.00 s da (1 kHz)
        const auto hf = dir.getChildFile ("hits.wav");
        {
            const int sr = 48000, n = sr * 3; juce::AudioBuffer<float> b (2, n); b.clear();
            for (double h : { 0.50, 1.25, 2.00 })
                for (int i = 0; i < sr / 4; ++i) { const int at = (int) (h * sr) + i; const float v = 0.6f * std::sin ((float) (juce::MathConstants<double>::twoPi * 1000.0 * i / sr)) * std::exp (-i / (sr * 0.06f)); b.setSample (0, at, v); b.setSample (1, at, v); }
            juce::WavAudioFormat wav; hf.deleteFile();
            if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (new juce::FileOutputStream (hf), sr, 2, 24, {}, 0))) w->writeFromAudioSampleBuffer (b, 0, n);
        }
        auto on = az::detectTransients (a9->engine, hf);
        std::printf ("       zarbalar:"); for (double o : on) std::printf (" %.4f", o); std::printf ("\n");
        check (on.size() == 3 && std::abs (on[0] - 0.5) < 0.004 && std::abs (on[1] - 1.25) < 0.004 && std::abs (on[2] - 2.0) < 0.004, "Zarba (transient) boshlanishlari aniq topildi (\xc2\xb1" "4 ms)");
        auto clip = tA->insertWaveClip ("hits", hf, { te::TimeRange (te::TimePosition::fromSeconds (1.0), te::TimePosition::fromSeconds (4.0)), {} }, false);

        auto mc9 = std::make_unique<az::MainComponent> (*a9); mc9->setSize (1600, 900);
        auto& ln = mc9->getLanes(); auto& gv = mc9->getViewState(); gv.pxPerSec = 100; gv.startSec = 0; gv.scrollY = 0; gv.transientMagnet = true;
        ln.onsetCache[hf.getFullPathName()] = on;
        // magnit: 1.25+1.0 = 2.25 s ga yaqin (2.30 s) bosilsa — aynan 2.25 s
        auto [cut, mag] = ln.cutPosition (*clip, gv.secToX (2.30));
        check (mag && std::abs (cut.inSeconds() - (1.0 + on[1])) < 1e-6, "Qaychi magniti: kesish zarba boshiga yopishadi");
        gv.transientMagnet = false;
        auto [cut2, mag2] = ln.cutPosition (*clip, gv.secToX (2.30));
        check (! mag2 && std::abs (cut2.inSeconds() - 2.25) < 1e-6, "Magnit o'chirilsa: gridga (1/8 = 0.25 s) kesiladi");
        gv.transientMagnet = true;

        // chap chetdan qirqish
        auto evm = [&] (juce::Point<float> p, juce::Point<float> down, juce::ModifierKeys m, bool dragged)
        { return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), p, m, 1.0f, 0, 0, 0, 0, &ln, &ln, juce::Time::getCurrentTime(), down, juce::Time::getCurrentTime(), 1, dragged); };
        {
            const auto r = ln.clipRect (*clip);
            const juce::Point<float> d (r.getX() + 2, r.getCentreY()), m (r.getX() + 2 + 50, r.getCentreY());   // +0.5 s
            ln.mouseDown (evm (d, d, juce::ModifierKeys(), false)); ln.mouseDrag (evm (m, d, juce::ModifierKeys(), true)); ln.mouseUp (evm (m, d, juce::ModifierKeys(), true));
            const auto ps = clip->getPosition();
            std::printf ("       chap chet: start %.3f, oxir %.3f, offset %.3f\n", ps.getStart().inSeconds(), ps.getEnd().inSeconds(), ps.getOffset().inSeconds());
            check (std::abs (ps.getStart().inSeconds() - 1.5) < 1e-3 && std::abs (ps.getEnd().inSeconds() - 4.0) < 1e-3 && std::abs (ps.getOffset().inSeconds() - 0.5) < 1e-3,
                   "Klipni chap chetidan sudrab qirqish (kontent joyida, oxiri qimirlamaydi)");
            // ortga: asl boshidan oldinga cho'zib bo'lmaydi
            const auto r2 = ln.clipRect (*clip);
            const juce::Point<float> d2 (r2.getX() + 2, r2.getCentreY()), m2 (r2.getX() + 2 - 300, r2.getCentreY());
            ln.mouseDown (evm (d2, d2, juce::ModifierKeys(), false)); ln.mouseDrag (evm (m2, d2, juce::ModifierKeys(), true)); ln.mouseUp (evm (m2, d2, juce::ModifierKeys(), true));
            check (std::abs (clip->getPosition().getStart().inSeconds() - 1.0) < 1e-3 && std::abs (clip->getPosition().getOffset().inSeconds()) < 1e-3, "Chap chet: audioning boshidan oldinga o'tmaydi");
        }
        // trek tartibi
        a9->moveTrackTo (*tA, 3);
        auto order = te::getAudioTracks (e9);
        check (order.size() == 3 && order[0] == tB && order[1] == tC && order[2] == tA, "Treklar tartibi almashtirildi (A oxiriga)");
        a9->moveTrackTo (*tA, 0);
        order = te::getAudioTracks (e9);
        check (order[0] == tA && order[1] == tB && order[2] == tC, "Trek eng tepaga qaytarildi");

        // Alt + bosish insert'da = bypass (mikser)
        {
            auto pl = e9.getPluginCache().createNewPlugin ("compressor", {});
            tA->pluginList.insertPlugin (pl, az::AppEngine::userPluginCount (*tA), nullptr);
            az::ChannelStrip st (*a9, tA); st.setSize (92, 260);
            const auto sl = st.insSlot (0).getCentre();
            juce::ModifierKeys alt (juce::ModifierKeys::altModifier | juce::ModifierKeys::leftButtonModifier);
            st.mouseDown (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), sl, alt, 1.0f, 0, 0, 0, 0, &st, &st, juce::Time::getCurrentTime(), sl, juce::Time::getCurrentTime(), 1, false));
            const bool off = ! pl->isEnabled();
            st.mouseDown (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), sl, alt, 1.0f, 0, 0, 0, 0, &st, &st, juce::Time::getCurrentTime(), sl, juce::Time::getCurrentTime(), 1, false));
            check (off && pl->isEnabled(), "Alt + bosish: insert plaginni o'chirish / yoqish (bypass)");
        }
        // spektrogramma: 1 kHz signal -> cho'qqi ~1 kHz
        {
            const auto sf = dir.getChildFile ("sine1k.wav");
            { const int sr = 48000, n = sr * 2; juce::AudioBuffer<float> b (2, n);
              for (int i = 0; i < n; ++i) { const float v = 0.5f * std::sin ((float) (juce::MathConstants<double>::twoPi * 1000.0 * i / sr)); b.setSample (0, i, v); b.setSample (1, i, v); }
              juce::WavAudioFormat wav; sf.deleteFile();
              if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (new juce::FileOutputStream (sf), sr, 2, 24, {}, 0))) w->writeFromAudioSampleBuffer (b, 0, n); }
            tC->insertWaveClip ("sine", sf, { te::TimeRange (te::TimePosition(), te::TimePosition::fromSeconds (2.0)), {} }, false);
            auto* sc = az::ensureScope (*tC);
            check (sc != nullptr && az::AppEngine::userPlugins (*tC).size() == 0, "Spektrogramma tap plagini qo'shildi (insertlar ro'yxatida ko'rinmaydi)");
            az::SpectroPanel panel (*tC); panel.setSize (330, 240);
            a9->exportWav (dir.getChildFile ("scope.wav"), 32);
            for (int k = 0; k < 6; ++k) panel.timerCallback();
            const auto& row = panel.rowAgo (0);
            int bi = 0; for (int i = 1; i < az::SpectroPanel::bins; ++i) if (row[(size_t) i] > row[(size_t) bi]) bi = i;
            std::printf ("       spektr cho'qqisi: %.0f Hz (%.1f dB)\n", az::SpectroPanel::binHz (bi), row[(size_t) bi]);
            check (std::abs (std::log2 (az::SpectroPanel::binHz (bi) / 1000.0)) < 0.15 && row[(size_t) bi] > -40, "Spektrogramma real-time: 1 kHz signal to'g'ri ko'rsatildi");
            az::removeScope (*tC);
            check (az::findScope (*tC) == nullptr, "Spektrogramma yopilganda tap plagini olib tashlandi");
        }
        // Tap tempo: to'xtagan holatda darhol, 0.5 s oraliq -> 120 BPM
        {
            az::TempoBox tb (*a9); e9.tempoSequence.getTempo (0)->setBpm (90.0);
            tb.tap(); juce::Thread::sleep (500); tb.tap(); juce::Thread::sleep (500); tb.tap();
            check (std::abs (e9.tempoSequence.getTempo (0)->getBpm() - 120.0) < 2.0 && tb.tapMode, "Tap tempo (Probel bilan ham): 3 bosish -> ~120 BPM");
        }
        mc9.reset();
    }

    // ================= 0.8: AZ BEAT normalize/delay/browse/moslik · plagin oynasi transporti · AUTO LVL
    {
        auto a10 = std::make_unique<az::AppEngine>(); a10->newProject (dir.getChildFile ("t08.azmus"));
        auto& e10 = *a10->edit; e10.tempoSequence.getTempo (0)->setBpm (120.0); e10.state.setProperty ("azKey", "A minor", nullptr);
        for (auto* t : te::getAudioTracks (e10)) e10.deleteTrack (t);
        auto sdir = dir.getChildFile ("smp"); sdir.deleteRecursively(); sdir.createDirectory();
        auto writeTone = [&] (const juce::File& f, double hz, float amp, double dur, bool decay)
        {
            const int sr = 48000, n = (int) (sr * dur); juce::AudioBuffer<float> b (2, n);
            for (int i = 0; i < n; ++i) { const float v = amp * std::sin ((float) (juce::MathConstants<double>::twoPi * hz * i / sr)) * (decay ? std::exp (-i / (sr * 0.15f)) : 1.0f); b.setSample (0, i, v); b.setSample (1, i, v); }
            juce::WavAudioFormat wav;
            if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 24, {}, 0))) w->writeFromAudioSampleBuffer (b, 0, n);
        };
        const auto quiet = sdir.getChildFile ("01 quiet kick.wav"), s808g = sdir.getChildFile ("02 808 G.wav"), s808fs = sdir.getChildFile ("03 808 Fsharp.wav");
        writeTone (quiet, 60, 0.05f, 0.4, true); writeTone (s808g, 98.0, 0.7f, 0.8, true); writeTone (s808fs, 92.5, 0.7f, 0.8, true);
        writeTone (sdir.getChildFile ("Pad_Loop_120bpm_Am.wav"), 220, 0.3f, 8.0, false);

        auto* bt = az::addBeatTrack (*a10); auto* bp = az::beatPluginOf (*bt);
        for (int p = 0; p < 16; ++p) { bp->pad (p).setProperty ("steps", juce::String::repeatedString ("0,", 64), nullptr); bp->clearPad (p); }
        bp->loadSample (0, quiet); bp->setStep (0, 4, 127);          // 4-step = 0.5 s
        bt->insertMIDIClip (te::TimeRange (te::TimePosition(), te::TimePosition::fromSeconds (2.0)), nullptr);
        auto renderInfo = [&] (const char* name, float& rms, double& onset)
        {
            const auto f = dir.getChildFile (name); a10->exportWav (f, 32);
            juce::AudioBuffer<float> b; double sr = 48000; az::dop::readFile (a10->engine, f, b, sr);
            rms = juce::Decibels::gainToDecibels (b.getMagnitude (0, b.getNumSamples()), -200.0f);
            onset = -1; for (int i = 0; i < b.getNumSamples(); ++i) if (std::abs (b.getSample (0, i)) > 0.002f) { onset = i / sr; break; }
        };
        float pk0, pk1, pk2; double on0, on1, on2;
        renderInfo ("b_plain.wav", pk0, on0);
        bp->pad (0).setProperty ("norm", true, nullptr); bp->rebuildForTest();
        renderInfo ("b_norm.wav", pk1, on1);
        std::printf ("       normalize: cho'qqi %.1f -> %.1f dBFS\n", pk0, pk1);
        check (pk0 < -20 && std::abs (pk1 + 1.0f) < 1.5f, "AZ BEAT Normalize: past signal -1 dBFS ga ko'tarildi");
        bp->pad (0).setProperty ("delay", -50.0, nullptr); bp->rebuildForTest();
        renderInfo ("b_early.wav", pk2, on2);
        double onLate; float pk3; bp->pad (0).setProperty ("delay", 100.0, nullptr); bp->rebuildForTest();
        renderInfo ("b_late.wav", pk3, onLate);
        std::printf ("       delay: 0 ms -> %.3f s, -50 ms -> %.3f s, +100 ms -> %.3f s\n", on1, on2, onLate);
        check (std::abs (on1 - 0.5) < 0.003 && std::abs (on2 - 0.45) < 0.003 && std::abs (onLate - 0.6) < 0.003, "AZ BEAT Delay: oldinga (-50 ms) va orqaga (+100 ms) siljitish");

        // moslik
        const auto key = az::sync::keyFromString ("A minor");
        auto mg = az::sync::matchToProject (a10->engine, s808g, 120, key), mf = az::sync::matchToProject (a10->engine, s808fs, 120, key);
        auto ml = az::sync::matchToProject (a10->engine, sdir.getChildFile ("Pad_Loop_120bpm_Am.wav"), 120, key);
        std::printf ("       moslik: %s | %s | %s\n", mg.text.toRawUTF8(), mf.text.toRawUTF8(), ml.text.toRawUTF8());
        check (mg.score == 2 && mf.score == 0 && ml.score == 2, "Sample moslik: 808 G (A minor'da) mos, F# mos emas, 120bpm Am loop mos");
        // papkada oldinga/orqaga
        {
            az::BeatEditor ed (*bp); bp->loadSample (1, s808g);
            ed.selectPadForTest (1); ed.browse (+1, false);
            const bool nxt = juce::File (bp->pad (1)["file"].toString()) == s808fs;
            ed.browse (-1, false); ed.browse (-1, false);
            const bool prv = juce::File (bp->pad (1)["file"].toString()) == quiet;
            ed.browse (+1, true);    // keyingi MOS: quiet(?) -> 808 G
            check (nxt && prv && juce::File (bp->pad (1)["file"].toString()) == s808g, "AZ BEAT: papkadagi sample'larni oldinga/orqaga (Shift: faqat moslari)");
        }
        // plagin oynasi paneli: Play ishlaydi
        {
            int got = -1; az::pluginWindowTransport = [&] (int c) { got = c; };
            juce::Component dummy; dummy.setSize (300, 200); juce::DocumentWindow dw ("t", juce::Colours::black, 0, false);
            az::PluginFrame fr (dummy, dw);
            fr.playB.triggerClick(); juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
            check (got == 0 && fr.pinB.getToggleState() && fr.getHeight() == 200 + az::PluginFrame::barH, "Plagin oynasida Play/Stop paneli va 'Qotirish' (yoqilgan)");
            az::pluginWindowTransport = nullptr;
        }
        // AUTO LVL
        {
            auto mc10 = std::make_unique<az::MainComponent> (*a10);
            auto& mb = mc10->getMedia();
            juce::AudioBuffer<float> b; double sr; az::dop::readFile (a10->engine, sdir.getChildFile ("Pad_Loop_120bpm_Am.wav"), b, sr);
            mb.sampleRmsDb = az::MediaBrowser::activeRmsDb (b); mb.masterPeakDb = -200;
            const float silentMix = mb.computeAutoDb();
            mb.masterPeakDb = -6.0f;
            const float loudMix = mb.computeAutoDb();
            std::printf ("       AUTO LVL: sample %.1f dB RMS -> jim mix: %+.1f dB, -6 dB mix: %+.1f dB\n", mb.sampleRmsDb, silentMix, loudMix);
            check (std::abs (mb.sampleRmsDb + 13.5f) < 0.6f && std::abs (silentMix + 6.5f) < 0.7f && std::abs (loudMix - (-18.0f + 13.5f)) < 0.7f,
                   "AUTO LVL: sample balandligi mix balandligiga moslanadi");
            auto* at = a10->addAudioTrack ("Loop");
            auto c = a10->importAudio (*at, sdir.getChildFile ("Pad_Loop_120bpm_Am.wav"), te::TimePosition());
            mb.masterPeakDb = -200;
            mb.applySync (*c, sdir.getChildFile ("Pad_Loop_120bpm_Am.wav"));
            check (std::abs (c->getGainDB() - silentMix) < 0.7f, "AUTO LVL: trekka qo'yilgan klip gain'i ham moslandi");
            mc10.reset();
        }
    }

    // ================= 0.9: AZ Pitch (VariAudio) · AUTO tuzatish · PSOLA · xor · vokal tozalash · AudioWarp
    {
        const double sr = 48000;
        // sintetik vokal: A3, C4 (-35 cent), E4 (+40 cent), [nafas], D4 (vibrato +-30 cent)
        struct N { double t0, t1, midi, vib; };
        const N seq[] = { { 0.10, 0.60, 57.0, 0 }, { 0.65, 1.15, 59.65, 0 }, { 1.20, 1.70, 64.40, 0 }, { 2.10, 2.80, 62.0, 0.30 } };
        const int len = (int) (sr * 3.0);
        juce::AudioBuffer<float> voc (1, len); voc.clear();
        juce::Random rnd (5); double ph = 0;
        for (int i = 0; i < len; ++i)
        {
            const double t = i / sr; double m = -1, env = 0;
            for (auto& n : seq) if (t >= n.t0 && t < n.t1) { m = n.midi + n.vib * std::sin (2 * juce::MathConstants<double>::pi * 5.5 * (t - n.t0)); env = juce::jmin (1.0, juce::jmin ((t - n.t0) / 0.02, (n.t1 - t) / 0.02)); }
            float v = 0;
            if (m > 0)
            {
                ph += 440.0 * std::pow (2.0, (m - 69) / 12.0) / sr;
                for (int k = 1; k <= 18; ++k) v += (float) (std::sin (2 * juce::MathConstants<double>::pi * k * ph) / k * (k == 3 || k == 4 ? 1.8 : 1.0));
                v *= (float) (0.12 * env);
            }
            if (t > 1.78 && t < 2.02) v += (rnd.nextFloat() * 2 - 1) * 0.02f;     // nafas
            voc.setSample (0, i, v);
        }
        auto an = az::pitch::analyse (voc, sr);
        auto notes = az::pitch::segment (an);
        std::printf ("       AZ Pitch notalar:"); for (auto& n : notes) std::printf (" [%.2f-%.2f %.2f]", n.start, n.end, n.orig); std::printf ("\n");
        bool ok = notes.size() == 4;
        const double exp[] = { 57.0, 59.65, 64.40, 62.0 };
        for (size_t i = 0; ok && i < 4; ++i) ok = std::abs (notes[i].orig - exp[i]) < 0.12;
        check (ok, "AZ Pitch: vokal 4 notaga to'g'ri bo'lindi, ton aniqligi +-12 cent");
        auto fixedNotes = notes;
        const auto am = az::sync::keyFromString ("A minor");
        auto ar = az::pitch::autoCorrect (fixedNotes, am, 1.0f, 0.35f);
        std::printf ("       AUTO: %d nota, og'ish %.0f -> %.0f cent\n", ar.fixedNotes, ar.avgCentsBefore, ar.avgCentsAfter);
        check (fixedNotes.size() == 4 && std::abs (fixedNotes[1].pitch - 60) < 0.01f && std::abs (fixedNotes[2].pitch - 64) < 0.01f && ar.avgCentsAfter < 1,
               "AUTO tuzatish: fals notalar (C4-35c, E4+40c) tonallikka aniq joylandi");
        // PSOLA render va qayta tahlil
        auto outB = az::pitch::renderNotes (voc, an, fixedNotes);
        auto an2 = az::pitch::analyse (outB, sr);
        auto n2 = az::pitch::segment (an2);
        std::printf ("       PSOLA'dan keyin:"); for (auto& n : n2) std::printf (" %.2f", n.orig); std::printf ("\n");
        bool ok2 = n2.size() == 4 && outB.getNumSamples() == voc.getNumSamples();
        const double tgt[] = { 57, 60, 64, 62 };
        for (size_t i = 0; ok2 && i < 4; ++i) ok2 = std::abs (n2[i].orig - tgt[i]) < 0.12;
        check (ok2, "PSOLA: tuzatilgan vokal haqiqatan tonallikda (qayta tahlil +-12 cent), uzunlik o'zgarmagan");
        // garmoniya: tertsiya yuqoriga (A minor): A->C, C->E, E->G, D->F
        check (az::pitch::diatonicShift (57, 2, am) == 3 && az::pitch::diatonicShift (60, 2, am) == 4 && az::pitch::diatonicShift (64, 2, am) == 3
               && az::pitch::diatonicShift (62, 4, am) == 7 && az::pitch::diatonicShift (60, -2, am) == -3, "Garmoniya: diatonik tertsiya/kvinta tonallikka mos");
        {
            az::pitch::Voice v { "t", 2, 0, 0, 0, 0 };
            auto hb = az::pitch::renderVoice (voc, an, fixedNotes, v, am, 1);
            auto nh = az::pitch::segment (az::pitch::analyse (hb, sr));
            std::printf ("       tertsiya ovozi:"); for (auto& n : nh) std::printf (" %.2f", n.orig); std::printf ("\n");
            check (nh.size() >= 4 && std::abs (nh[0].orig - 60) < 0.15 && std::abs (nh[1].orig - 64) < 0.15 && std::abs (nh[2].orig - 67) < 0.15, "Xor: tertsiya ovozi to'g'ri notalarda (C4, E4, G4 ...)");
        }
        // vokal tozalash: nafas pasayadi, ovoz o'zgarmaydi
        {
            auto cb = voc; az::pitch::CleanSettings cs; az::pitch::clean (cb, sr, an, cs);
            auto rmsDb = [&] (const juce::AudioBuffer<float>& b, double a, double z) { return juce::Decibels::gainToDecibels (b.getRMSLevel (0, (int) (a * sr), (int) ((z - a) * sr)), -200.0f); };
            const float br0 = rmsDb (voc, 1.85, 1.98), br1 = rmsDb (cb, 1.85, 1.98), vo0 = rmsDb (voc, 0.2, 0.5), vo1 = rmsDb (cb, 0.2, 0.5);
            std::printf ("       tozalash: nafas %.1f -> %.1f dB, ovoz %.1f -> %.1f dB\n", br0, br1, vo0, vo1);
            check (br1 < br0 - 8 && std::abs (vo1 - vo0) < 1.5f, "Vokal tozalash: nafas >8 dB pasaydi, ovoz saqlandi");
        }
        // Editor integratsiyasi
        auto a11 = std::make_unique<az::AppEngine>(); a11->newProject (dir.getChildFile ("t09.azmus"));
        auto& e11 = *a11->edit; e11.tempoSequence.getTempo (0)->setBpm (120.0); e11.state.setProperty ("azKey", "A minor", nullptr);
        for (auto* t : te::getAudioTracks (e11)) e11.deleteTrack (t);
        const auto vf = dir.getChildFile ("synth_vocal.wav");
        { juce::WavAudioFormat wav; vf.deleteFile(); if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (new juce::FileOutputStream (vf), sr, 1, 24, {}, 0))) w->writeFromAudioSampleBuffer (voc, 0, voc.getNumSamples()); }
        auto* vt = a11->addAudioTrack ("Vokal");
        auto vc = vt->insertWaveClip ("Vokal", vf, { te::TimeRange (te::TimePosition(), te::TimePosition::fromSeconds (3.0)), {} }, false);
        auto mc11 = std::make_unique<az::MainComponent> (*a11); mc11->setSize (1600, 900);
        mc11->openAudioEditor (*vc);
        auto& ed = mc11->getAudioEditor();
        check (ed.getNotes().size() == 4, "Audio Editor: klip 2x bosilganda ochildi va AZ Pitch tahlil qildi");
        ed.runAuto(); ed.renderNow();
        const auto nf = vc->getSourceFileReference().getFile();
        check (nf != vf && nf.getFileName().contains ("_DOP_") && az::dop::originalFile (*vc) == vf, "AZ Pitch: AUTO natija klipga qo'llandi (asl fayl saqlanadi)");
        // saqlash / qayta ochish
        a11->saveIfPossible();
        {
            auto a12 = std::make_unique<az::AppEngine>(); a12->openProject (a11->editFile);
            auto* c12 = dynamic_cast<te::WaveAudioClip*> (te::getAudioTracks (*a12->edit)[0]->getClips()[0]);
            juce::ValueTree pp; if (c12) for (auto p : az::dop::stack (*c12, false)) if (p["type"].toString() == "pitch") pp = p;
            auto ln = az::pitch::loadNotes (pp);
            check (ln.size() == 4 && std::abs (ln[1].pitch - 60) < 0.01f, "AZ Pitch tahrirlari loyiha bilan saqlanadi");
        }
        const int tBefore = te::getAudioTracks (e11).size();
        ed.makeHarmony (6);
        auto ts11 = te::getAudioTracks (e11);
        check (ts11.size() == tBefore + 3 && ts11[1]->getName().contains ("Xor") && ts11[1]->getClips().size() == 1, "XOR 3: bir bosishda 3 ta garmoniya treki yaratildi");
        // vokal tozalash (DOP stekida)
        ed.setTab (3); ed.applyClean (true);
        bool hasClean = false; for (auto p : az::dop::stack (*vc, false)) hasClean |= p["type"].toString() == "vclean";
        check (hasClean && vc->getSourceFileReference().getFile() != nf, "Vokal tozalash bir bosishda qo'llandi");
        // AudioWarp: hitpoint quantize + swing
        {
            auto* dt = a11->addAudioTrack ("Drum");
            auto dc = dt->insertWaveClip ("hits", dir.getChildFile ("hits.wav"), { te::TimeRange (te::TimePosition::fromSeconds (0.03), te::TimePosition::fromSeconds (3.03)), {} }, false);
            mc11->openAudioEditor (*dc); ed.setTab (0); ed.swingSlider().setValue (0);
            ed.quantizeHitpoints();
            bool onGrid = dt->getClips().size() >= 3;
            for (auto* c : dt->getClips()) { const double b = e11.tempoSequence.toBeats (c->getPosition().getStart()).inBeats(); onGrid &= std::abs (b * 4 - std::round (b * 4)) < 1e-4; }
            std::printf ("       hitpoint quantize: %d bo'lak\n", dt->getClips().size());
            check (onGrid, "AudioWarp: hitpoint'lar bo'yicha bo'lindi va 1/16 gridga quantize qilindi");
        }
        mc11.reset();
    }

    // ================= 0.9.1: kesilgan (temp/pitch moslangan) klip to'lqini va ovozi yo'qolmaydi
    {
        auto a13 = std::make_unique<az::AppEngine>(); a13->newProject (dir.getChildFile ("t091.azmus"));
        auto& e = *a13->edit; e.tempoSequence.getTempo (0)->setBpm (120.0);
        for (auto* t : te::getAudioTracks (e)) e.deleteTrack (t);
        const auto lf = dir.getChildFile ("Beat_100bpm.wav");
        { const int sr = 48000, n = sr * 4; juce::AudioBuffer<float> b (2, n); juce::Random r (3);
          for (int i = 0; i < n; ++i) { const float v = 0.4f * std::sin ((float) (juce::MathConstants<double>::twoPi * 330.0 * i / sr)) * (0.5f + 0.5f * std::sin ((float) (juce::MathConstants<double>::twoPi * 1.6 * i / sr))); b.setSample (0, i, v); b.setSample (1, i, v); }
          juce::WavAudioFormat wav; lf.deleteFile(); if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (new juce::FileOutputStream (lf), sr, 2, 24, {}, 0))) w->writeFromAudioSampleBuffer (b, 0, n); }
        auto* t = a13->addAudioTrack ("Loop");
        auto c = a13->importAudio (*t, lf, te::TimePosition());
        auto mc13 = std::make_unique<az::MainComponent> (*a13);
        mc13->getMedia().applySync (*c, lf);          // BPM SYNC: 100 -> 120 (autoTempo => proxy)
        const double cs = c->getPosition().getStart().inSeconds(), ce = c->getPosition().getEnd().inSeconds();
        std::printf ("       sinxron klip: %.3f-%.3f s, proxy=%d\n", cs, ce, (int) c->usesTimeStretchedProxy());
        auto* right = dynamic_cast<te::WaveAudioClip*> (t->splitClip (*c, te::TimePosition::fromSeconds ((cs + ce) / 2)));
        check (right != nullptr && c->usesTimeStretchedProxy(), "Sinxron (autoTempo) klip o'rtasidan kesildi");
        if (right)
        {
            const double rs = right->getPosition().getStart().inSeconds(), re = right->getPosition().getEnd().inSeconds();
            auto [t1, t2] = az::thumbRange (*right, rs, re);
            const double src0 = az::editToSourceTime (*right, rs), src1 = az::editToSourceTime (*right, re);
            std::printf ("       o'ng bo'lak: to'lqin oralig'i %.3f-%.3f, asl faylda %.3f-%.3f s\n", t1.inSeconds(), t2.inSeconds(), src0, src1);
            check (std::abs (t1.inSeconds()) < 1e-6 && std::abs (t2.inSeconds() - (re - rs)) < 1e-6 && std::abs (src0 - 2.0) < 0.02 && std::abs (src1 - 4.0) < 0.02,
                   "Kesilgan bo'lak to'lqini to'g'ri joydan chiziladi (proxy: klip boshidan, asl fayl: 2-4 s)");
            // ovoz: faqat o'ng bo'lak
            c->setMuted (true);
            const auto of = dir.getChildFile ("split_right.wav"); a13->exportWav (of, 32);
            juce::AudioBuffer<float> b; double sr; az::dop::readFile (a13->engine, of, b, sr);
            const float rmsR = b.getNumSamples() > 0 ? juce::Decibels::gainToDecibels (b.getRMSLevel (0, (int) (rs * sr), (int) ((re - rs) * sr * 0.9)), -200.0f) : -200.0f;
            // taqqoslash: kesilmagan holat (shu oraliqda)
            c->setMuted (false); right->setMuted (true);
            const auto ol = dir.getChildFile ("split_left.wav"); a13->exportWav (ol, 32);
            juce::AudioBuffer<float> bl; az::dop::readFile (a13->engine, ol, bl, sr);
            const float rmsL = juce::Decibels::gainToDecibels (bl.getRMSLevel (0, (int) (cs * sr), (int) ((rs - cs) * sr * 0.9)), -200.0f);
            int silent = 0; const int blk = (int) (sr * 0.05);
            for (int q = (int) (rs * sr); q + blk < (int) (re * sr * 0.98); q += blk) if (b.getMagnitude (0, q, blk) < 0.002f) ++silent;
            std::printf ("       ovoz: chap %.1f dB, o'ng %.1f dB RMS, o'ngda jim 50ms bloklar: %d\n", rmsL, rmsR, silent);
            check (rmsR > rmsL - 3 && silent == 0, "Kesilgan bo'lak ovozi yo'qolmagan (chap bo'lak bilan bir xil darajada)");
        }
        mc13.reset();
    }

    app2.reset(); app.reset();
    std::printf ("%s: %d ta xato\n", fails == 0 ? "NATIJA: HAMMASI O'TDI" : "NATIJA: XATOLAR BOR", fails);
    return fails == 0 ? 0 : 1;
}
