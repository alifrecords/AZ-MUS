// =============================================================================
//  AZ MUS — Audio Editor (pastki zona, Cubase "Editor" kabi)
//  · AudioWarp: Musical Mode, asl BPM, algoritm, hitpoint'lar bo'yicha quantize + swing
//  · AZ Pitch: notalar "suv tomchisi" ko'rinishida, sudrash, tonallikka joylash, Correct / Straighten,
//    AUTO (aqlli) — fals notalarni topib tonallikka joylash
//  · Xor: bir bosishda tertsiya / kvarta / kvinta / oktava / xor (3 va 6 ovoz) — yangi treklarga
//  · Vokal tozalash: bir bosishda HPF + nafas + gate + de-esser
// =============================================================================
#pragma once
#include "Dop.h"
#include "Arrangement.h"

namespace az
{
class AudioEditor : public juce::Component, private juce::Timer
{
public:
    std::function<void()> onClose, onChanged;
    std::function<void (const juce::String&)> onToast;

    AudioEditor (AppEngine& a, ViewState& v) : app (a), vs (v)
    {
        const char* tabs[] = { "AudioWarp", "AZ Pitch", "Xor / Garmoniya", "Vokal tozalash" };
        for (int i = 0; i < 4; ++i)
        {
            auto* b = tabB.add (new juce::TextButton (U (tabs[i]))); addAndMakeVisible (b);
            b->setClickingTogglesState (true); b->setRadioGroupId (501); b->getProperties().set ("radius", 5.0f);
            b->onClick = [this, i] { setTab (i); };
        }
        auto btn = [this] (juce::TextButton& b, const juce::String& t, juce::Colour on = juce::Colour())
        { addAndMakeVisible (b); b.setButtonText (t); b.getProperties().set ("radius", 5.0f); b.setWantsKeyboardFocus (false); if (! on.isTransparent()) b.setColour (juce::TextButton::buttonOnColourId, on); };
        btn (closeB, juce::String::fromUTF8 ("\xc3\x97")); btn (playB, juce::String::fromUTF8 ("\xe2\x96\xb6  Klip")); btn (zoomInB, "+"); btn (zoomOutB, juce::String::fromUTF8 ("\xe2\x88\x92"));
        btn (musicalB, "Musical Mode", juce::Colour (0xff4ad6ff)); btn (quantB, U ("Hitpoint \xe2\x86\x92 Quantize"));
        btn (analyseB, U ("Qayta tahlil")); btn (autoB, U ("AUTO tuzatish (aqlli)"), juce::Colour (0xffff9a4a)); btn (resetB, U ("Asliga qaytarish"));
        btn (cleanB, U ("Vokalni tozalash (1 bosish)"), green); btn (cleanOffB, U ("Tozalashni olib tashlash"));
        btn (autoApplyB, "Auto Apply", green);
        musicalB.setClickingTogglesState (true); autoApplyB.setClickingTogglesState (true); autoApplyB.setToggleState (true, juce::dontSendNotification);
        playB.setClickingTogglesState (true);
        for (int i = 0; i < 8; ++i)
        {
            auto* b = harmB.add (new juce::TextButton (juce::String::fromUTF8 (pitch::presetNames[i]))); addAndMakeVisible (b);
            b->getProperties().set ("radius", 6.0f); b->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffb98aff));
            b->onClick = [this, i] { makeHarmony (i); };
        }
        auto combo = [this] (juce::ComboBox& c) { addAndMakeVisible (c); };
        combo (algoBox); algoBox.addItem ("SoundTouch Normal", 1); algoBox.addItem ("SoundTouch Better", 2); algoBox.setSelectedId (2, juce::dontSendNotification);
        combo (resBox); resBox.addItem ("1/8", 1); resBox.addItem ("1/16", 2); resBox.addItem ("1/4", 3); resBox.setSelectedId (2, juce::dontSendNotification);
        combo (rootBox); for (int i = 0; i < 12; ++i) rootBox.addItem (sync::noteNames[i], i + 1);
        combo (modeBox); modeBox.addItem ("Major", 1); modeBox.addItem ("Minor", 2);
        auto slider = [this] (juce::Slider& s, double lo, double hi, double st, const juce::String& suf)
        { addAndMakeVisible (s); s.setSliderStyle (juce::Slider::LinearBar); s.setRange (lo, hi, st); s.setTextValueSuffix (suf); s.setColour (juce::Slider::trackColourId, red.withAlpha (0.25f)); };
        slider (bpmS, 40, 240, 0.01, " BPM (asl)"); slider (swingS, 0, 75, 1, "% swing");
        slider (correctS, 0, 100, 1, "% Correct Pitch"); slider (straightS, 0, 100, 1, "% Straighten");
        slider (breathS, -30, 0, 0.5, U (" dB nafas")); slider (gateS, -60, 0, 1, " dB gate"); slider (deessS, -15, 0, 0.5, " dB de-ess");
        addAndMakeVisible (hpfT); hpfT.setButtonText ("HPF 80 Hz"); hpfT.setToggleState (true, juce::dontSendNotification);
        breathS.setValue (-12, juce::dontSendNotification); gateS.setValue (-30, juce::dontSendNotification); deessS.setValue (-6, juce::dontSendNotification);

