// =============================================================================
//  AZ MUS — bir nechta audio fayl tashlanganda "Import parametrlari" oynasi
//  (Cubase "Import Options" tuzilishida): Alohida treklar / Bitta trek / Bekor qilish,
//  loyiha papkasiga nusxalash, loyiha sample rate'iga o'girish, "boshqa so'ramaslik".
// =============================================================================
#pragma once
#include "Dop.h"

namespace az
{
struct ImportOptions
{
    enum Mode { cancel = 0, differentTracks = 1, oneTrack = 2 };
    Mode mode = cancel;
    bool copyToProject = false, convertRate = true, sortByName = true, dontAsk = false;
};

struct ImportFileInfo { juce::File file; double rate = 0, length = 0; int bits = 0, channels = 0; bool valid = false; };

inline juce::Array<ImportFileInfo> scanImportFiles (te::Engine& e, const juce::StringArray& paths)
{
    juce::Array<ImportFileInfo> r;
    for (auto& p : paths)
    {
        ImportFileInfo i; i.file = juce::File (p);
        te::AudioFile af (e, i.file);
        if (af.isValid()) { i.valid = true; i.rate = af.getSampleRate(); i.length = af.getLength(); i.bits = af.getBitsPerSample(); i.channels = af.getNumChannels(); }
        r.add (i);
    }
    return r;
}

// --------------------------------------------------------------------- oyna tarkibi
class ImportOptionsPanel : public juce::Component
{
public:
    std::function<void (ImportOptions)> onDone;

    ImportOptionsPanel (const juce::Array<ImportFileInfo>& f, double projectRate, juce::PropertiesFile& p)
        : files (f), projRate (projectRate), pf (p)
    {
        for (auto& i : files) { totalBytes += i.file.getSize(); if (i.valid && std::abs (i.rate - projRate) > 0.5) ++mismatch; }
        auto tog = [this] (juce::ToggleButton& b, const juce::String& t, const char* key, bool def)
        {
            addAndMakeVisible (b); b.setButtonText (t); b.setToggleState (pf.getBoolValue (key, def), juce::dontSendNotification);
            b.setColour (juce::ToggleButton::textColourId, text); b.setColour (juce::ToggleButton::tickColourId, red);
        };
        tog (copyB, U ("Fayllarni loyiha papkasiga nusxalash (Copy Files to Working Directory)"), "azImpCopy", false);
        tog (convB, U ("Kerak bo'lsa loyiha sample rate'iga o'girish (") + dop::rateText (projRate) + ")", "azImpConvert", true);
        tog (sortB, U ("Treklarni fayl nomi bo'yicha tartiblash"), "azImpSort", true);
        tog (askB, U ("Boshqa so'ramaslik (Do Not Ask Again)"), "azImpDontAsk", false);
        convB.setEnabled (mismatch > 0);
        if (mismatch == 0) convB.setButtonText (U ("Sample rate: hamma fayllar loyihaga mos (") + dop::rateText (projRate) + ")");

        for (auto* b : { &diffB, &oneB, &cancelB }) { addAndMakeVisible (b); b->getProperties().set ("radius", 7.0f); }
        diffB.setButtonText (U ("Alohida treklar")); oneB.setButtonText (U ("Bitta trek")); cancelB.setButtonText (U ("Bekor qilish"));
        diffB.setTooltip (U ("Har bir fayl o'z trekiga, hammasi bir vaqtda boshlanadi (stemlar uchun)"));
        oneB.setTooltip (U ("Hammasi bitta trekka ketma-ket joylanadi"));
        diffB.setClickingTogglesState (false); diffB.setToggleState (true, juce::dontSendNotification);
        diffB.onClick = [this] { finish (ImportOptions::differentTracks); };
        oneB.onClick = [this] { finish (ImportOptions::oneTrack); };
        cancelB.onClick = [this] { finish (ImportOptions::cancel); };
        addAndMakeVisible (list); list.setViewedComponent (&rows, false); list.setScrollBarsShown (true, false);
        rows.owner = this;
        setSize (680, 560);
    }
    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::returnKey) { finish (ImportOptions::differentTracks); return true; }
        if (k == juce::KeyPress::escapeKey) { finish (ImportOptions::cancel); return true; }
        return false;
    }
    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        g.fillAll (bg2);
        label (g, U ("Import parametrlari"), { 22, 16, 400, 26 }, 18, text, Justification::centredLeft, 700);
        String sum = String (files.size()) + U (" ta audio fayl  \xc2\xb7  ") + File::descriptionOfSizeInBytes (totalBytes);
        if (mismatch > 0) sum << U ("  \xc2\xb7  ") << mismatch << U (" tasi loyiha sample rate'iga mos emas");
        label (g, sum, { 22, 42, 640, 18 }, 11, mismatch > 0 ? yellow : silver, Justification::centredLeft, 500);
        caps (g, U ("FAYLLAR"), { 22, 70, 200, 12 });
        caps (g, U ("PARAMETRLAR"), { 22, (float) list.getBottom() + 14, 200, 12 });
        label (g, U ("Fayllarni qanday joylashtiray?"), { 22, (float) diffB.getY() - 26, 400, 18 }, 12.5f, text, Justification::centredLeft, 600);
    }
    void resized() override
    {
        list.setBounds (18, 86, getWidth() - 36, 220);
        rows.setSize (list.getWidth() - 10, juce::jmax (list.getHeight(), files.size() * 24));
        int y = list.getBottom() + 32;
        for (auto* b : { &copyB, &convB, &sortB }) { b->setBounds (20, y, getWidth() - 40, 24); y += 26; }
        askB.setBounds (20, y + 8, getWidth() - 40, 24);
        const int bw = 140;
        cancelB.setBounds (getWidth() - 22 - bw + 20, getHeight() - 48, bw - 20, 32);
        oneB.setBounds (cancelB.getX() - 10 - bw, getHeight() - 48, bw, 32);
        diffB.setBounds (oneB.getX() - 10 - bw, getHeight() - 48, bw, 32);
    }
    ImportOptions current (ImportOptions::Mode m) const
    {
        ImportOptions o; o.mode = m;
        o.copyToProject = copyB.getToggleState(); o.convertRate = convB.getToggleState() && mismatch > 0;
        o.sortByName = sortB.getToggleState(); o.dontAsk = askB.getToggleState();
        return o;
    }
