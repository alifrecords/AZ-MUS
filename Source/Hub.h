// =============================================================================
//  AZ MUS — Bosh sahifa (Hub): profil (artist / guruh), yangi loyiha, loyihalar
//  to'ri, shablonlar, savat va AZ MUSIC Playlist (az-playlist.pages.dev) pleyeri.
//  DAW ishga tushganda avtomatik oxirgi loyihaga kirmaydi — shu sahifa ochiladi.
// =============================================================================
#pragma once
#include "AppEngine.h"
#include "Routing.h"
#include <thread>

namespace az
{
namespace hub
{
    inline juce::File appDataDir()
    {
        auto d = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("AZ MUS");
        d.createDirectory(); return d;
    }
    inline juce::File thumbsDir()   { auto d = AppEngine::projectsFolder().getChildFile (".thumbs"); d.createDirectory(); return d; }
    inline juce::File trashDir()    { auto d = AppEngine::projectsFolder().getChildFile (".Savat");  d.createDirectory(); return d; }
    inline juce::File playlistCache() { auto d = appDataDir().getChildFile ("playlist"); d.createDirectory(); return d; }
    inline juce::String keyOf (const juce::File& f) { return juce::String::toHexString (f.getFullPathName().hashCode64()); }
    inline juce::File thumbFile (const juce::File& p) { return thumbsDir().getChildFile (keyOf (p) + ".png"); }
    inline juce::File coverFile (const juce::File& p) { return thumbsDir().getChildFile (keyOf (p) + ".cover.png"); }
    inline juce::File metaFile  (const juce::File& p) { return thumbsDir().getChildFile (keyOf (p) + ".json"); }

    inline bool saveImage (const juce::Image& img, const juce::File& f)
    {
        if (! img.isValid()) return false;
        f.deleteFile(); juce::FileOutputStream os (f);
        juce::PNGImageFormat png; return os.openedOk() && png.writeImageToStream (img, os);
    }
    // rasmni berilgan o'lchamga "cover" usulida kesib kichraytirish
    inline juce::Image coverCrop (const juce::Image& src, int w, int h)
    {
        if (! src.isValid()) return {};
        juce::Image out (juce::Image::ARGB, w, h, true);
        juce::Graphics g (out); g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        const float s = juce::jmax ((float) w / src.getWidth(), (float) h / src.getHeight());
        const float dw = src.getWidth() * s, dh = src.getHeight() * s;
        g.drawImage (src, juce::Rectangle<float> ((w - dw) * 0.5f, (h - dh) * 0.5f, dw, dh));
        return out;
    }
    inline void drawCover (juce::Graphics& g, const juce::Image& img, juce::Rectangle<float> r, float radius)
    {
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path clip; clip.addRoundedRectangle (r, radius); g.reduceClipRegion (clip);
        const float s = juce::jmax (r.getWidth() / img.getWidth(), r.getHeight() / img.getHeight());
        const float dw = img.getWidth() * s, dh = img.getHeight() * s;
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (img, juce::Rectangle<float> (r.getCentreX() - dw * 0.5f, r.getCentreY() - dh * 0.5f, dw, dh));
    }

    // ---------------------------------------------------------------- loyiha ma'lumotlari
    struct Project
    {
        juce::File file; juce::String name; juce::Time modified; juce::int64 bytes = 0;
        double bpm = 0, length = 0; int tracks = 0; juce::String key;
        juce::Image thumb;   // muqova (foydalanuvchi qo'ygan) yoki aranjirovka surati
        bool customCover = false;
    };
    inline void writeMeta (const juce::File& projectFile, double bpm, const juce::String& key, double lengthSec, int tracks)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("bpm", bpm); o->setProperty ("key", key); o->setProperty ("length", lengthSec); o->setProperty ("tracks", tracks);
        o->setProperty ("file", projectFile.getFullPathName());
        metaFile (projectFile).replaceWithText (juce::JSON::toString (juce::var (o)));
    }
    inline Project readProject (const juce::File& f, bool withImage = true)
    {
        Project p; p.file = f; p.name = f.getFileNameWithoutExtension(); p.modified = f.getLastModificationTime(); p.bytes = f.getSize();
        const auto m = juce::JSON::parse (metaFile (f));
        if (m.isObject()) { p.bpm = m["bpm"]; p.length = m["length"]; p.tracks = m["tracks"]; p.key = m["key"].toString(); }
        if (withImage)
        {
            if (coverFile (f).existsAsFile()) { p.thumb = juce::ImageFileFormat::loadFrom (coverFile (f)); p.customCover = p.thumb.isValid(); }
            if (! p.thumb.isValid() && thumbFile (f).existsAsFile()) p.thumb = juce::ImageFileFormat::loadFrom (thumbFile (f));
        }
        return p;
    }
    inline void addRecent (juce::PropertiesFile& pf, const juce::File& f)
    {
        juce::StringArray r; r.addLines (pf.getValue ("azRecent"));
        r.removeString (f.getFullPathName()); r.insert (0, f.getFullPathName());
        while (r.size() > 40) r.remove (r.size() - 1);
        pf.setValue ("azRecent", r.joinIntoString ("\n"));
    }
    inline juce::Array<juce::File> listProjects (juce::PropertiesFile* pf)
    {
        juce::Array<juce::File> fs;
        const auto root = AppEngine::projectsFolder();
        auto scanDir = [&] (const juce::File& d) { for (const auto& e : juce::RangedDirectoryIterator (d, false, "*.azmus", juce::File::findFiles)) fs.addIfNotAlreadyThere (e.getFile()); };
        scanDir (root);
        for (const auto& e : juce::RangedDirectoryIterator (root, false, "*", juce::File::findDirectories))   // bir daraja ichki papkalar
            if (! e.getFile().getFileName().startsWithChar ('.')) scanDir (e.getFile());
        if (pf != nullptr)
        {
            juce::StringArray r; r.addLines (pf->getValue ("azRecent"));
            const auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory);
            for (auto& s : r) if (juce::File f (s); s.isNotEmpty() && f.existsAsFile() && ! f.isAChildOf (root.getChildFile (".Savat")) && ! f.isAChildOf (tmp)) fs.addIfNotAlreadyThere (f);
        }
        std::sort (fs.begin(), fs.end(), [] (const juce::File& a, const juce::File& b) { return a.getLastModificationTime() > b.getLastModificationTime(); });
        return fs;
    }
    inline bool moveToTrash (const juce::File& f)
    {
        if (! f.existsAsFile()) return false;
        const auto dst = trashDir().getNonexistentChildFile (f.getFileNameWithoutExtension(), ".azmus", false);
        const auto th = thumbFile (f), cv = coverFile (f), me = metaFile (f);
        if (! f.moveFileTo (dst)) return false;
        th.moveFileTo (thumbFile (dst)); cv.moveFileTo (coverFile (dst)); me.moveFileTo (metaFile (dst));
        return true;
    }
    inline juce::File restoreFromTrash (const juce::File& f)
    {
        const auto dst = AppEngine::projectsFolder().getNonexistentChildFile (f.getFileNameWithoutExtension(), ".azmus", false);
        const auto th = thumbFile (f), cv = coverFile (f), me = metaFile (f);
        if (! f.moveFileTo (dst)) return {};
        th.moveFileTo (thumbFile (dst)); cv.moveFileTo (coverFile (dst)); me.moveFileTo (metaFile (dst));
        return dst;
    }
    inline juce::String sizeText (juce::int64 b)
    {
        if (b >= 1024 * 1024) return juce::String (b / (1024.0 * 1024.0), 1) + "M";
        return juce::String (juce::jmax ((juce::int64) 1, b / 1024)) + "K";
    }
    inline juce::String timeText (double s)
    {
        const int t = juce::roundToInt (s);
        return juce::String (t / 60).paddedLeft ('0', 2) + ":" + juce::String (t % 60).paddedLeft ('0', 2);
    }
    inline juce::String sanitizeName (juce::String n)
    {
        n = juce::File::createLegalFileName (n.trim());
        return n.isEmpty() ? juce::String::fromUTF8 ("Yangi qo'shiq") : n;
    }

    // ---------------------------------------------------------------- shablonlar (aranjirovshiklar uchun)
    struct Template { const char* name; const char* desc; double bpm; juce::uint32 colour; };
    inline const Template templates[] = {
        { "Bo'sh loyiha",       "2 audio + 1 instrument trek",                   120, 0xff8d8587 },
        { "Vokal yozish",       "Lead, Back, Dubl vokal + minus (beat)",         100, 0xffff3347 },
        { "Aranjirovka",        "AZ BEAT, Bass, Keys, Pad, Gitara, vokallar",    110, 0xffffc04a },
        { "Beat yasash",        "AZ BEAT, 808 Bass, Melodiya, Sample",           140, 0xffb98aff },
        { "Mastering",          "Stereo mix + referens trek",                    120, 0xff3ddc97 },
        { "Ovoz / Podkast",     "2 mikrofon + fon musiqasi",                      90, 0xff8ab4ff },
    };
    constexpr int numTemplates = (int) std::size (templates);

    inline void applyTemplate (AppEngine& app, int id)
    {
        if (! app.edit || id <= 0 || id >= numTemplates) return;
        for (auto* t : te::getAudioTracks (*app.edit)) app.edit->deleteTrack (t);
        auto A = [&] (const char* n) { return app.addAudioTrack (juce::String::fromUTF8 (n)); };
        auto I = [&] (const char* n) { return app.addInstrumentTrack (juce::String::fromUTF8 (n)); };
        switch (id)
        {
            case 1: A ("Minus (beat)"); A ("Lead Vokal"); A ("Back Vokal"); A ("Dubl"); break;
            case 2: addBeatTrack (app); I ("Bass"); I ("Keys"); I ("Pad"); A ("Gitara"); A ("Lead Vokal"); A ("Back Vokal"); break;
            case 3: addBeatTrack (app); I ("808 Bass"); I ("Melodiya"); A ("Sample"); break;
            case 4: A ("Mix (stereo)"); A ("Referens"); break;
            case 5: A ("Mikrofon 1"); A ("Mikrofon 2"); A ("Fon musiqasi"); break;
            default: break;
        }
        app.edit->getUndoManager().clearUndoHistory();
    }

    // ---------------------------------------------------------------- AZ MUSIC Playlist
    struct Song
    {
        juce::String id, title, genre, key, voice, mood, status, audio, cover, added;
        double bpm = 0, duration = 0; bool introOnly = false;
        juce::Array<float> peaks;
        juce::Image coverImg;
    };
    inline juce::Array<Song> parsePlaylist (const juce::String& json, juce::String* artistOut = nullptr)
    {
        juce::Array<Song> r;
        const auto v = juce::JSON::parse (json);
        if (! v.isObject()) return r;
        if (artistOut) *artistOut = v["settings"]["artist"].toString();
        const auto today = juce::Time::getCurrentTime().formatted ("%Y-%m-%d");
        if (auto* arr = v["tracks"].getArray())
            for (auto& t : *arr)
            {
                Song s;
                s.id = t["id"].toString(); s.title = t["title"].toString(); s.genre = t["genre"].toString();
                s.key = t["key"].toString().replaceCharacter ('-', ' '); s.voice = t["voice"].toString(); s.mood = t["mood"].toString();
                s.status = t["status"].toString(); s.added = t["added"].toString();
                s.bpm = t["bpm"]; s.duration = t["duration"]; s.introOnly = (bool) t["introOnly"];
                s.audio = s.introOnly || t["audio"].toString().isEmpty() ? t["intro"].toString() : t["audio"].toString();
                if (auto* m = t["media"].getArray())
                    for (auto& mm : *m) if (mm["type"].toString() == "image") { s.cover = mm["src"].toString(); break; }
                if (s.cover.isEmpty()) s.cover = t["og"].toString();
                if (auto* pk = t["peaks"].getArray()) for (auto& p : *pk) s.peaks.add ((float) (double) p);
                const auto pub = t["publishAt"].toString();
                if (pub.isNotEmpty() && pub.substring (0, 10) > today) continue;     // hali chiqmagan
                if (s.title.isEmpty() || s.audio.isEmpty()) continue;
                r.add (s);
            }
        return r;
    }
    // manba: sayt (https://az-playlist.pages.dev/) yoki test uchun mahalliy papka
    inline juce::String& playlistBase() { static juce::String b ("https://az-playlist.pages.dev/"); return b; }
    inline bool fetchToFile (const juce::String& rel, const juce::File& dst, std::atomic<bool>* cancel = nullptr)
    {
        const auto base = playlistBase();
        if (! base.startsWithIgnoreCase ("http"))
        {
            const auto src = juce::File (base).getChildFile (rel);
            return src.existsAsFile() && (src == dst || src.copyFileTo (dst));
        }
        juce::URL url (base + rel);
        auto in = url.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress).withConnectionTimeoutMs (15000).withNumRedirectsToFollow (5));
        if (in == nullptr) return false;
        const auto tmp = dst.getSiblingFile (dst.getFileName() + ".part"); tmp.deleteFile();
        {
            juce::FileOutputStream os (tmp); if (! os.openedOk()) return false;
            char buf[16384];
            for (;;)
            {
                if (cancel && cancel->load()) return false;
                const int n = in->read (buf, sizeof (buf));
                if (n <= 0) break;
                os.write (buf, (size_t) n);
            }
        }
        return tmp.getSize() > 0 && tmp.moveFileTo (dst);
    }
    // mp3/ogg va h.k.ni loyiha uchun WAV'ga o'girish (TE har doim WAV'ni ishonchli o'qiydi)
    inline juce::File ensureWav (const juce::File& src, const juce::String& baseName)
    {
        if (src.hasFileExtension ("wav;aif;aiff;flac")) return src;
        const auto dir = AppEngine::projectsFolder().getChildFile ("Audio"); dir.createDirectory();
        const auto wav = dir.getChildFile (juce::File::createLegalFileName (baseName) + ".wav");
        if (wav.existsAsFile() && wav.getSize() > 1000) return wav;
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (src));
        if (r == nullptr) return {};
        juce::WavAudioFormat wf;
        std::unique_ptr<juce::AudioFormatWriter> w (wf.createWriterFor (new juce::FileOutputStream (wav), r->sampleRate, juce::jmax (1u, r->numChannels), 24, {}, 0));
        if (w == nullptr || ! w->writeFromAudioReader (*r, 0, r->lengthInSamples)) { w.reset(); wav.deleteFile(); return {}; }
        return wav;
    }
    inline juce::String playlistUrl() { return playlistBase().startsWithIgnoreCase ("http") ? playlistBase() : juce::String ("https://az-playlist.pages.dev/"); }
}

