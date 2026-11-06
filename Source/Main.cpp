// =============================================================================
//  AZ MUS — Digital Audio Workstation · Azimxodjayev Music
//  Ishga tushish: splash -> audio -> MIDI -> VST3 skan -> kutubxona -> loyiha
// =============================================================================
#include <JuceHeader.h>
#include "Theme.h"
#include "AppEngine.h"
#include "Splash.h"
#include "MainComponent.h"
#include "SelfTest.h"
using az::U;

// ---------------------------------------------------------------- Xavfsiz (alohida jarayonli) skaner
// Har bir plagin AZ MUS'ning alohida nusxasida (--scan-plugin) tekshiriladi.
// Plagin qulasa yoki osilib qolsa — faqat o'sha jarayon o'ladi, plagin qora ro'yxatga tushadi.
struct OutOfProcessScanner : public juce::KnownPluginList::CustomScanner
{
    explicit OutOfProcessScanner (juce::KnownPluginList& l) : list (l) {}

    bool findPluginTypesFor (juce::AudioPluginFormat& format, juce::OwnedArray<juce::PluginDescription>& result, const juce::String& id) override
    {
        if (list.getBlacklistedFiles().contains (id)) return true;
        const auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("azmus_scan", ".xml");
        juce::ChildProcess cp;
        const juce::StringArray cmd { juce::File::getSpecialLocation (juce::File::currentExecutableFile).getFullPathName(),
                                      "--scan-plugin", format.getName(), id, tmp.getFullPathName() };
        if (! cp.start (cmd, 0)) return false;
        const auto t0 = juce::Time::getMillisecondCounter();
        while (cp.isRunning())
        {
            if (shouldExit()) { cp.kill(); tmp.deleteFile(); return true; }
            if (juce::Time::getMillisecondCounter() - t0 > 45000)            // osilib qoldi
            {
                cp.kill(); list.addToBlacklist (id); failed.add (id); tmp.deleteFile(); return true;
            }
            juce::Thread::sleep (30);
        }
        if (cp.getExitCode() != 0 || ! tmp.existsAsFile())                   // qulab tushdi
        {
            list.addToBlacklist (id); failed.add (id); tmp.deleteFile(); return true;
        }
        if (auto xml = juce::parseXML (tmp))
            for (auto* e : xml->getChildIterator())
            {
                juce::PluginDescription d;
                if (d.loadFromXml (*e)) result.add (new juce::PluginDescription (d));
            }
        tmp.deleteFile();
        return true;
    }

    juce::KnownPluginList& list;
    juce::StringArray failed;
};

// Bola jarayon: bitta plaginni o'qib, natijani XML faylga yozadi
static int runScanChild (const juce::StringArray& a)
{
    const int i = a.indexOf ("--scan-plugin");
    if (i < 0 || a.size() < i + 4) return 2;
    juce::AudioPluginFormatManager fm;
    juce::addDefaultFormatsToManager (fm);
    for (auto* fmt : fm.getFormats())
        if (fmt->getName() == a[i + 1])
        {
            juce::OwnedArray<juce::PluginDescription> found;
            fmt->findAllTypesForFile (found, a[i + 2]);
            juce::XmlElement root ("PLUGINS");
            for (auto* d : found) root.addChildElement (d->createXml().release());
            return root.writeTo (juce::File (a[i + 3])) ? 0 : 3;
        }
    return 4;
}

// ---------------------------------------------------------------- VST3 skaneri (fon oqimi)
class PluginScanThread : public juce::Thread
{
public:
    PluginScanThread (te::Engine& e) : juce::Thread ("AZ VST3 scan"), engine (e) {}
    std::atomic<float> progress { 0 };
    juce::CriticalSection lock; juce::String current;
    OutOfProcessScanner* scanner = nullptr;