private:
    void finish (ImportOptions::Mode m)
    {
        auto o = current (m);
        if (m != ImportOptions::cancel)
        {
            pf.setValue ("azImpCopy", o.copyToProject); pf.setValue ("azImpConvert", convB.getToggleState());
            pf.setValue ("azImpSort", o.sortByName); pf.setValue ("azImpDontAsk", o.dontAsk); pf.setValue ("azImpMode", (int) m);
        }
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>()) dw->exitModalState (0);
        if (onDone) { auto fn = onDone; juce::MessageManager::callAsync ([fn, o] { fn (o); }); }
    }
    struct Rows : public juce::Component
    {
        ImportOptionsPanel* owner = nullptr;
        void paint (juce::Graphics& g) override
        {
            using namespace juce;
            for (int i = 0; i < owner->files.size(); ++i)
            {
                const auto& f = owner->files.getReference (i);
                auto r = Rectangle<float> (0, i * 24.0f, (float) getWidth(), 24);
                if (i % 2 == 0) { g.setColour (Colours::white.withAlpha (0.03f)); g.fillRect (r); }
                g.setColour (trackColours[i % 8]); g.fillRoundedRectangle (r.getX() + 6, r.getY() + 7, 4, 10, 2);
                label (g, f.file.getFileName(), r.withTrimmedLeft (18).withWidth (r.getWidth() * 0.52f), 11, text, Justification::centredLeft, 500);
                const bool mis = f.valid && std::abs (f.rate - owner->projRate) > 0.5;
                const String dot = String::fromUTF8 ("  \xc2\xb7  ");
                const String info = f.valid ? dop::rateText (f.rate) + dot + String (f.bits) + "-bit" + dot + (f.channels > 1 ? "stereo" : "mono") : U ("o'qib bo'lmadi");
                label (g, info, { r.getWidth() * 0.55f, r.getY(), r.getWidth() * 0.30f, 24 }, 10, ! f.valid ? red : mis ? yellow : dim, Justification::centredLeft, 500);
                const int t = roundToInt (f.length);
                label (g, String (t / 60) + ":" + String (t % 60).paddedLeft ('0', 2), r.withTrimmedRight (10), 10, silver, Justification::centredRight, 500);
            }
        }
    } rows;
    juce::Array<ImportFileInfo> files; double projRate; juce::PropertiesFile& pf;
    juce::int64 totalBytes = 0; int mismatch = 0;
    juce::ToggleButton copyB, convB, sortB, askB; juce::TextButton diffB, oneB, cancelB;
    juce::Viewport list;
};