// =============================================================================  Bosh sahifa
class StartHub : public juce::Component, private juce::Timer, private juce::ChangeListener
{
public:
    // tashqi (MainWindow) amallari
    std::function<void (juce::File file, int templ, double bpm, juce::String key)> onCreate;
    std::function<void (juce::File)> onOpen;
    std::function<void (const hub::Song&, juce::File wav)> onCreateFromSong;
    std::function<void (juce::File audio)> onCreateFromAudio;

    enum Page { home = 0, templatesPage, playlistPage, trashPage };

    explicit StartHub (AppEngine& a) : app (a), pf (a.engine.getPropertyStorage().getPropertiesFile())
    {
        setWantsKeyboardFocus (true);
        formats.registerBasicFormats();
        readAhead.startThread (juce::Thread::Priority::normal);
        transport.addChangeListener (this);
        player.setSource (&transport);
        app.engine.getDeviceManager().deviceManager.addAudioCallback (&player);

        // profil
        for (auto* l : { &nameL, &roleL })
        {
            addAndMakeVisible (l); l->setEditable (false, true, false);
            l->setColour (juce::Label::textColourId, l == &nameL ? text : dim);
            l->setColour (juce::Label::textWhenEditingColourId, text);
            l->setColour (juce::Label::backgroundWhenEditingColourId, bg3);
            l->setColour (juce::Label::outlineWhenEditingColourId, red.withAlpha (0.6f));
            l->setFont (font (l == &nameL ? 14.0f : 11.5f, l == &nameL ? 600 : 400));
            l->setMinimumHorizontalScale (0.7f);
            l->setTooltip (U ("Ikki marta bosib o'zgartiring"));
        }
        nameL.setText (pf.getValue ("azProfileName", "Azimxodjayev Music"), juce::dontSendNotification);
        roleL.setText (pf.getValue ("azProfileRole", isGroup() ? U ("Ijodiy guruh") : U ("Aranjirovshik \xc2\xb7 Prodyuser")), juce::dontSendNotification);
        nameL.onTextChange = [this] { pf.setValue ("azProfileName", nameL.getText()); };
        roleL.onTextChange = [this] { pf.setValue ("azProfileRole", roleL.getText()); };
        loadAvatar();

        addAndMakeVisible (swapB); swapB.setButtonText (juce::String::fromUTF8 ("\xe2\x87\x84"));
        swapB.setTooltip (U ("Artist / Ijodiy guruh"));
        swapB.onClick = [this]
        {
            pf.setValue ("azProfileGroup", ! isGroup());
            const auto r = roleL.getText();
            if (r == U ("Ijodiy guruh") || r == U ("Aranjirovshik \xc2\xb7 Prodyuser") || r.isEmpty())
            { roleL.setText (isGroup() ? U ("Ijodiy guruh") : U ("Aranjirovshik \xc2\xb7 Prodyuser"), juce::dontSendNotification); pf.setValue ("azProfileRole", roleL.getText()); }
            repaint();
        };
        addAndMakeVisible (photoB); photoB.setButtonText (U ("Rasm qo'yish"));
        photoB.getProperties().set ("radius", 7.0f);
        photoB.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffb98aff).withAlpha (0.28f));
        photoB.onClick = [this] { chooseAvatar(); };

        // navigatsiya
        const char* nav[] = { "Bosh sahifa", "Shablonlar", "AZ Playlist", "Savat" };
        for (int i = 0; i < 4; ++i)
        {
            auto* b = navB.add (new juce::TextButton (U (nav[i]))); addAndMakeVisible (b);
            b->setClickingTogglesState (true); b->setRadioGroupId (901); b->getProperties().set ("radius", 8.0f);
            b->setColour (juce::TextButton::buttonOnColourId, juce::Colours::white.withAlpha (0.10f));
            b->setColour (juce::TextButton::textColourOnId, text);
            b->setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
            b->setColour (juce::TextButton::textColourOffId, silver);
            b->onClick = [this, i] { if (navB[i]->getToggleState()) setPage ((Page) i); };
        }
        navB[0]->setToggleState (true, juce::dontSendNotification);
        const char* tools[] = { "AZ BEAT bilan beat", "Audio fayldan loyiha", "Loyihani ochish..." };
        for (int i = 0; i < 3; ++i)
        {
            auto* b = toolB.add (new juce::TextButton (U (tools[i]))); addAndMakeVisible (b);
            b->getProperties().set ("radius", 8.0f);
            b->setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        }
        toolB[0]->onClick = [this] { showNewDialog (3); };
        toolB[1]->onClick = [this] { chooseAudioForProject(); };
        toolB[2]->onClick = [this] { chooseProjectFile(); };

        // loyihalar paneli
        addAndMakeVisible (searchE); searchE.setTextToShowWhenEmpty (U ("Qidirish..."), dim2);
        searchE.setFont (font (12)); searchE.setColour (juce::TextEditor::backgroundColourId, bg2);
        searchE.setColour (juce::TextEditor::outlineColourId, line); searchE.setColour (juce::TextEditor::focusedOutlineColourId, red.withAlpha (0.5f));
        searchE.setIndents (10, 5);
        searchE.onTextChange = [this] { scroll = 0; layoutCards(); repaint(); };
        addAndMakeVisible (sortBox); sortBox.addItem (U ("Oxirgi o'zgartirilgan"), 1); sortBox.addItem (U ("Nomi bo'yicha"), 2); sortBox.addItem (U ("Hajmi bo'yicha"), 3);
        sortBox.setSelectedId (1, juce::dontSendNotification); sortBox.onChange = [this] { sortProjects(); layoutCards(); repaint(); };
        addAndMakeVisible (trashTopB); trashTopB.setButtonText (U ("Savat")); trashTopB.onClick = [this] { navB[trashPage]->setToggleState (true, juce::sendNotification); };
        addAndMakeVisible (emptyTrashB); emptyTrashB.setButtonText (U ("Savatni tozalash")); emptyTrashB.onClick = [this] { emptyTrash(); };
        addAndMakeVisible (siteB); siteB.setButtonText (U ("Saytda ochish")); siteB.onClick = [] { juce::URL (hub::playlistUrl()).launchInDefaultBrowser(); };
        addAndMakeVisible (reloadB); reloadB.setButtonText (U ("Yangilash")); reloadB.onClick = [this] { loadPlaylist (true); };
        for (auto* b : { &trashTopB, &emptyTrashB, &siteB, &reloadB, &swapB }) b->getProperties().set ("radius", 7.0f);

        // pleyer paneli
        for (auto* b : { &playB, &songProjB, &stopB })
        { addAndMakeVisible (b); b->getProperties().set ("radius", 8.0f); }
        playB.setButtonText (juce::String::fromUTF8 ("\xe2\x96\xb6")); stopB.setButtonText (juce::String::fromUTF8 ("\xe2\x96\xa0"));
        songProjB.setButtonText (U ("Shu qo'shiqdan loyiha"));
        songProjB.setTooltip (U ("Yangi loyiha: qo'shiq BPM va tonalligi bilan, qo'shiq \"Referens\" trekka joylanadi"));
        songProjB.setColour (juce::TextButton::buttonColourId, red.withAlpha (0.35f));
        playB.onClick = [this] { if (transport.isPlaying()) transport.stop(); else if (curSong >= 0) { if (transport.getLengthInSeconds() > 0) transport.start(); else playSong (curSong); } repaint(); };
        stopB.onClick = [this] { transport.stop(); transport.setPosition (0); repaint(); };
        songProjB.onClick = [this] { createFromCurrentSong(); };
        addChildComponent (vol); vol.setSliderStyle (juce::Slider::LinearHorizontal); vol.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        vol.setRange (0.0, 1.0); vol.setValue (pf.getDoubleValue ("azHubVol", 0.8), juce::dontSendNotification);
        vol.onValueChange = [this] { transport.setGain ((float) vol.getValue()); pf.setValue ("azHubVol", vol.getValue()); };
        transport.setGain ((float) vol.getValue());

        // yangi loyiha oynasi
        addChildComponent (dlg);
        dlg.create.onClick = [this] { createFromDialog(); };
        dlg.cancel.onClick = [this] { dlg.setVisible (false); };
        dlg.nameE.onReturnKey = [this] { createFromDialog(); };
        dlg.nameE.onEscapeKey = [this] { dlg.setVisible (false); };

        refreshProjects();
        setPage (home);
        startTimerHz (15);
        setSize (1680, 1000);
    }
    ~StartHub() override
    {
        cancel = true;
        for (auto& t : threads) if (t.joinable()) t.join();
        transport.setSource (nullptr); player.setSource (nullptr); readerSrc.reset();
        readAhead.stopThread (2000);
        app.engine.getDeviceManager().deviceManager.removeAudioCallback (&player);
        transport.removeChangeListener (this);
    }

    // ------------------------------------------------------------ ochiq API (test va MainWindow uchun)
    Page page = home;
    void setPage (Page p)
    {
        page = p; scroll = 0; hover = -1;
        if (! navB[p]->getToggleState()) navB[p]->setToggleState (true, juce::dontSendNotification);
        if (p == home || p == trashPage) refreshProjects();
        if (p == playlistPage && songs.isEmpty()) loadPlaylist (false);
        resized(); repaint();
    }
    void refreshProjects()
    {
        projects.clear();
        if (page == trashPage)
        {
            for (const auto& e : juce::RangedDirectoryIterator (hub::trashDir(), false, "*.azmus", juce::File::findFiles))
                projects.add (hub::readProject (e.getFile()));
        }
        else for (auto& f : hub::listProjects (&pf)) projects.add (hub::readProject (f));
        sortProjects(); layoutCards();
    }
    int numVisibleProjects() const { return filtered.size(); }
    const juce::Array<hub::Song>& getSongs() const { return songs; }
    void showNewDialog (int templ = 0)
    {
        dlg.setTemplate (juce::jlimit (0, hub::numTemplates - 1, templ));
        dlg.nameE.setText (AppEngine::projectsFolder().getNonexistentChildFile (U ("Yangi qo'shiq"), ".azmus", false).getFileNameWithoutExtension(), juce::dontSendNotification);
        dlg.setVisible (true); dlg.toFront (true); resized();
        dlg.nameE.grabKeyboardFocus(); dlg.nameE.selectAll();
    }
    bool isNewDialogVisible() const { return dlg.isVisible(); }
    void loadPlaylist (bool force)
    {
        const auto cached = hub::playlistCache().getChildFile ("playlist.json");
        if (cached.existsAsFile() && ! force) applyPlaylist (cached.loadFileAsString());
        if (loading) return;
        loading = true; netError = false; repaint();
        launch ([this, cached]
        {
            const auto tmp = hub::playlistCache().getChildFile ("playlist.new.json");
            const bool ok = hub::fetchToFile ("playlist.json", tmp, &cancel) && hub::parsePlaylist (tmp.loadFileAsString()).size() > 0;
            if (ok) tmp.moveFileTo (cached);
            juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<StartHub> (this), ok, cached]
            {
                if (sp == nullptr) return;
                sp->loading = false; sp->netError = ! ok;
                if (cached.existsAsFile()) sp->applyPlaylist (cached.loadFileAsString());
                sp->repaint();
            });
        });
    }
    void applyPlaylist (const juce::String& json)
    {
        auto list = hub::parsePlaylist (json, &artist);
        // muqovalar keshdan
        for (auto& s : list)
        {
            for (auto& o : songs) if (o.id == s.id && o.coverImg.isValid()) s.coverImg = o.coverImg;
            const auto cf = coverCacheFile (s);
            if (! s.coverImg.isValid() && cf.existsAsFile()) s.coverImg = juce::ImageFileFormat::loadFrom (cf);
        }
        const auto playingId = curSong >= 0 && curSong < songs.size() ? songs[curSong].id : juce::String();
        songs = list; curSong = -1;
        for (int i = 0; i < songs.size(); ++i) if (songs[i].id == playingId) curSong = i;
        fetchMissingCovers();
        layoutCards(); repaint();
    }
    void playSong (int i)
    {
        if (i < 0 || i >= songs.size()) return;
        curSong = i; transport.stop(); transport.setSource (nullptr); readerSrc.reset();
        const auto f = audioCacheFile (songs[i]);
        if (f.existsAsFile()) { startFile (f); return; }
        downloading = songs[i].id; repaint();
        const auto rel = songs[i].audio; const auto id = songs[i].id;
        launch ([this, rel, f, id]
        {
            const bool ok = hub::fetchToFile (rel, f, &cancel);
            juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<StartHub> (this), ok, f, id]
            {
                if (sp == nullptr) return;
                if (sp->downloading == id) sp->downloading = {};
                if (! ok) { sp->status = U ("Yuklab bo'lmadi \xe2\x80\x94 internetni tekshiring"); sp->repaint(); return; }
                if (sp->curSong >= 0 && sp->songs[sp->curSong].id == id) sp->startFile (f);
            });
        });
    }
    bool isPlaying() const { return transport.isPlaying(); }
    void createFromSongForTest (int i) { curSong = i; createFromCurrentSong(); }