    void run() override
    {
        auto& pm = engine.getPluginManager();
        auto sc = std::make_unique<OutOfProcessScanner> (pm.knownPluginList);
        scanner = sc.get();
        pm.knownPluginList.setCustomScanner (std::move (sc));
        for (auto* fmt : pm.pluginFormatManager.getFormats())
        {
            if (fmt->getName() != "VST3" && fmt->getName() != "AudioUnit") continue;
            juce::PluginDirectoryScanner ds (pm.knownPluginList, *fmt, fmt->getDefaultLocationsToSearch(), true,
                                             engine.getPropertyStorage().getAppCacheFolder().getChildFile ("PluginScanDeadMansPedal"), false);
            juce::String name;
            for (;;)
            {
                { const juce::ScopedLock sl (lock); current = ds.getNextPluginFileThatWillBeScanned(); }
                if (threadShouldExit() || ! ds.scanNextFile (true, name)) break;
                progress = ds.getProgress();
            }
        }
        progress = 1.0f;
    }
private:
    te::Engine& engine;
};

// ---------------------------------------------------------------- Asosiy oyna
// Butun DAW maydonini masshtablash (Ctrl + / Ctrl - / Ctrl 0, yoki menyu qatoridagi − +)
class ZoomHost : public juce::Component
{
public:
    ZoomHost (az::AppEngine& a) : app (a), main (a)
    {
        addAndMakeVisible (main);
        scale = (float) app.engine.getPropertyStorage().getPropertiesFile().getDoubleValue ("azUiScale", 1.0);
        main.onUiZoom = [this] (int d) { step (d); };
        main.setUiZoomLabel (scale);
        setSize (1680, 1000);
    }
    void step (int d)
    {
        static const float steps[] = { 0.7f, 0.8f, 0.9f, 1.0f, 1.1f, 1.25f, 1.4f, 1.6f, 1.8f, 2.0f };
        int i = 3; float best = 10;
        for (int k = 0; k < 10; ++k) if (std::abs (steps[k] - scale) < best) { best = std::abs (steps[k] - scale); i = k; }
        scale = d == 0 ? 1.0f : steps[juce::jlimit (0, 9, i + d)];
        app.engine.getPropertyStorage().getPropertiesFile().setValue ("azUiScale", (double) scale);
        main.setUiZoomLabel (scale);
        resized();
    }
    void resized() override
    {
        main.setTransform (juce::AffineTransform::scale (scale));
        main.setBounds (0, 0, juce::roundToInt ((float) getWidth() / scale), juce::roundToInt ((float) getHeight() / scale));
    }
    az::AppEngine& app; az::MainComponent main; float scale = 1.0f;
};

class MainWindow : public juce::DocumentWindow
{
public:
    MainWindow (az::AppEngine& app)
        : DocumentWindow ("AZ MUS", az::bg0, DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (new ZoomHost (app), false);
        setResizable (true, false);
        setResizeLimits (1200, 720, 10000, 10000);
        centreWithSize (1680, 1000);
       #if ! JUCE_LINUX
        setFullScreen (true);
       #endif
        setVisible (true);
    }
    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
};