inline void showImportOptions (juce::Component* parent, const juce::Array<ImportFileInfo>& files, double projRate, juce::PropertiesFile& pf, std::function<void (ImportOptions)> done)
{
    auto* panel = new ImportOptionsPanel (files, projRate, pf);
    panel->onDone = std::move (done);
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (panel);
    o.dialogTitle = "AZ MUS";
    o.dialogBackgroundColour = bg2;
    o.componentToCentreAround = parent;
    o.escapeKeyTriggersCloseButton = false;
    o.useNativeTitleBar = true;
    o.resizable = false;
    if (auto* w = o.launchAsync()) juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<juce::Component> (panel)] { if (sp) sp->grabKeyboardFocus(); }), juce::ignoreUnused (w);
}

// --------------------------------------------------------------------- nusxalash / o'girish (har qanday oqimda ishlaydi)
inline void prepareImportFiles (te::Engine& engine, const juce::Array<ImportFileInfo>& files, const ImportOptions& opt, double rate, const juce::File& dir,
                                juce::Array<juce::File>& results, juce::StringArray& errors, std::function<bool (double, const juce::String&)> progress = nullptr)
{
    for (int i = 0; i < files.size(); ++i)
    {
        const auto& fi = files.getReference (i);
        if (progress && ! progress ((double) i / files.size(), fi.file.getFileName())) return;
        juce::File out = fi.file;
        if (! fi.valid) { errors.add (fi.file.getFileName()); continue; }
        if (opt.convertRate && std::abs (fi.rate - rate) > 0.5)
        {
            juce::String err;
            const auto c = dop::convertFileRate (engine, fi.file, rate, &err, dir); // asl papkaga hech narsa yozilmaydi
            if (c != juce::File()) out = c; else errors.add (fi.file.getFileName() + ": " + err);
        }
        else if (opt.copyToProject && ! fi.file.isAChildOf (dir))
        {
            dir.createDirectory();
            const auto dst = dir.getChildFile (fi.file.getFileName());
            if (dst.existsAsFile() && dst.getSize() == fi.file.getSize()) out = dst;
            else
            {
                const auto d2 = dst.existsAsFile() ? dir.getNonexistentChildFile (fi.file.getFileNameWithoutExtension(), fi.file.getFileExtension(), false) : dst;
                if (fi.file.copyFileTo (d2)) out = d2; else errors.add (fi.file.getFileName() + U (": nusxalab bo'lmadi"));
            }
        }
        results.add (out);
    }
    if (progress) progress (1.0, {});
}

// fon oqimida, progress oynasi bilan
class ImportPrepareJob : public juce::ThreadWithProgressWindow
{
public:
    ImportPrepareJob (te::Engine& e, juce::Array<ImportFileInfo> f, ImportOptions o, double projRate, juce::File destDir,
                      std::function<void (juce::Array<juce::File>, juce::StringArray)> done)
        : juce::ThreadWithProgressWindow (U ("Audio import qilinmoqda..."), true, true), engine (e), files (std::move (f)), opt (o), rate (projRate), dir (destDir), onDone (std::move (done)) {}
    void run() override
    {
        prepareImportFiles (engine, files, opt, rate, dir, results, errors,
                            [this] (double p, const juce::String& m) { setProgress (p); setStatusMessage (m); return ! threadShouldExit(); });
    }
    const juce::Array<juce::File>& getResults() const { return results; }
    void threadComplete (bool userPressedCancel) override
    {
        if (! userPressedCancel && onDone) onDone (results, errors);
        delete this;
    }
private:
    te::Engine& engine; juce::Array<ImportFileInfo> files; ImportOptions opt; double rate; juce::File dir;
    std::function<void (juce::Array<juce::File>, juce::StringArray)> onDone;
    juce::Array<juce::File> results; juce::StringArray errors;
};
}