private:
    // ------------------------------------------------------------ yangi loyiha oynasi
    struct NewDialog : public juce::Component
    {
        juce::TextEditor nameE, bpmE; juce::ComboBox keyBox; juce::TextButton create { U ("Yaratish") }, cancel { U ("Bekor qilish") };
        int templ = 0; std::function<void()> onTemplate;
        NewDialog()
        {
            for (auto* e : { &nameE, &bpmE })
            {
                addAndMakeVisible (e); e->setFont (font (13)); e->setIndents (10, 7);
                e->setColour (juce::TextEditor::backgroundColourId, bg1); e->setColour (juce::TextEditor::outlineColourId, line);
                e->setColour (juce::TextEditor::focusedOutlineColourId, red.withAlpha (0.6f));
            }
            bpmE.setInputRestrictions (6, "0123456789.");
            addAndMakeVisible (keyBox);
            const char* notes[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
            int id = 1; for (auto* n : notes) keyBox.addItem (juce::String (n) + " minor", id++);
            for (auto* n : notes) keyBox.addItem (juce::String (n) + " major", id++);
            keyBox.setSelectedId (10, juce::dontSendNotification);
            addAndMakeVisible (create); addAndMakeVisible (cancel);
            create.getProperties().set ("radius", 8.0f); cancel.getProperties().set ("radius", 8.0f);
            create.setColour (juce::TextButton::buttonColourId, red.withAlpha (0.85f));
        }
        void setTemplate (int t) { templ = t; bpmE.setText (juce::String (juce::roundToInt (hub::templates[t].bpm))); repaint(); }
        juce::Rectangle<float> tplRect (int i) const
        {
            const float w = (getWidth() - 48 - 2 * 10) / 3.0f;
            return { 24 + (i % 3) * (w + 10), 200 + (i / 3) * 62.0f, w, 54 };
        }
        void paint (juce::Graphics& g) override
        {
            using namespace juce;
            auto r = getLocalBounds().toFloat();
            g.setColour (Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (r.translated (0, 6), 16);
            g.setColour (bg2); g.fillRoundedRectangle (r, 16);
            g.setColour (Colours::white.withAlpha (0.08f)); g.drawRoundedRectangle (r.reduced (0.5f), 16, 1);
            label (g, U ("Yangi loyiha"), { 24, 18, r.getWidth() - 48, 28 }, 19, text, Justification::centredLeft, 700);
            caps (g, U ("NOMI"), { 24, 58, 200, 12 });
            caps (g, U ("TEMP (BPM)"), { 24, 122, 120, 12 });
            caps (g, U ("TONALLIK"), { 164, 122, 160, 12 });
            caps (g, U ("SHABLON"), { 24, 184, 200, 12 });
            for (int i = 0; i < hub::numTemplates; ++i)
            {
                auto t = tplRect (i); const bool on = i == templ; const Colour c (hub::templates[i].colour);
                g.setColour (on ? c.withAlpha (0.18f) : Colours::white.withAlpha (0.03f)); g.fillRoundedRectangle (t, 9);
                g.setColour (on ? c : Colours::white.withAlpha (0.07f)); g.drawRoundedRectangle (t.reduced (0.5f), 9, on ? 1.5f : 1.0f);
                g.setColour (c); g.fillEllipse (t.getX() + 12, t.getY() + 13, 8, 8);
                label (g, U (hub::templates[i].name), { t.getX() + 26, t.getY() + 8, t.getWidth() - 32, 18 }, 12, text, Justification::centredLeft, 600);
                label (g, U (hub::templates[i].desc), { t.getX() + 12, t.getY() + 28, t.getWidth() - 18, 18 }, 9.5f, dim, Justification::centredLeft, 400);
            }
            label (g, U ("Saqlanadi: Hujjatlar / AZ MUS Projects"), { 24, r.getBottom() - 46, 300, 24 }, 10, dim2, Justification::centredLeft, 400);
        }
        void resized() override
        {
            nameE.setBounds (24, 74, getWidth() - 48, 34);
            bpmE.setBounds (24, 138, 120, 32); keyBox.setBounds (164, 138, 150, 32);
            create.setBounds (getWidth() - 24 - 130, getHeight() - 52, 130, 34);
            cancel.setBounds (getWidth() - 24 - 130 - 10 - 120, getHeight() - 52, 120, 34);
        }
        void mouseDown (const juce::MouseEvent& e) override
        {
            for (int i = 0; i < hub::numTemplates; ++i) if (tplRect (i).contains (e.position)) { setTemplate (i); return; }
        }
    } dlg;

    // ------------------------------------------------------------ joylashuv
    static constexpr int sideW = 256;
    juce::Rectangle<float> mainArea() const { return getLocalBounds().toFloat().withTrimmedLeft ((float) sideW).reduced (36, 0); }
    juce::Rectangle<float> bannerRect() const { auto m = mainArea(); return { m.getX(), 34, m.getWidth(), 92 }; }
    juce::Rectangle<float> listArea() const
    {
        auto m = mainArea();
        const float top = page == home ? 196.0f : page == playlistPage ? 196.0f : 120.0f;
        const float bottom = (float) getHeight() - (page == playlistPage ? 96.0f : 16.0f);
        return { m.getX(), top, m.getWidth(), bottom - top };
    }
    juce::Rectangle<float> promoRect() const { return { 18, (float) getHeight() - 132, (float) sideW - 36, 112 }; }
    juce::Rectangle<float> avatarRect() const { return { 30, 30, 44, 44 }; }

    void resized() override
    {
        nameL.setBounds (80, 28, sideW - 80 - 52, 22);
        roleL.setBounds (80, 50, sideW - 80 - 30, 18);
        swapB.setBounds (sideW - 54, 32, 28, 28);
        photoB.setBounds (30, 110, sideW - 60, 30);
        int y = 176;
        for (auto* b : navB) { b->setBounds (18, y, sideW - 36, 36); y += 42; }
        y += 34;
        for (auto* b : toolB) { b->setBounds (18, y, sideW - 36, 32); y += 36; }

        auto m = mainArea();
        const bool home_ = page == home, trash_ = page == trashPage, pl_ = page == playlistPage;
        searchE.setVisible (home_ || trash_); sortBox.setVisible (home_); trashTopB.setVisible (home_);
        emptyTrashB.setVisible (trash_); siteB.setVisible (pl_); reloadB.setVisible (pl_);
        for (auto* b : { &playB, &stopB, &songProjB }) b->setVisible (pl_);
        vol.setVisible (pl_);
        const float hy = home_ ? 150.0f : 72.0f;
        searchE.setBounds ((int) m.getRight() - 470, (int) hy, 210, 28);
        sortBox.setBounds ((int) m.getRight() - 250, (int) hy, 160, 28);
        trashTopB.setBounds ((int) m.getRight() - 80, (int) hy, 80, 28);
        emptyTrashB.setBounds ((int) m.getRight() - 140, (int) hy, 140, 28);
        searchE.setBounds (trash_ ? juce::Rectangle<int> ((int) m.getRight() - 360, (int) hy, 210, 28) : searchE.getBounds());
        siteB.setBounds ((int) m.getRight() - 120, 150, 120, 28);
        reloadB.setBounds ((int) m.getRight() - 230, 150, 100, 28);
        // pleyer
        const int py = getHeight() - 74;
        stopB.setBounds ((int) m.getX() + 64, py + 18, 36, 36);
        playB.setBounds ((int) m.getX() + 18, py + 14, 44, 44);
        songProjB.setBounds ((int) m.getRight() - 190, py + 20, 176, 32);
        vol.setBounds ((int) m.getRight() - 300, py + 26, 96, 20);
        if (dlg.isVisible()) dlg.setBounds (getLocalBounds().withSizeKeepingCentre (560, 400));
        layoutCards();
    }

    // ------------------------------------------------------------ kartalar
    juce::Array<int> filtered; juce::Array<juce::Rectangle<float>> cards; float contentH = 0;
    void sortProjects()
    {
        const int mode = sortBox.getSelectedId();
        std::sort (projects.begin(), projects.end(), [mode] (const hub::Project& a, const hub::Project& b)
        {
            if (mode == 2) return a.name.compareNatural (b.name) < 0;
            if (mode == 3) return a.bytes > b.bytes;
            return a.modified > b.modified;
        });
    }
    void layoutCards()
    {
        cards.clear(); filtered.clear();
        auto la = listArea();
        if (page == templatesPage)
        {
            const int cols = juce::jmax (1, (int) ((la.getWidth() + 18) / 300));
            const float w = (la.getWidth() - (cols - 1) * 18.0f) / cols, h = 150;
            for (int i = 0; i < hub::numTemplates; ++i)
            { filtered.add (i); cards.add ({ la.getX() + (i % cols) * (w + 18), la.getY() + (i / cols) * (h + 18), w, h }); }
            contentH = ((hub::numTemplates + cols - 1) / cols) * (h + 18);
            return;
        }
        const float cw = page == playlistPage ? 168.0f : 150.0f, ch = page == playlistPage ? 232.0f : 178.0f, gap = 20;
        const int cols = juce::jmax (1, (int) ((la.getWidth() + gap) / (cw + gap)));
        const float w = (la.getWidth() - (cols - 1) * gap) / cols;
        const float imgH = page == playlistPage ? w : w * 0.82f;
        const float h = imgH + (page == playlistPage ? 64.0f : 46.0f);
        int n = 0;
        if (page == playlistPage) { for (int i = 0; i < songs.size(); ++i) filtered.add (i); }
        else
        {
            const auto q = searchE.getText().trim();
            for (int i = 0; i < projects.size(); ++i) if (q.isEmpty() || projects[i].name.containsIgnoreCase (q)) filtered.add (i);
        }
        for (int k = 0; k < filtered.size(); ++k, ++n)
            cards.add ({ la.getX() + (n % cols) * (w + gap), la.getY() + (n / cols) * (h + gap) - scroll, w, h });
        contentH = ((filtered.size() + cols - 1) / cols) * (h + gap);
        juce::ignoreUnused (cw, ch);
    }
    int cardAt (juce::Point<float> p) const
    {
        if (! listArea().contains (p)) return -1;
        for (int i = 0; i < cards.size(); ++i) if (cards[i].contains (p)) return i;
        return -1;
    }

    // ------------------------------------------------------------ chizish
    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        g.fillAll (Colour (0xff0b0a0e));
        // yon panel
        auto side = getLocalBounds().toFloat().withWidth ((float) sideW);
        g.setColour (Colour (0xff100f13)); g.fillRect (side);
        g.setColour (line); g.drawVerticalLine (sideW - 1, 0, (float) getHeight());
        // profil kartasi
        auto pc = Rectangle<float> (18, 18, (float) sideW - 36, 134);
        panel (g, pc, 12, 0.035f, 0.07f);
        auto av = avatarRect();
        if (avatar.isValid()) { hub::drawCover (g, avatar, av, av.getWidth() * 0.5f); }
        else
        {
            g.setGradientFill (ColourGradient (Colour (0xffff5a3a), av.getX(), av.getY(), Colour (0xffb98aff), av.getRight(), av.getBottom(), false));
            g.fillEllipse (av);
            label (g, nameL.getText().substring (0, 1).toUpperCase(), av, 20, Colours::white, Justification::centred, 700);
        }
        g.setColour (Colours::white.withAlpha (0.15f)); g.drawEllipse (av, 1.0f);
        const bool grp = isGroup();
        auto badge = Rectangle<float> (30, 84, grp ? 84.0f : 64.0f, 18);
        g.setColour (Colour (0xffb98aff).withAlpha (0.16f)); g.fillRoundedRectangle (badge, 9);
        label (g, grp ? U ("GURUH") : U ("ARTIST"), badge, 9, Colour (0xffcdb0ff), Justification::centred, 700);
        label (g, "AZ MUS " + JUCEApplication::getInstance()->getApplicationVersion(), { badge.getRight() + 8, 84, 120, 18 }, 9.5f, dim2, Justification::centredLeft, 500);
        caps (g, U ("VOSITALAR"), { 30, (float) toolB[0]->getY() - 22, 160, 12 });
        // promo: AZ MUSIC Playlist
        auto pr = promoRect();
        g.setGradientFill (ColourGradient (Colour (0xff3a0d1a), pr.getX(), pr.getY(), Colour (0xff1a1236), pr.getRight(), pr.getBottom(), false));
        g.fillRoundedRectangle (pr, 12);
        g.setColour (red.withAlpha (hoverPromo ? 0.7f : 0.35f)); g.drawRoundedRectangle (pr.reduced (0.5f), 12, 1.2f);
        // tovush to'lqini bezagi
        {
            Path w; const float y0 = pr.getBottom() - 30;
            for (int i = 0; i < 46; ++i)
            {
                const float x = pr.getX() + 14 + i * ((pr.getWidth() - 28) / 46.0f);
                const float a = 4 + 12 * std::abs (std::sin (i * 0.55f + (float) phase)) * (0.4f + 0.6f * std::abs (std::sin (i * 0.17f)));
                w.addRoundedRectangle (x, y0 - a, 2.4f, a * 2, 1.2f);
            }
            g.setColour (red.withAlpha (0.55f)); g.fillPath (w);
        }
        label (g, "AZ MUSIC Playlist", { pr.getX() + 14, pr.getY() + 12, pr.getWidth() - 28, 20 }, 14, text, Justification::centredLeft, 700);
        label (g, U ("Yangi qo'shiqlarni DAW ichida tinglang \xe2\x86\x92"), { pr.getX() + 14, pr.getY() + 32, pr.getWidth() - 28, 16 }, 10, silver, Justification::centredLeft, 400);

        auto m = mainArea();
        if (page == home)
        {
            auto b = bannerRect();
            g.setGradientFill (ColourGradient (Colour (0xffff4a5a), b.getX(), b.getY(), Colour (0xff7a2cff), b.getRight(), b.getBottom(), false));
            g.fillRoundedRectangle (b, 14);
            if (hoverBanner) { g.setColour (Colours::white.withAlpha (0.08f)); g.fillRoundedRectangle (b, 14); }
            // fon: tovush to'lqini
            {
                Path w; const float cy = b.getCentreY();
                for (int i = 0; i < 120; ++i)
                {
                    const float x = b.getX() + 10 + i * ((b.getWidth() - 20) / 120.0f);
                    const float a = 3 + 26 * std::abs (std::sin (i * 0.21f)) * std::abs (std::sin (i * 0.047f + 0.6f));
                    w.addRoundedRectangle (x, cy - a, 3.0f, a * 2, 1.5f);
                }
                g.setColour (Colours::white.withAlpha (0.10f)); g.fillPath (w);
            }
            auto plus = Rectangle<float> (b.getCentreX() - 118, b.getCentreY() - 12, 24, 24);
            g.setColour (Colour (0xff141218)); g.fillRoundedRectangle (plus, 5);
            g.setColour (Colours::white); g.fillRect (plus.getCentreX() - 6, plus.getCentreY() - 1, 12.0f, 2.0f); g.fillRect (plus.getCentreX() - 1, plus.getCentreY() - 6, 2.0f, 12.0f);
            label (g, U ("Yangi loyiha"), { plus.getRight() + 14, b.getCentreY() - 14, 260, 28 }, 20, Colour (0xff141218), Justification::centredLeft, 700);
            label (g, U ("Ctrl+N"), b.reduced (18, 10), 10, Colour (0xff141218).withAlpha (0.6f), Justification::bottomRight, 600);
            label (g, U ("Loyihalar"), { m.getX(), 150, 200, 28 }, 15, silver, Justification::centredLeft, 600);
            paintProjects (g);
        }
        else if (page == trashPage)
        {
            label (g, U ("Savat"), { m.getX(), 30, 300, 30 }, 22, text, Justification::centredLeft, 700);
            label (g, U ("O'chirilgan loyihalar shu yerda turadi \xe2\x80\x94 o'ng tugma: tiklash yoki butunlay o'chirish"), { m.getX(), 72, 600, 28 }, 11, dim, Justification::centredLeft, 400);
            paintProjects (g);
        }
        else if (page == templatesPage)
        {
            label (g, U ("Shablonlar"), { m.getX(), 30, 300, 30 }, 22, text, Justification::centredLeft, 700);
            label (g, U ("Aranjirovshik va prodyuserlar uchun tayyor treklar to'plami \xe2\x80\x94 bosing, nom bering, ishlang"), { m.getX(), 72, 700, 28 }, 11, dim, Justification::centredLeft, 400);
            paintTemplates (g);
        }
        else paintPlaylist (g);

        if (status.isNotEmpty())
            label (g, status, { m.getX(), (float) getHeight() - (page == playlistPage ? 100.0f : 24.0f), m.getWidth(), 18 }, 10.5f, yellow, Justification::centredLeft, 500);
        if (dlg.isVisible()) { g.setColour (Colours::black.withAlpha (0.55f)); g.fillAll(); }
    }
    void paintEmptyThumb (juce::Graphics& g, juce::Rectangle<float> r, int seed)
    {
        using namespace juce;
        g.setColour (Colour (0xff141218)); g.fillRoundedRectangle (r, 8);
        Random rnd (seed);
        const Colour c = trackColours[seed % 8];
        const int rows = 4; const float rh = (r.getHeight() - 16) / rows;
        for (int i = 0; i < rows; ++i)
        {
            float x = r.getX() + 8 + rnd.nextFloat() * 20;
            while (x < r.getRight() - 20)
            {
                const float w = 18 + rnd.nextFloat() * 40;
                g.setColour ((i == 0 ? c : trackColours[(seed + i) % 8]).withAlpha (0.45f));
                g.fillRoundedRectangle (x, r.getY() + 8 + i * rh + 2, jmin (w, r.getRight() - 8 - x), rh - 4, 3);
                x += w + 6 + rnd.nextFloat() * 18;
            }
        }
    }
    void paintProjects (juce::Graphics& g)
    {
        using namespace juce;
        auto la = listArea();
        if (filtered.isEmpty())
        {
            label (g, page == trashPage ? U ("Savat bo'sh") : searchE.getText().isNotEmpty() ? U ("Hech narsa topilmadi") : U ("Hali loyiha yo'q \xe2\x80\x94 \"Yangi loyiha\" ni bosing"),
                   la.withHeight (80), 12, dim, Justification::centred, 500);
            return;
        }
        Graphics::ScopedSaveState ss (g); g.reduceClipRegion (la.expanded (4).toNearestInt());
        for (int k = 0; k < cards.size(); ++k)
        {
            auto c = cards[k]; if (! c.intersects (la)) continue;
            const auto& p = projects.getReference (filtered[k]);
            auto img = c.withHeight (c.getWidth() * 0.82f);
            if (p.thumb.isValid()) hub::drawCover (g, p.thumb, img, 9);
            else paintEmptyThumb (g, img, (int) (p.name.hashCode() & 0x7fffffff));
            if (k == hover) { g.setColour (Colours::white.withAlpha (0.09f)); g.fillRoundedRectangle (img, 9); g.setColour (red.withAlpha (0.8f)); g.drawRoundedRectangle (img.reduced (0.5f), 9, 1.5f); }
            if (p.bpm > 0)
            {
                auto t = Rectangle<float> (img.getX() + 6, img.getBottom() - 22, 0, 16);
                const String s = String (roundToInt (p.bpm)) + " BPM" + (p.key.isNotEmpty() ? "  " + p.key : String());
                t.setWidth (textWidth (font (9, 600), s) + 12);
                g.setColour (Colours::black.withAlpha (0.6f)); g.fillRoundedRectangle (t, 5);
                label (g, s, t, 9, Colours::white, Justification::centred, 600);
            }
            label (g, p.name, { c.getX(), img.getBottom() + 6, c.getWidth(), 18 }, 12, text, Justification::centredLeft, 600);
            String info = hub::sizeText (p.bytes) + "  |  " + (p.length > 0 ? hub::timeText (p.length) : p.modified.formatted ("%d.%m.%Y"));
            if (p.tracks > 0) info << "  |  " << p.tracks << U (" trek");
            label (g, info, { c.getX(), img.getBottom() + 24, c.getWidth(), 16 }, 10, dim, Justification::centredLeft, 400);
        }
    }
    void paintTemplates (juce::Graphics& g)
    {
        using namespace juce;
        for (int k = 0; k < cards.size(); ++k)
        {
            auto c = cards[k]; const auto& t = hub::templates[k]; const Colour col (t.colour);
            g.setGradientFill (ColourGradient (col.withAlpha (0.22f), c.getX(), c.getY(), Colour (0xff141218), c.getRight(), c.getBottom(), false));
            g.fillRoundedRectangle (c, 12);
            g.setColour (k == hover ? col : Colours::white.withAlpha (0.07f)); g.drawRoundedRectangle (c.reduced (0.5f), 12, k == hover ? 1.6f : 1.0f);
            // trek chiziqlari bezagi
            Random rnd (k * 31 + 7);
            for (int i = 0; i < 5; ++i)
            {
                float x = c.getX() + 16; const float y = c.getY() + 62 + i * 13;
                while (x < c.getRight() - 30)
                {
                    const float w = 14 + rnd.nextFloat() * 50;
                    g.setColour (trackColours[(k + i) % 8].withAlpha (0.35f));
                    g.fillRoundedRectangle (x, y, jmin (w, c.getRight() - 16 - x), 8, 2);
                    x += w + 5 + rnd.nextFloat() * 22;
                }
            }
            label (g, U (t.name), { c.getX() + 16, c.getY() + 14, c.getWidth() - 32, 22 }, 15, text, Justification::centredLeft, 700);
            label (g, U (t.desc), { c.getX() + 16, c.getY() + 36, c.getWidth() - 32, 16 }, 10.5f, silver, Justification::centredLeft, 400);
            label (g, String (roundToInt (t.bpm)) + " BPM", c.reduced (16, 12), 10, col, Justification::bottomRight, 700);
        }
    }
    void paintPlaylist (juce::Graphics& g)
    {
        using namespace juce;
        auto m = mainArea();
        // sarlavha bannari
        auto b = Rectangle<float> (m.getX(), 30, m.getWidth(), 100);
        g.setGradientFill (ColourGradient (Colour (0xff2a0a14), b.getX(), b.getY(), Colour (0xff12102a), b.getRight(), b.getBottom(), false));
        g.fillRoundedRectangle (b, 14);
        label (g, artist.isNotEmpty() ? artist : String ("AZIMXODJAYEV MUSIC"), { b.getX() + 24, b.getY() + 18, 600, 30 }, 22, text, Justification::centredLeft, 800, 0.04f);
        label (g, U ("AZ MUSIC Playlist \xc2\xb7 ") + String (songs.size()) + U (" ta qo'shiq \xc2\xb7 BPM va tonallik bilan \xe2\x80\x94 aranjirovka uchun tayyor"),
               { b.getX() + 24, b.getY() + 52, 700, 18 }, 11, silver, Justification::centredLeft, 400);
        label (g, hub::playlistUrl().fromFirstOccurrenceOf ("//", false, false).trimCharactersAtEnd ("/"), { b.getX() + 24, b.getY() + 72, 400, 16 }, 10, red, Justification::centredLeft, 600);
        label (g, U ("Qo'shiqlar"), { m.getX(), 150, 200, 28 }, 15, silver, Justification::centredLeft, 600);
        if (loading) label (g, U ("Yuklanmoqda\xe2\x80\xa6"), { m.getX() + 110, 150, 200, 28 }, 11, dim, Justification::centredLeft, 500);
        else if (netError) label (g, songs.isEmpty() ? U ("Internet yo'q \xe2\x80\x94 \"Yangilash\" ni bosing") : U ("Oflayn: oxirgi saqlangan ro'yxat"), { m.getX() + 110, 150, 360, 28 }, 11, yellow, Justification::centredLeft, 500);

        auto la = listArea();
        {
            Graphics::ScopedSaveState ss (g); g.reduceClipRegion (la.expanded (4).toNearestInt());
            for (int k = 0; k < cards.size(); ++k)
            {
                auto c = cards[k]; if (! c.intersects (la)) continue;
                const auto& s = songs.getReference (filtered[k]);
                auto img = c.withHeight (c.getWidth());
                if (s.coverImg.isValid()) hub::drawCover (g, s.coverImg, img, 10);
                else { g.setColour (Colour (0xff17151b)); g.fillRoundedRectangle (img, 10); label (g, s.title.substring (0, 1), img, 40, dim2, Justification::centred, 800); }
                const bool cur = filtered[k] == curSong;
                if (k == hover || cur)
                {
                    g.setColour (Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (img, 10);
                    auto pb = img.withSizeKeepingCentre (48, 48);
                    g.setColour (red); g.fillEllipse (pb);
                    if (downloading == s.id) label (g, U ("\xe2\x80\xa6"), pb, 18, Colours::white, Justification::centred, 800);
                    else if (cur && transport.isPlaying()) { g.setColour (Colours::white); g.fillRect (pb.getCentreX() - 7, pb.getCentreY() - 8, 5.0f, 16.0f); g.fillRect (pb.getCentreX() + 2, pb.getCentreY() - 8, 5.0f, 16.0f); }
                    else { Path tri; tri.addTriangle (pb.getCentreX() - 5, pb.getCentreY() - 9, pb.getCentreX() - 5, pb.getCentreY() + 9, pb.getCentreX() + 10, pb.getCentreY()); g.setColour (Colours::white); g.fillPath (tri); }
                    if (cur) { g.setColour (red); g.drawRoundedRectangle (img.reduced (0.5f), 10, 2.0f); }
                }
                if (s.status.isNotEmpty() && s.status != "open")
                {
                    auto bd = Rectangle<float> (img.getX() + 8, img.getY() + 8, 70, 18);
                    g.setColour (Colours::black.withAlpha (0.65f)); g.fillRoundedRectangle (bd, 6);
                    label (g, s.status == "sold" ? U ("SOTILGAN") : s.status.toUpperCase(), bd, 8.5f, yellow, Justification::centred, 700);
                }
                auto tag = Rectangle<float> (img.getRight() - 52, img.getBottom() - 24, 46, 18);
                g.setColour (Colours::black.withAlpha (0.6f)); g.fillRoundedRectangle (tag, 6);
                label (g, hub::timeText (s.duration), tag, 9, Colours::white, Justification::centred, 600);
                label (g, s.title, { c.getX(), img.getBottom() + 7, c.getWidth(), 18 }, 12.5f, cur ? red : text, Justification::centredLeft, 600);
                label (g, s.genre, { c.getX(), img.getBottom() + 25, c.getWidth(), 15 }, 10, dim, Justification::centredLeft, 400);
                label (g, String (roundToInt (s.bpm)) + " BPM" + String::fromUTF8 ("  \xc2\xb7  ") + s.key, { c.getX(), img.getBottom() + 41, c.getWidth(), 15 }, 10, silver, Justification::centredLeft, 600);
            }
        }
        // pleyer paneli
        auto pb = Rectangle<float> (m.getX(), (float) getHeight() - 74, m.getWidth(), 66);
        g.setColour (Colour (0xff141218)); g.fillRoundedRectangle (pb, 12);
        g.setColour (line); g.drawRoundedRectangle (pb.reduced (0.5f), 12, 1);
        if (curSong >= 0 && curSong < songs.size())
        {
            const auto& s = songs.getReference (curSong);
            auto ci = Rectangle<float> (pb.getX() + 112, pb.getY() + 9, 48, 48);
            if (s.coverImg.isValid()) hub::drawCover (g, s.coverImg, ci, 6);
            label (g, s.title, { ci.getRight() + 12, pb.getY() + 13, 220, 18 }, 13, text, Justification::centredLeft, 700);
            label (g, s.genre + String::fromUTF8 ("  \xc2\xb7  ") + String (roundToInt (s.bpm)) + " BPM" + String::fromUTF8 ("  \xc2\xb7  ") + s.key, { ci.getRight() + 12, pb.getY() + 33, 260, 16 }, 10, dim, Justification::centredLeft, 400);
            // to'lqin + progress
            auto wv = waveRect();
            const double len = transport.getLengthInSeconds() > 0 ? transport.getLengthInSeconds() : s.duration;
            const float prog = len > 0 ? (float) (transport.getCurrentPosition() / len) : 0.0f;
            const int nPk = s.peaks.size();
            const int bars = jmax (1, (int) (wv.getWidth() / 4));
            for (int i = 0; i < bars; ++i)
            {
                const float v = nPk > 0 ? s.peaks[jmin (nPk - 1, i * nPk / bars)] : 0.5f;
                const float h = jmax (2.0f, v * wv.getHeight());
                g.setColour ((float) i / bars < prog ? red : Colours::white.withAlpha (0.22f));
                g.fillRoundedRectangle (wv.getX() + i * 4, wv.getCentreY() - h * 0.5f, 2.5f, h, 1.2f);
            }
            label (g, hub::timeText (transport.getCurrentPosition()) + " / " + hub::timeText (len), { wv.getRight() + 10, wv.getY(), 90, wv.getHeight() }, 10, dim, Justification::centredLeft, 500);
            if (downloading == s.id) label (g, U ("Yuklanmoqda\xe2\x80\xa6"), wv, 10, silver, Justification::centred, 600);
        }
        else label (g, U ("Qo'shiqni tanlang \xe2\x80\x94 muqovani bosing"), pb.withTrimmedLeft (112), 11, dim, Justification::centredLeft, 500);
    }
    juce::Rectangle<float> waveRect() const
    {
        auto m = mainArea();
        return { m.getX() + 460, (float) getHeight() - 60, juce::jmax (100.0f, m.getWidth() - 460 - 420), 38 };
    }

    // ------------------------------------------------------------ sichqoncha
    void mouseMove (const juce::MouseEvent& e) override
    {
        const int h = dlg.isVisible() ? -1 : cardAt (e.position);
        const bool hb = page == home && bannerRect().contains (e.position), hp = promoRect().contains (e.position);
        if (h != hover || hb != hoverBanner || hp != hoverPromo) { hover = h; hoverBanner = hb; hoverPromo = hp; repaint(); }
        setMouseCursor (h >= 0 || hb || hp || avatarRect().contains (e.position) ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    }
    void mouseExit (const juce::MouseEvent&) override { hover = -1; hoverBanner = hoverPromo = false; repaint(); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (dlg.isVisible()) { dlg.setVisible (false); repaint(); return; }
        status = {};
        if (avatarRect().contains (e.position)) { chooseAvatar(); return; }
        if (promoRect().contains (e.position)) { navB[playlistPage]->setToggleState (true, juce::sendNotification); return; }
        if (page == home && bannerRect().contains (e.position)) { showNewDialog (0); return; }
        if (page == playlistPage && waveRect().contains (e.position) && curSong >= 0 && transport.getLengthInSeconds() > 0)
        { transport.setPosition ((e.position.x - waveRect().getX()) / waveRect().getWidth() * transport.getLengthInSeconds()); repaint(); return; }
        const int k = cardAt (e.position);
        if (k < 0) return;
        if (page == templatesPage) { showNewDialog (k); return; }
        if (page == playlistPage)
        {
            const int i = filtered[k];
            if (e.mods.isPopupMenu()) { songMenu (i); return; }
            if (i == curSong && transport.isPlaying()) transport.stop();
            else if (i == curSong && transport.getLengthInSeconds() > 0) transport.start();
            else playSong (i);
            repaint(); return;
        }
        const auto p = projects[filtered[k]];
        if (e.mods.isPopupMenu() || page == trashPage) { projectMenu (p); return; }
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        // bitta bosish bilan ochish (CapCut kabi): sichqoncha qo'yib yuborilganda, sudralmagan bo'lsa
        if (page != home || e.mods.isPopupMenu() || e.mouseWasDraggedSinceMouseDown() || e.getNumberOfClicks() > 1) return;
        const int k = cardAt (e.position);
        if (k >= 0) openLater (projects[filtered[k]].file);
    }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        const float maxS = juce::jmax (0.0f, contentH - listArea().getHeight());
        scroll = juce::jlimit (0.0f, maxS, scroll - w.deltaY * 220.0f);
        layoutCards(); repaint();
    }
    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress ('n', juce::ModifierKeys::commandModifier, 0)) { showNewDialog (0); return true; }
        if (k == juce::KeyPress ('o', juce::ModifierKeys::commandModifier, 0)) { chooseProjectFile(); return true; }
        if (k == juce::KeyPress::escapeKey && dlg.isVisible()) { dlg.setVisible (false); repaint(); return true; }
        if (k == juce::KeyPress::spaceKey && page == playlistPage) { playB.triggerClick(); return true; }
        return false;
    }

    // ------------------------------------------------------------ amallar
    void projectMenu (const hub::Project& p)
    {
        juce::PopupMenu m;
        if (page == trashPage) { m.addItem (1, U ("Tiklash")); m.addSeparator(); m.addItem (2, U ("Butunlay o'chirish")); }
        else
        {
            m.addItem (10, U ("Ochish")); m.addSeparator();
            m.addItem (11, U ("Muqova rasmini qo'yish...")); if (p.customCover) m.addItem (12, U ("Muqovani olib tashlash"));
            m.addItem (13, U ("Nomini o'zgartirish...")); m.addItem (14, U ("Nusxa olish"));
            m.addItem (15, U ("Papkada ko'rsatish")); m.addSeparator(); m.addItem (16, U ("Savatga tashlash"));
        }
        m.showMenuAsync ({}, [this, p] (int r)
        {
            switch (r)
            {
                case 1: hub::restoreFromTrash (p.file); break;
                case 2: hub::thumbFile (p.file).deleteFile(); hub::coverFile (p.file).deleteFile(); hub::metaFile (p.file).deleteFile(); p.file.deleteFile(); break;
                case 10: openLater (p.file); return;
                case 11: chooseCover (p.file); return;
                case 12: hub::coverFile (p.file).deleteFile(); break;
                case 13: renameProject (p); return;
                case 14:
                {
                    const auto dst = p.file.getParentDirectory().getNonexistentChildFile (p.name + U (" (nusxa)"), ".azmus", false);
                    if (p.file.copyFileTo (dst)) { hub::thumbFile (p.file).copyFileTo (hub::thumbFile (dst)); hub::coverFile (p.file).copyFileTo (hub::coverFile (dst)); hub::metaFile (p.file).copyFileTo (hub::metaFile (dst)); }
                    break;
                }
                case 15: p.file.revealToUser(); return;
                case 16: hub::moveToTrash (p.file); status = U ("Savatga tashlandi: ") + p.name; break;
                default: return;
            }
            refreshProjects(); repaint();
        });
    }
    void renameProject (const hub::Project& p)
    {
        auto* aw = new juce::AlertWindow (U ("Nomini o'zgartirish"), {}, juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor ("n", p.name); aw->addButton (U ("Saqlash"), 1, juce::KeyPress (juce::KeyPress::returnKey)); aw->addButton (U ("Bekor"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw, p] (int r)
        {
            if (r == 1)
            {
                const auto dst = p.file.getSiblingFile (hub::sanitizeName (aw->getTextEditorContents ("n")) + ".azmus");
                if (dst != p.file && ! dst.exists() && p.file.moveFileTo (dst))
                { hub::thumbFile (p.file).moveFileTo (hub::thumbFile (dst)); hub::coverFile (p.file).moveFileTo (hub::coverFile (dst)); hub::metaFile (p.file).moveFileTo (hub::metaFile (dst)); }
            }
            refreshProjects(); repaint();
        }), true);
    }
    void songMenu (int i)
    {
        juce::PopupMenu m;
        m.addItem (1, U ("Tinglash")); m.addItem (2, U ("Shu qo'shiqdan loyiha (BPM / tonallik)")); m.addItem (3, U ("Saytda ochish"));
        m.showMenuAsync ({}, [this, i] (int r)
        {
            if (r == 1) playSong (i);
            if (r == 2) { curSong = i; createFromCurrentSong(); }
            if (r == 3) juce::URL (hub::playlistUrl() + "?t=" + songs[i].id).launchInDefaultBrowser();
        });
    }
    void createFromDialog()
    {
        const auto name = hub::sanitizeName (dlg.nameE.getText());
        const auto f = AppEngine::projectsFolder().getNonexistentChildFile (name, ".azmus", false);
        const double bpm = juce::jlimit (20.0, 400.0, dlg.bpmE.getText().getDoubleValue() > 0 ? dlg.bpmE.getText().getDoubleValue() : 120.0);
        const auto key = dlg.keyBox.getText(); const int t = dlg.templ;
        dlg.setVisible (false);
        transport.stop();
        juce::MessageManager::callAsync ([fn = onCreate, f, t, bpm, key] { if (fn) fn (f, t, bpm, key); });
    }
    void createFromCurrentSong()
    {
        if (curSong < 0 || curSong >= songs.size()) { status = U ("Avval qo'shiqni tanlang"); repaint(); return; }
        const auto s = songs[curSong];
        const auto mp3 = audioCacheFile (s);
        auto go = [this, s] (const juce::File& src)
        {
            const auto wav = hub::ensureWav (src, s.id);
            if (wav == juce::File()) { status = U ("Audio faylni o'qib bo'lmadi"); repaint(); return; }
            transport.stop();
            juce::MessageManager::callAsync ([fn = onCreateFromSong, s, wav] { if (fn) fn (s, wav); });
        };
        if (mp3.existsAsFile()) { go (mp3); return; }
        status = U ("Qo'shiq yuklanmoqda\xe2\x80\xa6"); repaint();
        const auto rel = s.audio;
        launch ([this, rel, mp3, go]
        {
            const bool ok = hub::fetchToFile (rel, mp3, &cancel);
            juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<StartHub> (this), ok, mp3, go]
            {
                if (sp == nullptr) return;
                if (! ok) { sp->status = U ("Yuklab bo'lmadi \xe2\x80\x94 internetni tekshiring"); sp->repaint(); return; }
                sp->status = {}; go (mp3);
            });
        });
    }
    void startFile (const juce::File& f)
    {
        transport.stop(); transport.setSource (nullptr); readerSrc.reset();
        std::unique_ptr<juce::AudioFormatReader> r (formats.createReaderFor (f));
        if (r == nullptr) { status = U ("Bu formatni o'qib bo'lmadi: ") + f.getFileExtension(); repaint(); return; }
        const double sr = r->sampleRate;
        readerSrc = std::make_unique<juce::AudioFormatReaderSource> (r.release(), true);
        transport.setSource (readerSrc.get(), 32768, &readAhead, sr);
        transport.setPosition (0); transport.start(); repaint();
    }
    juce::File audioCacheFile (const hub::Song& s) const { return hub::playlistCache().getChildFile (s.id + "." + juce::File (s.audio).getFileExtension().trimCharactersAtStart (".")); }
    juce::File coverCacheFile (const hub::Song& s) const { return hub::playlistCache().getChildFile (s.id + ".cover." + (s.cover.isNotEmpty() ? juce::File (s.cover).getFileExtension().trimCharactersAtStart (".") : juce::String ("jpg"))); }
    void fetchMissingCovers()
    {
        juce::StringArray rels, ids; juce::Array<juce::File> dsts;
        for (auto& s : songs) if (! s.coverImg.isValid() && s.cover.isNotEmpty()) { rels.add (s.cover); ids.add (s.id); dsts.add (coverCacheFile (s)); }
        if (rels.isEmpty()) return;
        launch ([this, rels, ids, dsts]
        {
            for (int i = 0; i < rels.size() && ! cancel; ++i)
            {
                if (! dsts[i].existsAsFile() && ! hub::fetchToFile (rels[i], dsts[i], &cancel)) continue;
                auto img = juce::ImageFileFormat::loadFrom (dsts[i]);
                if (img.isValid() && img.getWidth() > 360) { img = hub::coverCrop (img, 360, 360); hub::saveImage (img, dsts[i].withFileExtension ("png")); dsts[i].deleteFile(); hub::saveImage (img, dsts[i]); }
                const auto id = ids[i];
                juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<StartHub> (this), id, img]
                {
                    if (sp == nullptr) return;
                    for (auto& s : sp->songs) if (s.id == id) s.coverImg = img;
                    sp->repaint();
                });
            }
        });
    }
    void launch (std::function<void()> fn)
    {
        threads.emplace_back (std::move (fn));
    }

    // Hub o'zini o'chirib yuborishi mumkin bo'lgan amallar doim keyinroq, nusxa funksiya bilan chaqiriladi
    void openLater (const juce::File& f) { transport.stop(); juce::MessageManager::callAsync ([fn = onOpen, f] { if (fn) fn (f); }); }
    bool isGroup() const { return pf.getBoolValue ("azProfileGroup", false); }
    void loadAvatar()
    {
        const auto f = hub::appDataDir().getChildFile ("profile.png");
        avatar = f.existsAsFile() ? juce::ImageFileFormat::loadFrom (f) : juce::Image();
    }
    void chooseAvatar()
    {
        chooser = std::make_unique<juce::FileChooser> (U ("Profil rasmi (artist yoki guruh)"), juce::File::getSpecialLocation (juce::File::userPicturesDirectory), "*.png;*.jpg;*.jpeg");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult(); if (! f.existsAsFile()) return;
            setAvatarFromFile (f);
        });
    }