        closeB.onClick = [this] { stopPreview(); if (onClose) onClose(); };
        playB.onClick = [this] { preview (playB.getToggleState()); };
        zoomInB.onClick = [this] { zoom *= 1.5; repaint(); };
        zoomOutB.onClick = [this] { zoom = juce::jmax (1.0, zoom / 1.5); scrollSec = juce::jmin (scrollSec, maxScroll()); repaint(); };
        musicalB.onClick = [this] { applyMusical(); };
        bpmS.onDragEnd = [this] { if (musicalB.getToggleState()) applyMusical(); };
        algoBox.onChange = [this] { if (auto* c = clip()) c->setTimeStretchMode (algoBox.getSelectedId() == 1 ? te::TimeStretcher::soundtouchNormal : te::TimeStretcher::soundtouchBetter); };
        quantB.onClick = [this] { quantizeHitpoints(); };
        analyseB.onClick = [this] { analyseClip (true); };
        autoB.onClick = [this] { runAuto(); };
        resetB.onClick = [this] { for (auto& n : notes) { n.pitch = n.orig; n.straighten = 0; n.gainDb = 0; } correctS.setValue (0, juce::dontSendNotification); straightS.setValue (0, juce::dontSendNotification); notesEdited (true); };
        correctS.onValueChange = [this] { applyCorrect(); };
        straightS.onValueChange = [this] { for (auto i : targets()) notes[(size_t) i].straighten = (float) straightS.getValue() / 100.0f; repaint(); };
        correctS.onDragEnd = [this] { notesEdited (true); };
        straightS.onDragEnd = [this] { notesEdited (true); };
        rootBox.onChange = [this] { repaint(); };
        modeBox.onChange = [this] { repaint(); };
        cleanB.onClick = [this] { applyClean (true); };
        cleanOffB.onClick = [this] { applyClean (false); };
        setTab (1);
        startTimerHz (30);
    }
    ~AudioEditor() override { stopTimer(); stopPreview(); }

    // ---------------------------------------------------------------- klip
    te::WaveAudioClip* clip() const { return dynamic_cast<te::WaveAudioClip*> (clipPtr.get()); }
    void setClip (te::WaveAudioClip* c)
    {
        stopPreview();
        clipPtr = c; notes.clear(); analysis = {}; selected.clear(); src.setSize (0, 0); hits.clear(); zoom = 1; scrollSec = 0; report = {};
        if (c == nullptr) { repaint(); return; }
        dop::readFile (app.engine, dop::originalFile (*c), src, srcRate);
        region = dop::clipRegion (*c);
        hits = detectTransients (app.engine, dop::originalFile (*c));
        auto fi = sync::analyse (app.engine, dop::originalFile (*c), projBpm());
        const double lb = c->getLoopInfo().getBpm (c->getAudioFile().getInfo());
        bpmS.setValue (lb > 30 ? lb : (fi.bpm > 0 ? fi.bpm : projBpm()), juce::dontSendNotification);
        musicalB.setToggleState (c->getAutoTempo(), juce::dontSendNotification);
        algoBox.setSelectedId (c->getTimeStretchMode() == te::TimeStretcher::soundtouchNormal ? 1 : 2, juce::dontSendNotification);
        auto k = sync::keyFromString (app.edit->state.getProperty ("azKey", "A minor").toString());
        rootBox.setSelectedId (k.root + 1, juce::dontSendNotification); modeBox.setSelectedId (k.minor ? 2 : 1, juce::dontSendNotification);
        // saqlangan tahlil bormi?
        if (auto p = pitchProc (false); p.isValid() && p.hasProperty ("f0"))
        {
            analysis = pitch::loadAnalysis (p); notes = pitch::loadNotes (p);
            if (std::abs (analysis.sampleRate - srcRate) > 1) { analysis = {}; notes.clear(); }
        }
        if (tab == 1 && notes.empty()) analyseClip (false);
        fitView();
        repaint();
    }
    sync::KeyInfo key() const { return { rootBox.getSelectedId() - 1, modeBox.getSelectedId() == 2, false }; }
    double projBpm() const { return app.edit ? app.edit->tempoSequence.getTempo (0)->getBpm() : 120.0; }

    void setTab (int t)
    {
        tab = t; for (int i = 0; i < tabB.size(); ++i) tabB[i]->setToggleState (i == t, juce::dontSendNotification);
        const bool warp = t == 0, pit = t == 1, har = t == 2, cln = t == 3;
        for (auto* c : std::initializer_list<juce::Component*> { &musicalB, &bpmS, &algoBox, &swingS, &resBox, &quantB }) c->setVisible (warp);
        for (auto* c : std::initializer_list<juce::Component*> { &analyseB, &autoB, &resetB, &correctS, &straightS, &autoApplyB }) c->setVisible (pit);
        rootBox.setVisible (pit || har); modeBox.setVisible (pit || har);
        for (auto* b : harmB) b->setVisible (har);
        correctS.setVisible (pit); straightS.setVisible (pit); resetB.setVisible (pit); autoApplyB.setVisible (pit);
        for (auto* c : std::initializer_list<juce::Component*> { &cleanB, &cleanOffB, &breathS, &gateS, &deessS, &hpfT }) c->setVisible (cln);
        if ((pit || har) && notes.empty() && clip()) analyseClip (false);
        resized(); repaint();
    }

    // ---------------------------------------------------------------- tahlil va saqlash
    juce::ValueTree pitchProc (bool create)
    {
        auto* c = clip(); if (! c) return {};
        auto st = dop::stack (*c, create); if (! st.isValid()) return {};
        for (auto p : st) if (p["type"].toString() == "pitch") return p;
        if (! create) return {};
        auto p = dop::makeProc ("pitch"); st.addChild (p, 0, nullptr);      // birinchi — toza asl ovozga
        return p;
    }
    void analyseClip (bool force)
    {
        if (! clip() || src.getNumSamples() == 0) return;
        if (! force && ! notes.empty()) return;
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        analysis = pitch::analyse (src, srcRate);
        notes = pitch::segment (analysis);
        auto p = pitchProc (true);
        pitch::storeAnalysis (p, analysis); pitch::storeNotes (p, notes, nullptr);
        report = juce::String ((int) notes.size()) + U (" nota topildi \xc2\xb7 tahlil ") + juce::String ((juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0, 2) + " s";
        fitView(); repaint();
    }
    void notesEdited (bool commit)
    {
        if (auto p = pitchProc (true); p.isValid())
        {
            if (commit) app.edit->getUndoManager().beginNewTransaction ("AZ Pitch");
            pitch::storeNotes (p, notes, commit ? &app.edit->getUndoManager() : nullptr);
        }
        repaint();
        if (autoApplyB.getToggleState()) pendingRender = juce::Time::getMillisecondCounter() + 450;
    }
    void renderNow()
    {
        pendingRender = 0;
        if (auto* c = clip())
        {
            dop::render (*c, app.engine, app.editFile.getParentDirectory().getChildFile ("Audio"));
            if (onChanged) onChanged();
        }
    }

    // ---------------------------------------------------------------- AZ Pitch amallari
    juce::Array<int> targets() const
    {
        juce::Array<int> r; if (! selected.isEmpty()) r = selected; else for (int i = 0; i < (int) notes.size(); ++i) r.add (i);
        return r;
    }
    void applyCorrect()
    {
        const float v = (float) correctS.getValue() / 100.0f; const auto k = key();
        for (auto i : targets()) { auto& n = notes[(size_t) i]; const int t = pitch::nearestScaleNote (n.orig, k); n.pitch = n.orig + ((float) t - n.orig) * v; }
        repaint();
    }
    void runAuto()
    {
        if (notes.empty()) analyseClip (true);
        auto sel = targets();
        std::vector<pitch::Note> sub; for (auto i : sel) sub.push_back (notes[(size_t) i]);
        auto r = pitch::autoCorrect (sub, key(), 1.0f, 0.35f);
        for (int k = 0; k < sel.size(); ++k) notes[(size_t) sel[k]] = sub[(size_t) k];
        correctS.setValue (100, juce::dontSendNotification); straightS.setValue (35, juce::dontSendNotification);
        report = U ("AUTO: ") + juce::String (r.fixedNotes) + U (" ta nota tonallikka joylandi \xc2\xb7 o'rtacha og'ish ") + juce::String ((int) r.avgCentsBefore)
               + juce::String::fromUTF8 (" \xe2\x86\x92 ") + juce::String ((int) r.avgCentsAfter) + " cent (" + sync::keyName (key()) + ")";
        ripple (getLocalBounds().getCentre().toFloat(), juce::Colour (0xffff9a4a));
        notesEdited (true);
    }

    // ---------------------------------------------------------------- Xor / garmoniya
    void makeHarmony (int presetId)
    {
        auto* c = clip(); if (! c || ! app.edit) return;
        if (notes.empty()) analyseClip (true);
        auto voices = pitch::preset (presetId);
        auto* srcTrack = dynamic_cast<te::AudioTrack*> (c->getTrack());
        auto dir = app.editFile.getParentDirectory().getChildFile ("Audio"); dir.createDirectory();
        app.edit->getUndoManager().beginNewTransaction ("Xor");
        te::Track* after = srcTrack;
        int made = 0;
        for (size_t vi = 0; vi < voices.size(); ++vi)
        {
            const auto& v = voices[vi];
            auto buf = pitch::renderVoice (src, analysis, notes, v, key(), 1234 + (int) vi * 77 + presetId * 13);
            const auto f = dir.getNonexistentChildFile (juce::File::createLegalFileName (c->getName() + " - " + v.name), ".wav");
            {
                juce::WavAudioFormat wav;
                std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), srcRate, (unsigned) buf.getNumChannels(), 24, {}, 0));
                if (! w) continue;
                w->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
            }
            auto* t = app.edit->insertNewAudioTrack (te::TrackInsertPoint (nullptr, after), nullptr).get();
            if (! t) continue;
            t->setName ((srcTrack ? srcTrack->getName() : juce::String ("Vokal")) + juce::String::fromUTF8 (" \xc2\xb7 ") + v.name);
            t->setColour (juce::Colour (0xffb98aff).withRotatedHue (0.04f * (float) vi));
            if (auto vp = t->getVolumePlugin()) vp->setPan (v.pan);
            auto nc = t->insertWaveClip (c->getName() + " " + v.name, f, { c->getPosition().time, c->getPosition().getOffset() }, false);
            if (nc) nc->setGainDB (0);
            after = t; ++made;
        }
        if (onToast) onToast (juce::String (made) + U (" ta ovoz yaratildi (") + juce::String::fromUTF8 (pitch::presetNames[presetId]) + U (") \xe2\x80\x94 tonallik: ") + sync::keyName (key()));
        if (onChanged) onChanged();
    }

    // ---------------------------------------------------------------- vokal tozalash
    void applyClean (bool on)
    {
        auto* c = clip(); if (! c) return;
        auto st = dop::stack (*c, true);
        juce::ValueTree p; for (auto q : st) if (q["type"].toString() == "vclean") p = q;
        app.edit->getUndoManager().beginNewTransaction (U ("Vokal tozalash"));
        if (! on) { if (p.isValid()) st.removeChild (p, &app.edit->getUndoManager()); }
        else
        {
            if (! p.isValid()) { p = dop::makeProc ("vclean"); st.addChild (p, -1, &app.edit->getUndoManager()); }
            p.setProperty ("hpf", hpfT.getToggleState(), nullptr); p.setProperty ("breath", breathS.getValue(), nullptr);
            p.setProperty ("gate", gateS.getValue(), nullptr); p.setProperty ("deess", deessS.getValue(), nullptr);
        }
        renderNow();
        report = on ? U ("Vokal tozalandi: HPF, nafas ") + juce::String (breathS.getValue(), 0) + " dB, gate " + juce::String (gateS.getValue(), 0) + " dB, de-ess " + juce::String (deessS.getValue(), 1) + " dB"
                    : U ("Tozalash olib tashlandi");
        if (onToast) onToast (report);
        repaint();
    }

    // ---------------------------------------------------------------- AudioWarp
    void applyMusical()
    {
        auto* c = clip(); if (! c) return;
        app.edit->getUndoManager().beginNewTransaction ("Musical Mode");
        if (musicalB.getToggleState())
        {
            c->setTimeStretchMode (algoBox.getSelectedId() == 1 ? te::TimeStretcher::soundtouchNormal : te::TimeStretcher::soundtouchBetter);
            c->getLoopInfo().setBpm (bpmS.getValue(), c->getAudioFile().getInfo());
            const double beats = c->getPosition().getLength().inSeconds() * bpmS.getValue() / 60.0;
            c->setAutoTempo (true);
            c->setLength (te::TimeDuration::fromSeconds (beats * 60.0 / projBpm()), true);
            report = U ("Musical Mode: ") + juce::String (bpmS.getValue(), 2) + U (" \xe2\x86\x92 ") + juce::String (projBpm(), 2) + " BPM";
        }
        else { c->setAutoTempo (false); report = U ("Musical Mode o'chirildi"); }
        if (onChanged) onChanged();
        repaint();
    }
    void quantizeHitpoints()
    {
        auto* c = clip(); if (! c || hits.empty()) return;
        auto* ct = dynamic_cast<te::ClipTrack*> (c->getTrack()); if (! ct) return;
        auto& ts = app.edit->tempoSequence;
        const double res = resBox.getSelectedId() == 1 ? 0.5 : resBox.getSelectedId() == 3 ? 1.0 : 0.25;
        const double swing = swingS.getValue() / 100.0;
        const double cs = c->getPosition().getStart().inSeconds(), off = c->getPosition().getOffset().inSeconds(), ce = c->getPosition().getEnd().inSeconds();
        app.edit->getUndoManager().beginNewTransaction ("Hitpoint Quantize");
        // kesish nuqtalari (oxiridan boshlab — kliplar ko'rsatkichlari buzilmasin)
        std::vector<double> cuts;
        juce::ignoreUnused (off);
        for (double h : hits) { const double et = sourceToEditTime (*c, h); if (et > cs + 0.02 && et < ce - 0.02) cuts.push_back (et); }
        juce::Array<te::Clip*> slices; slices.add (c);
        te::Clip* cur = c;
        for (double t : cuts)
        {
            auto* nc = ct->splitClip (*cur, te::TimePosition::fromSeconds (t));
            if (nc == nullptr) continue;
            slices.add (nc); cur = nc;
        }
        int moved = 0;
        for (auto* s : slices)
        {
            const double b = ts.toBeats (s->getPosition().getStart()).inBeats();
            double q = std::round (b / res) * res;
            const int idx = (int) std::lround (q / res);
            if (idx % 2 == 1) q += swing * res * 0.5;
            const auto nt = ts.toTime (te::BeatPosition::fromBeats (q));
            if (std::abs ((nt - s->getPosition().getStart()).inSeconds()) > 1e-4) { s->setStart (nt, false, true); ++moved; }
        }
        report = juce::String (slices.size()) + U (" bo'lak, ") + juce::String (moved) + U (" tasi gridga surildi (swing ") + juce::String ((int) swingS.getValue()) + "%)";
        setClip (nullptr);
        if (onChanged) onChanged();
        if (onToast) onToast (report);
    }

    // ---------------------------------------------------------------- tinglash
    void preview (bool on)
    {
        auto* c = clip(); if (! c || ! app.edit) return;
        auto& tr = app.edit->getTransport();
        if (on) { oldLoop = tr.getLoopRange(); oldLooping = tr.looping; tr.setLoopRange (c->getPosition().time); tr.looping = true; tr.setPosition (c->getPosition().getStart()); tr.play (false); previewing = true; }
        else stopPreview();
    }
    void stopPreview()
    {
        if (! previewing || ! app.edit) return;
        auto& tr = app.edit->getTransport(); tr.stop (false, false); tr.setLoopRange (oldLoop); tr.looping = oldLooping; previewing = false;
        playB.setToggleState (false, juce::dontSendNotification);
    }

    // ---------------------------------------------------------------- ko'rinish
    juce::Rectangle<float> view() const { return getLocalBounds().toFloat().withTrimmedLeft (leftW).withTrimmedTop (34).reduced (0, 0).withTrimmedBottom (4); }
    juce::Rectangle<float> noteArea() const { return view().withTrimmedLeft (tab == 0 ? 34 : 46).withTrimmedTop (18); }
    double visLen() const { return juce::jmax (0.2, region.getLength() / zoom); }
    double maxScroll() const { return juce::jmax (0.0, region.getLength() - visLen()); }
    float timeToX (double fileSec) const { auto a = noteArea(); return a.getX() + (float) ((fileSec - region.getStart() - scrollSec) / visLen()) * a.getWidth(); }
    double xToTime (float x) const { auto a = noteArea(); return region.getStart() + scrollSec + (double) (x - a.getX()) / a.getWidth() * visLen(); }
    float midiToY (float m) const { auto a = noteArea(); return a.getBottom() - (m - lowMidi) / (float) (highMidi - lowMidi) * a.getHeight(); }
    float yToMidi (float y) const { auto a = noteArea(); return lowMidi + (a.getBottom() - y) / a.getHeight() * (float) (highMidi - lowMidi); }
    void fitView()
    {
        if (notes.empty()) { lowMidi = 48; highMidi = 76; return; }
        float lo = 127, hi = 0; for (auto& n : notes) { lo = juce::jmin (lo, juce::jmin (n.pitch, n.orig)); hi = juce::jmax (hi, juce::jmax (n.pitch, n.orig)); }
        lowMidi = (int) std::floor (lo) - 4; highMidi = (int) std::ceil (hi) + 4;
        if (highMidi - lowMidi < 18) { const int c = (lowMidi + highMidi) / 2; lowMidi = c - 9; highMidi = c + 9; }
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto top = r.removeFromTop (34).reduced (8, 5);
        closeB.setBounds (top.removeFromRight (28)); top.removeFromRight (6);
        zoomInB.setBounds (top.removeFromRight (26)); top.removeFromRight (2); zoomOutB.setBounds (top.removeFromRight (26)); top.removeFromRight (8);
        playB.setBounds (top.removeFromRight (78));
        top.removeFromLeft (leftW - 8);
        for (auto* b : tabB) { b->setBounds (top.removeFromLeft (juce::jmin (150, juce::jmax (90, top.getWidth() / 5)))); top.removeFromLeft (4); }
        auto left = juce::Rectangle<int> (8, 40, leftW - 16, getHeight() - 46);
        auto row = [&] (int h) { auto x = left.removeFromTop (h); left.removeFromTop (5); return x; };
        if (tab == 0)
        {
            row (14); musicalB.setBounds (row (26)); bpmS.setBounds (row (22)); algoBox.setBounds (row (24)); row (6);
            swingS.setBounds (row (22)); resBox.setBounds (row (24)); quantB.setBounds (row (28));
        }
        else if (tab == 1 || tab == 2)
        {
            row (14);
            auto kr = row (24); rootBox.setBounds (kr.removeFromLeft (64)); kr.removeFromLeft (4); modeBox.setBounds (kr);
            if (tab == 1)
            {
                autoB.setBounds (row (30)); correctS.setBounds (row (22)); straightS.setBounds (row (22));
                auto rr = row (24); analyseB.setBounds (rr.removeFromLeft (rr.getWidth() / 2 - 2)); rr.removeFromLeft (4); resetB.setBounds (rr);
                autoApplyB.setBounds (row (22));
            }
            else
            {
                row (16);
                for (int i = 0; i < 8; ++i) { const int w = (left.getWidth() - 6) / 2; harmB[i]->setBounds (left.getX() + (i % 2) * (w + 6), left.getY() + (i / 2) * 34, w, 30); }
            }
        }
        else
        {
            row (14); cleanB.setBounds (row (32)); hpfT.setBounds (row (22)); breathS.setBounds (row (22)); gateS.setBounds (row (22)); deessS.setBounds (row (22)); cleanOffB.setBounds (row (24));
        }
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        g.fillAll (Colour (0xff0c0b0e));
        g.setColour (line); g.drawHorizontalLine (0, 0, (float) getWidth());
        // chap panel
        auto lp = Rectangle<float> (0, 34, (float) leftW, (float) getHeight() - 34);
        g.setColour (Colour (0xff121015)); g.fillRect (lp);
        g.setColour (line); g.drawVerticalLine (leftW - 1, 34, (float) getHeight());
        caps (g, "EDITOR", { 12, 11, 70, 12 }, 9.5f, redSoft);
        auto* c = clip();
        label (g, c ? c->getName() : U ("Audio klipni 2 marta bosing"), { 70, 6, (float) leftW - 78, 22 }, 10.5f, text, Justification::centredLeft, 600);
        const char* heads[] = { "AUDIOWARP", "AZ PITCH \xc2\xb7 VARIAUDIO", "XOR / GARMONIYA", "VOKAL TOZALASH" };
        caps (g, String::fromUTF8 (heads[tab]), { 10, 42, (float) leftW - 20, 12 }, 8.5f, dim);
        if (tab == 2) label (g, U ("Bir bosishda: tanlangan tonallik bo'yicha diatonik interval, inson kabi kechikish va detune bilan yangi treklar"),
                             { 10, (float) getHeight() - 52, (float) leftW - 20, 40 }, 8.5f, dim2, Justification::topLeft, 400);
        if (! c || src.getNumSamples() == 0)
        {
            label (g, U ("Aranjirovkada audio klipni 2 marta bosing (yoki tanlab Enter) \xe2\x80\x94 bu yerda ochiladi"), view(), 12, dim, Justification::centred, 500);
            return;
        }
        auto v = view(); auto a = noteArea();
        g.setColour (Colour (0xff08070a)); g.fillRect (v);
        drawRuler (g, v.withHeight (18).withTrimmedLeft (a.getX() - v.getX()));
        if (tab == 0) drawWave (g, a); else drawPitch (g, a);
        // ijro chizig'i
        if (app.edit)
        {
            const double pos = app.edit->getTransport().getPosition().inSeconds();
            const double ft = editToSourceTime (*c, pos);
            const float x = timeToX (ft);
            if (x >= a.getX() && x <= a.getRight()) { g.setColour (Colours::white.withAlpha (0.8f)); g.fillRect (x - 0.5f, v.getY(), 1.0f, v.getHeight()); }
        }
        if (report.isNotEmpty())
        {
            auto rr = Rectangle<float> ((float) leftW + 10, (float) getHeight() - 26, jmin ((float) getWidth() - leftW - 20, textWidth (font (9.5f, 600), report) + 24), 20);
            g.setColour (Colour (0xff0c0b0e).withAlpha (0.85f)); g.fillRoundedRectangle (rr, 5);
            label (g, report, rr.reduced (10, 0), 9.5f, yellow, Justification::centredLeft, 600);
        }
        // to'lqin effekti (ripple)
        for (auto& rp : ripples)
        {
            const float t = rp.age / 24.0f;
            g.setColour (rp.col.withAlpha (0.45f * (1 - t))); g.drawEllipse (rp.p.x - 8 - 60 * t, rp.p.y - 5 - 30 * t, 16 + 120 * t, 10 + 60 * t, 1.6f * (1 - t) + 0.4f);
        }
    }
    void drawRuler (juce::Graphics& g, juce::Rectangle<float> r)
    {
        using namespace juce;
        g.setColour (Colour (0xff7d5cff).withAlpha (0.55f)); g.fillRect (r);
        auto* c = clip(); if (! c || ! app.edit) return;
        auto& ts = app.edit->tempoSequence;
        const double t0 = sourceToEditTime (*c, region.getStart() + scrollSec), t1 = sourceToEditTime (*c, region.getStart() + scrollSec + visLen());
        const double b0 = ts.toBeats (te::TimePosition::fromSeconds (jmax (0.0, t0))).inBeats(), b1 = ts.toBeats (te::TimePosition::fromSeconds (t1)).inBeats();
        const double step = (b1 - b0) > 64 ? 4 : (b1 - b0) > 16 ? 1 : 0.5;
        for (double b = std::ceil (b0 / step) * step; b < b1; b += step)
        {
            const double et = ts.toTime (te::BeatPosition::fromBeats (b)).inSeconds();
            const float x = timeToX (editToSourceTime (*c, et));
            const bool bar = std::abs (std::fmod (b, 4.0)) < 1e-6;
            g.setColour (Colours::white.withAlpha (bar ? 0.85f : 0.4f)); g.fillRect (x, r.getY() + (bar ? 2 : 10), 1.0f, bar ? 16.0f : 8.0f);
            if (bar) label (g, String ((int) (b / 4) + 1), { x + 3, r.getY(), 30, 18 }, 9.5f, Colours::white, Justification::centredLeft, 700);
            g.setColour (Colours::white.withAlpha (bar ? 0.07f : 0.03f)); g.fillRect (x, r.getBottom(), 1.0f, (float) getHeight() - r.getBottom() - 4);
        }
    }
    void drawWave (juce::Graphics& g, juce::Rectangle<float> a)
    {
        using namespace juce;
        const int W = (int) a.getWidth(); const int C = jmin (2, src.getNumChannels());
        for (int ch = 0; ch < C; ++ch)
        {
            auto lane = a.withHeight (a.getHeight() / C).translated (0, a.getHeight() / C * ch).reduced (0, 6);
            for (double db : { 0.0, -6.0 })
            {
                const float gpos = Decibels::decibelsToGain ((float) db) * lane.getHeight() * 0.48f;
                g.setColour (Colours::white.withAlpha (0.05f)); g.drawHorizontalLine ((int) (lane.getCentreY() - gpos), lane.getX(), lane.getRight()); g.drawHorizontalLine ((int) (lane.getCentreY() + gpos), lane.getX(), lane.getRight());
                label (g, db == 0 ? "0" : "-6", { a.getX() - 30, lane.getCentreY() - gpos - 6, 26, 12 }, 8, dim2, Justification::centredRight, 400);
            }
            for (int x = 0; x < W; ++x)
            {
                const double t0 = xToTime (a.getX() + x), t1 = xToTime (a.getX() + x + 1);
                const int s0 = jlimit (0, src.getNumSamples() - 1, (int) (t0 * srcRate)), s1 = jlimit (s0 + 1, src.getNumSamples(), (int) (t1 * srcRate));
                const auto mm = src.findMinMax (ch, s0, s1 - s0);
                g.setColour (Colour (0xffc9c4cc).withAlpha (0.8f));
                g.fillRect (a.getX() + x, lane.getCentreY() - mm.getEnd() * lane.getHeight() * 0.48f, 1.0f, jmax (1.0f, (mm.getEnd() - mm.getStart()) * lane.getHeight() * 0.48f));
            }
        }
        // hitpoint'lar
        for (double h : hits)
        {
            const float x = timeToX (h); if (x < a.getX() || x > a.getRight()) continue;
            g.setColour (Colour (0xff4ad6ff).withAlpha (0.55f)); g.fillRect (x, a.getY(), 1.0f, a.getHeight());
            Path tri; tri.addTriangle (x - 4, a.getY(), x + 4, a.getY(), x, a.getY() + 6); g.fillPath (tri);
        }
    }
    void drawPitch (juce::Graphics& g, juce::Rectangle<float> a)
    {
        using namespace juce;
        const auto k = key();
        // klaviatura va tonallik qatorlari
        auto keys = Rectangle<float> (a.getX() - 46, a.getY(), 44, a.getHeight());
        for (int m = lowMidi; m <= highMidi; ++m)
        {
            const float y0 = midiToY ((float) m + 0.5f), y1 = midiToY ((float) m - 0.5f);
            const int pc = m % 12; const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
            const bool inK = pitch::inScale (pc, k);
            g.setColour (inK ? Colour (0xff4ad6ff).withAlpha (pc == k.root ? 0.07f : 0.035f) : Colours::transparentBlack); g.fillRect (a.getX(), y0, a.getWidth(), y1 - y0);
            g.setColour (Colours::white.withAlpha (0.03f)); g.drawHorizontalLine ((int) y1, a.getX(), a.getRight());
            g.setColour (black ? Colour (0xff1a181d) : Colour (0xffd8d4da)); g.fillRect (keys.getX(), y0 + 0.5f, black ? 28.0f : 44.0f, y1 - y0 - 1);
            if (pc == 0) label (g, "C" + String (m / 12 - 2), { keys.getX() + 20, y0, 22, y1 - y0 }, 8, Colour (0xff222026), Justification::centredRight, 700);
        }
        // to'lqin (fon)
        {
            const int W = (int) a.getWidth();
            for (int x = 0; x < W; x += 2)
            {
                const double t0 = xToTime (a.getX() + x), t1 = xToTime (a.getX() + x + 2);
                const int s0 = jlimit (0, src.getNumSamples() - 1, (int) (t0 * srcRate)), s1 = jlimit (s0 + 1, src.getNumSamples(), (int) (t1 * srcRate));
                const float m = src.getMagnitude (0, s0, s1 - s0);
                g.setColour (Colours::white.withAlpha (0.045f)); g.fillRect (a.getX() + x, a.getCentreY() - m * a.getHeight() * 0.45f, 2.0f, m * a.getHeight() * 0.9f);
            }
        }
        // notalar — "suv tomchisi"
        for (int i = 0; i < (int) notes.size(); ++i)
        {
            const auto& n = notes[(size_t) i];
            const float x0 = timeToX (n.start), x1 = timeToX (n.end);
            if (x1 < a.getX() || x0 > a.getRight()) continue;
            const bool sel = selected.contains (i);
            const float dev = n.pitch - (float) pitch::nearestScaleNote (n.pitch, k);
            const bool inTune = std::abs (dev) < 0.1f;
            const float shown = i == dragIdx ? dragShown : n.pitch;
            const float yc = midiToY (shown), h = jmax (10.0f, (midiToY (0) - midiToY (1)) * 0.9f);
            auto r = Rectangle<float> (x0, yc - h / 2, jmax (4.0f, x1 - x0), h);
            const Colour base = inTune ? Colour (0xff3ddc97) : std::abs (dev) < 0.3f ? Colour (0xffffc04a) : Colour (0xffff4d6d);
            const Colour col = sel ? base.brighter (0.35f) : base;
            // tomchi: gradient + glyans + soya
            g.setColour (Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (r.translated (0, 2), h / 2);
            g.setGradientFill (ColourGradient (col.withAlpha (0.95f), r.getX(), r.getY(), col.darker (0.6f).withAlpha (0.9f), r.getX(), r.getBottom(), false));
            g.fillRoundedRectangle (r, h / 2);
            g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.45f), r.getX(), r.getY(), Colours::transparentWhite, r.getX(), r.getY() + h * 0.55f, false));
            g.fillRoundedRectangle (r.reduced (2, 1).withHeight (h * 0.42f), h / 3);
            if (sel) { g.setColour (Colours::white); g.drawRoundedRectangle (r.expanded (1), h / 2, 1.4f); }
            // asl joy (agar siljigan bo'lsa) — xira kontur
            if (std::abs (n.pitch - n.orig) > 0.05f)
            {
                const float yo = midiToY (n.orig);
                g.setColour (Colours::white.withAlpha (0.18f)); g.drawRoundedRectangle (Rectangle<float> (x0, yo - h / 2, r.getWidth(), h), h / 2, 1.0f);
                g.drawLine (x0 + r.getWidth() * 0.5f, yo, x0 + r.getWidth() * 0.5f, yc, 0.8f);
            }
            // ichidagi pitch egri chizig'i (siljish + straighten bilan)
            if (analysis.frames() > 0)
            {
                Path p; bool st = false;
                const int f0 = (int) (n.start / pitch::hopSec), f1 = jmin (analysis.frames(), (int) (n.end / pitch::hopSec));
                for (int f = f0; f < f1; ++f)
                {
                    const float m = analysis.midiAt (f); if (m <= 0) continue;
                    const float mm = shown + (m - n.orig) * (1 - n.straighten);
                    const float x = timeToX (f * pitch::hopSec), y = midiToY (mm);
                    if (! st) { p.startNewSubPath (x, y); st = true; } else p.lineTo (x, y);
                }
                g.setColour (Colours::white.withAlpha (0.85f)); g.strokePath (p, PathStrokeType (1.1f, PathStrokeType::curved));
            }
            if (r.getWidth() > 34)
                label (g, MidiMessage::getMidiNoteName ((int) std::lround (shown), true, true, 3) + (std::abs (shown - std::round (shown)) > 0.04f ? String (" ") + (shown > std::round (shown) ? "+" : "") + String ((int) std::lround ((shown - std::round (shown)) * 100)) : String()),
                       r.reduced (6, 0), 8.5f, Colour (0xff101012), Justification::centredLeft, 700);
        }
        if (notes.empty()) label (g, U ("Notalar topilmadi \xe2\x80\x94 \"Qayta tahlil\" tugmasini bosing (vokal monofonik bo'lishi kerak)"), a, 11, dim, Justification::centred, 500);
        if (marq) { g.setColour (Colours::white.withAlpha (0.08f)); g.fillRect (*marq); g.setColour (Colours::white.withAlpha (0.4f)); g.drawRect (*marq, 1.0f); }
    }

    // ---------------------------------------------------------------- sichqoncha
    int noteAt (juce::Point<float> p) const
    {
        for (int i = (int) notes.size(); --i >= 0;)
        {
            const auto& n = notes[(size_t) i];
            const float h = juce::jmax (10.0f, (midiToY (0) - midiToY (1)) * 0.9f);
            if (juce::Rectangle<float> (timeToX (n.start), midiToY (n.pitch) - h / 2, juce::jmax (4.0f, timeToX (n.end) - timeToX (n.start)), h).expanded (2).contains (p)) return i;
        }
        return -1;
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        dragIdx = -1; marq.reset();
        if (tab == 0 || ! noteArea().contains (e.position)) return;
        const int i = noteAt (e.position);
        if (i < 0) { if (! e.mods.isShiftDown()) selected.clear(); marq = juce::Rectangle<float> (e.position, e.position); repaint(); return; }
        if (e.mods.isShiftDown()) { if (selected.contains (i)) selected.removeFirstMatchingValue (i); else selected.add (i); }
        else if (! selected.contains (i)) { selected.clear(); selected.add (i); }
        dragIdx = i; dragStartPitch = notes[(size_t) i].pitch; dragShown = dragStartPitch;
        dragStarts.clear(); for (auto s : selected) dragStarts.add (notes[(size_t) s].pitch);
        repaint();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (marq) { marq = juce::Rectangle<float> (e.mouseDownPosition, e.position); selected.clear();
                    for (int i = 0; i < (int) notes.size(); ++i) { const auto& n = notes[(size_t) i]; if (marq->intersects (juce::Rectangle<float> (timeToX (n.start), midiToY (n.pitch) - 4, timeToX (n.end) - timeToX (n.start), 8.0f))) selected.add (i); }
                    repaint(); return; }
        if (dragIdx < 0) return;
        const float semis = (float) -e.getDistanceFromDragStartY() / (midiToY (0) - midiToY (1));
        float target = dragStartPitch + semis;
        if (! e.mods.isAltDown()) target = std::round (target);         // yarim tonga yopishadi; Alt — erkin (cent)
        const float delta = target - dragStartPitch;
        for (int k = 0; k < selected.size(); ++k) notes[(size_t) selected[k]].pitch = dragStarts[k] + delta;
        dragTarget = target;
        repaint();
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (marq) { marq.reset(); repaint(); return; }
        if (dragIdx >= 0 && e.mouseWasDraggedSinceMouseDown())
        {
            ripple ({ timeToX ((notes[(size_t) dragIdx].start + notes[(size_t) dragIdx].end) * 0.5), midiToY (notes[(size_t) dragIdx].pitch) }, juce::Colour (0xff4ad6ff));
            notesEdited (true);
        }
        dragIdx = -1;
    }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int i = noteAt (e.position); if (i < 0) return;
        auto& n = notes[(size_t) i];
        n.pitch = (float) pitch::nearestScaleNote (n.orig, key());     // tonallikka joylash
        ripple (e.position, juce::Colour (0xff3ddc97));
        notesEdited (true);
    }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (e.mods.isCommandDown()) { zoom = juce::jlimit (1.0, 200.0, zoom * (w.deltaY > 0 ? 1.25 : 0.8)); scrollSec = juce::jlimit (0.0, maxScroll(), scrollSec); }
        else if (e.mods.isShiftDown() && tab != 0) { const int d = w.deltaY > 0 ? 1 : -1; lowMidi += d; highMidi += d; }
        else scrollSec = juce::jlimit (0.0, maxScroll(), scrollSec - (w.deltaY != 0 ? w.deltaY : w.deltaX) * visLen() * 0.3);
        repaint();
    }
    bool keyPressed (const juce::KeyPress& k) override
    {
        if (tab != 1) return false;
        const int d = k.getKeyCode() == juce::KeyPress::upKey ? 1 : k.getKeyCode() == juce::KeyPress::downKey ? -1 : 0;
        if (d != 0 && ! selected.isEmpty()) { for (auto i : selected) notes[(size_t) i].pitch += d; notesEdited (true); return true; }
        if (k.getModifiers().isCommandDown() && k.getKeyCode() == 'A') { selected.clear(); for (int i = 0; i < (int) notes.size(); ++i) selected.add (i); repaint(); return true; }
        return false;
    }

    // test uchun
    std::vector<pitch::Note>& getNotes() { return notes; }
    const pitch::Analysis& getAnalysis() const { return analysis; }
    juce::String getReport() const { return report; }
    juce::Slider& swingSlider() { return swingS; }
    juce::ComboBox& rootCombo() { return rootBox; }
    juce::ComboBox& modeCombo() { return modeBox; }