// ---------------------------------------------------------------- Ilova
class AZMusApp : public juce::JUCEApplication, private juce::Timer
{
public:
    const juce::String getApplicationName() override    { return "AZ MUS"; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override
    {
        const auto cmd = juce::JUCEApplicationBase::getCommandLineParameters();
        return cmd.contains ("--scan-plugin") || cmd.contains ("--selftest");
    }

    void initialise (const juce::String& cmd) override
    {
        args = juce::StringArray::fromTokens (cmd, true);
        args.trim(); args.removeEmptyStrings();
        for (auto& a : args) a = a.unquoted();
        juce::LookAndFeel::setDefaultLookAndFeel (&lnf);
        az::loadLanguage();
        if (args.contains ("--lang")) az::lang = args[args.indexOf ("--lang") + 1].getIntValue();

        if (args.contains ("--scan-plugin"))
        {
            setApplicationReturnValue (runScanChild (args)); quit(); return;
        }
        if (args.contains ("--selftest"))
        {
            az::headlessMode = true;
            const int code = runSelfTest (args);
            setApplicationReturnValue (code); quit(); return;
        }
        splash = std::make_unique<az::SplashWindow>();
        if (args.contains ("--splash-shot")) { stage = 100; startTimer (2200); return; }
        stage = 0; startTimer (350);
    }

    void shutdown() override
    {
        stopTimer();
        if (scan) scan->stopThread (8000);
        mainWindow.reset(); splash.reset(); app.reset();
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void systemRequestedQuit() override { if (app) app->saveIfPossible(); quit(); }
    void anotherInstanceStarted (const juce::String& cmd) override
    {
        const juce::File f (cmd.unquoted().trim());
        if (app && f.existsAsFile()) app->openProject (f);
    }

private:
    void timerCallback() override
    {
        auto& s = splash->content;
        switch (stage)
        {
            case 0:   // audio dvigatel
                s.setStep (0, U ("Audio drayver ishga tushirilmoqda\xe2\x80\xa6"), 0.08f);
                app = std::make_unique<az::AppEngine>();
                if (auto* d = app->engine.getDeviceManager().deviceManager.getCurrentAudioDevice())
                    s.setDetail (d->getName() + U (" \xc2\xb7 ") + juce::String ((int) d->getCurrentSampleRate()) + " Hz");
                stage = 1; startTimer (450); break;
            case 1:   // MIDI
                app->engine.getDeviceManager().rescanMidiDeviceList();
                s.setStep (1, U ("MIDI qurilmalar topildi:"), 0.2f);
                s.setDetail (juce::String (app->engine.getDeviceManager().getNumMidiInDevices()) + U (" ta kirish"));
                stage = 2; startTimer (450); break;
            case 2:   // VST3
                s.setStep (2, U ("VST3 plaginlar skanerlanmoqda\xe2\x80\xa6"), 0.25f); s.setDetail ({});
                if (args.contains ("--noscan")) { stage = 3; startTimer (200); break; }
                scan = std::make_unique<PluginScanThread> (app->engine); scan->startThread();
                stage = 20; startTimer (60); break;
            case 20:
            {
                if (splash->skipRequested)
                {
                    s.setStep (2, juce::String::fromUTF8 ("Skanerlash o'tkazib yuborildi \xe2\x80\x94 keyingi safar davom etadi"), 0.75f);
                    scan->signalThreadShouldExit(); scan->stopThread (5000);
                    stage = 3; startTimer (600); break;
                }
                juce::String cur; { const juce::ScopedLock sl (scan->lock); cur = scan->current; }
                s.setStep (2, U ("VST3 plaginlar skanerlanmoqda\xe2\x80\xa6"), 0.25f + 0.5f * scan->progress.load());
                s.setDetail (juce::File (cur).getFileName());
                if (! scan->isThreadRunning())
                {
                    if (scan->scanner != nullptr && scan->scanner->failed.size() > 0)
                        failedPlugins = scan->scanner->failed;
                    stage = 3; startTimer (200);
                }
                break;
            }
            case 3:   // kutubxona
                s.setStep (3, U ("Sample kutubxona tayyorlanmoqda\xe2\x80\xa6"), 0.82f);
                s.setDetail (juce::String (app->engine.getPluginManager().knownPluginList.getNumTypes()) + U (" ta VST3 \xc2\xb7 186 ta Airwindows")
                             + (failedPlugins.isEmpty() ? juce::String() : U (" \xc2\xb7 ") + juce::String (failedPlugins.size()) + U (" ta nosoz plagin chetlatildi")));
                stage = 4; startTimer (400); break;
            case 4:   // loyiha
            {
                s.setStep (4, U ("Loyiha oynasi ochilmoqda\xe2\x80\xa6"), 0.95f);
                const juce::File last (app->engine.getPropertyStorage().getPropertiesFile().getValue ("azLastEdit"));
                juce::File arg; for (auto& a : args) if (a.unquoted().endsWith (".azmus")) arg = juce::File (a.unquoted());
                if (! (arg.existsAsFile() && app->openProject (arg)) && ! (last.existsAsFile() && app->openProject (last)))
                    app->newProject();
                if (args.contains ("--demo")) makeDemo (*app);
                if (args.contains ("--mm")) if (auto* m = az::Routing (*app).monitorMatch (true)) { m->source = 1; m->target = 0; }   // test: A8X → Focal
                mainWindow = std::make_unique<MainWindow> (*app);
                if (args.contains ("--size"))   // test: oyna o'lchami (masalan 1366x768)
                {
                    auto sz = args[args.indexOf ("--size") + 1];
                    mainWindow->setFullScreen (false);
                    mainWindow->setBounds (0, 0, sz.upToFirstOccurrenceOf ("x", false, false).getIntValue(), sz.fromFirstOccurrenceOf ("x", false, false).getIntValue());
                }
                stage = 5; startTimer (500); break;
            }
            case 5:
                s.setStep (5, U ("Tayyor"), 1.0f);
                splash->toFront (false);
                stage = 6; startTimer (500); break;
            case 6:
                stopTimer(); splash.reset();
                if (mainWindow) mainWindow->toFront (true);
                if (args.contains ("--hold"))   // test: oynalarni ochib, tashqaridan skrinshot olish uchun
                {
                    if (auto* zh = dynamic_cast<ZoomHost*> (mainWindow->getContentComponent()))
                    {
                        if (args.contains ("--connections")) zh->main.showConnections();
                        auto selectVocal = [this]
                        {
                            for (auto* t : te::getAudioTracks (*app->edit))
                                for (auto* c : t->getClips())
                                    if (dynamic_cast<te::WaveAudioClip*> (c) && (t->getName().containsIgnoreCase ("vok") || t->getName().containsIgnoreCase ("voc"))) { app->selection.selectOnly (c); return; }
                        };
                        if (args.contains ("--dop")) { selectVocal(); zh->main.openDop(); }
                        if (args.contains ("--render")) { selectVocal(); zh->main.openRender(); }
                        if (args.contains ("--mixerwin")) zh->main.toggleMixerWindow();
                        if (args.contains ("--picker"))
                            az::showPluginPicker (*app, zh->main.localAreaToGlobal (juce::Rectangle<int> (110, 200, 120, 26)), [] (const az::PickItem&) {});
                    }
                    break;
                }
                if (args.contains ("--screenshot")) { stage = 50; startTimer (2500); }
                else if (! failedPlugins.isEmpty())
                {
                    juce::StringArray names; for (auto& f : failedPlugins) names.add (juce::File (f).getFileName());
                    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "AZ MUS",
                        juce::String::fromUTF8 ("Quyidagi plaginlar skanerlashda qulab tushdi yoki javob bermadi, shuning uchun chetlatildi "
                                                "(AZ MUS'ga zarar yetmadi):\n\n") + names.joinIntoString ("\n")
                        + juce::String::fromUTF8 ("\n\nQayta urinish: Plaginlar \xe2\x86\x92 VST3 plaginlarni skanerlash \xe2\x86\x92 Options \xe2\x86\x92 Clear blacklist"));
                }
                break;
            case 50:  // avtomatik skrinshot (test uchun)
            {
                stopTimer();
                const auto out = juce::File::getCurrentWorkingDirectory().getChildFile (args[args.indexOf ("--screenshot") + 1]);
                if (args.contains ("--connections")) if (auto* zh = dynamic_cast<ZoomHost*> (mainWindow->getContentComponent())) { zh->main.showConnections(); }
                auto img = mainWindow->getContentComponent()->createComponentSnapshot (mainWindow->getContentComponent()->getLocalBounds(), true, 1.0f);
                juce::FileOutputStream fo (out); fo.setPosition (0); fo.truncate();
                juce::PNGImageFormat().writeImageToStream (img, fo);
                quit(); break;
            }
            case 100: // splash skrinshoti
            {
                stopTimer();
                auto img = splash->content.createComponentSnapshot (splash->content.getLocalBounds(), true, 2.0f);
                const auto out = juce::File::getCurrentWorkingDirectory().getChildFile (args[args.indexOf ("--splash-shot") + 1]);
                juce::FileOutputStream fo (out); fo.setPosition (0); fo.truncate();
                juce::PNGImageFormat().writeImageToStream (img, fo);
                quit(); break;
            }
            default: stopTimer(); break;
        }
    }

    juce::StringArray args;
    az::LookAndFeel lnf;
    std::unique_ptr<az::AppEngine> app;
    std::unique_ptr<az::SplashWindow> splash;
    std::unique_ptr<MainWindow> mainWindow;
    std::unique_ptr<PluginScanThread> scan;
    juce::StringArray failedPlugins;
    int stage = 0;
};

START_JUCE_APPLICATION (AZMusApp)
