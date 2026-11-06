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
        check (std::abs (withCR - base) < 0.05f, "Monitor Match eksport/mixdown'ga ta'sir qilmaydi (faqat monitor)");

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

    app2.reset(); app.reset();
    std::printf ("%s: %d ta xato\n", fails == 0 ? "NATIJA: HAMMASI O'TDI" : "NATIJA: XATOLAR BOR", fails);
    return fails == 0 ? 0 : 1;
}