private:
    struct Ripple { juce::Point<float> p; juce::Colour col; int age = 0; };
    std::vector<Ripple> ripples;
    void ripple (juce::Point<float> p, juce::Colour c) { ripples.push_back ({ p, c, 0 }); }
    void timerCallback() override
    {
        bool rp = false;
        for (auto& r : ripples) { ++r.age; rp = true; }
        ripples.erase (std::remove_if (ripples.begin(), ripples.end(), [] (const Ripple& r) { return r.age > 24; }), ripples.end());
        // sudralgan tomchi maqsadga silliq "oqib" boradi
        if (dragIdx >= 0 && std::abs (dragShown - notes[(size_t) dragIdx].pitch) > 0.002f) { dragShown += (notes[(size_t) dragIdx].pitch - dragShown) * 0.35f; rp = true; }
        if (pendingRender != 0 && juce::Time::getMillisecondCounter() > pendingRender) renderNow();
        if (rp || (app.edit && app.edit->getTransport().isPlaying() && clip())) repaint();
    }
    AppEngine& app; ViewState& vs;
    te::Clip::Ptr clipPtr;
    juce::AudioBuffer<float> src; double srcRate = 48000; juce::Range<double> region;
    pitch::Analysis analysis; std::vector<pitch::Note> notes; std::vector<double> hits;
    juce::Array<int> selected; juce::Array<float> dragStarts;
    int dragIdx = -1; float dragStartPitch = 0, dragShown = 0, dragTarget = 0;
    std::optional<juce::Rectangle<float>> marq;
    int tab = 1, lowMidi = 48, highMidi = 76; double zoom = 1, scrollSec = 0;
    juce::uint32 pendingRender = 0;
    static constexpr int leftW = 230;
    juce::String report;
    bool previewing = false, oldLooping = false; te::TimeRange oldLoop;
    juce::OwnedArray<juce::TextButton> tabB, harmB;
    juce::TextButton closeB, playB, zoomInB, zoomOutB, musicalB, quantB, analyseB, autoB, resetB, cleanB, cleanOffB, autoApplyB;
    juce::ComboBox algoBox, resBox, rootBox, modeBox;
    juce::Slider bpmS, swingS, correctS, straightS, breathS, gateS, deessS;
    juce::ToggleButton hpfT;
};
}