public:
    bool setAvatarFromFile (const juce::File& f)
    {
        auto img = juce::ImageFileFormat::loadFrom (f);
        if (! img.isValid()) return false;
        hub::saveImage (hub::coverCrop (img, 256, 256), hub::appDataDir().getChildFile ("profile.png"));
        loadAvatar(); repaint();
        return avatar.isValid();
    }
    bool setProjectCover (const juce::File& project, const juce::File& image)
    {
        auto img = juce::ImageFileFormat::loadFrom (image);
        if (! img.isValid()) return false;
        hub::saveImage (hub::coverCrop (img, 480, 394), hub::coverFile (project));
        refreshProjects(); repaint(); return true;
    }
private:
    void chooseCover (const juce::File& project)
    {
        chooser = std::make_unique<juce::FileChooser> (U ("Loyiha muqovasi"), juce::File::getSpecialLocation (juce::File::userPicturesDirectory), "*.png;*.jpg;*.jpeg");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this, project] (const juce::FileChooser& fc)
        { if (fc.getResult().existsAsFile()) setProjectCover (project, fc.getResult()); });
    }
    void chooseProjectFile()
    {
        chooser = std::make_unique<juce::FileChooser> (U ("Loyihani ochish"), AppEngine::projectsFolder(), "*.azmus;*.tracktionedit");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        { if (fc.getResult().existsAsFile()) openLater (fc.getResult()); });
    }
    void chooseAudioForProject()
    {
        chooser = std::make_unique<juce::FileChooser> (U ("Audio fayl (minus, demo, referens)"), juce::File::getSpecialLocation (juce::File::userMusicDirectory), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        { if (fc.getResult().existsAsFile()) { transport.stop(); juce::MessageManager::callAsync ([fn = onCreateFromAudio, f = fc.getResult()] { if (fn) fn (f); }); } });
    }
    void emptyTrash()
    {
        for (const auto& e : juce::RangedDirectoryIterator (hub::trashDir(), false, "*.azmus", juce::File::findFiles))
        { const auto f = e.getFile(); hub::thumbFile (f).deleteFile(); hub::coverFile (f).deleteFile(); hub::metaFile (f).deleteFile(); f.deleteFile(); }
        refreshProjects(); repaint();
    }

    void timerCallback() override
    {
        phase += 0.08;
        if (page == playlistPage || hoverPromo) repaint();
        else repaint (promoRect().toNearestInt());
    }
    void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint(); }

    AppEngine& app; juce::PropertiesFile& pf;
    juce::Label nameL, roleL; juce::Image avatar;
    juce::TextButton swapB, photoB, trashTopB, emptyTrashB, siteB, reloadB, playB, stopB, songProjB;
    juce::OwnedArray<juce::TextButton> navB, toolB;
    juce::TextEditor searchE; juce::ComboBox sortBox; juce::Slider vol;
    juce::Array<hub::Project> projects;
    juce::Array<hub::Song> songs; juce::String artist, downloading, status;
    int curSong = -1, hover = -1; bool hoverBanner = false, hoverPromo = false, loading = false, netError = false;
    float scroll = 0; double phase = 0;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::AudioFormatManager formats; juce::AudioSourcePlayer player; juce::AudioTransportSource transport;
    juce::TimeSliceThread readAhead { "AZ hub audio" };
    std::unique_ptr<juce::AudioFormatReaderSource> readerSrc;
    std::atomic<bool> cancel { false }; std::vector<std::thread> threads;

    friend class StartHubTest;
};
}
