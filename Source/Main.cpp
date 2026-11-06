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

// ---------------------------------------------------------------- VST3 skaneri (fon oqimi)
class PluginScanThread : public juce::Thread
{
public:
    PluginScanThread (te::Engine& e) : juce::Thread ("AZ VST3 scan"), engine (e) {}
    std::atomic<float> progress { 0 };
    juce::CriticalSection lock; juce::String current;

    void run() override
    {
        auto& pm = engine.getPluginManager();
        for (auto* fmt : pm.pluginFormatManager.getFormats())
        {
            if (fmt->getName() != "VST3" && fmt->getName() != "AudioUnit") continue;
            juce::PluginDirectoryScanner sc (pm.knownPluginList, *fmt, fmt->getDefaultLocationsToSearch(), true,
                                             engine.getPropertyStorage().getAppCacheFolder().getChildFile ("PluginScanDeadMansPedal"), true);
            juce::String name;
            for (;;)
            {
                { const juce::ScopedLock sl (lock); current = sc.getNextPluginFileThatWillBeScanned(); }
                if (threadShouldExit() || ! sc.scanNextFile (true, name)) break;
                progress = sc.getProgress();
            }
        }
        progress = 1.0f;
    }
private:
    te::Engine& engine;
};

// ---------------------------------------------------------------- Asosiy oyna
class MainWindow : public juce::DocumentWindow
{
public:
    MainWindow (az::AppEngine& app)
        : DocumentWindow ("AZ MUS", az::bg0, DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (new az::MainComponent (app), true);
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
    bool moreThanOneInstanceAllowed() override          { return false; }

    void initialise (const juce::String& cmd) override
    {
        args = juce::StringArray::fromTokens (cmd, true);
        args.trim(); args.removeEmptyStrings();
        for (auto& a : args) a = a.unquoted();
        juce::LookAndFeel::setDefaultLookAndFeel (&lnf);

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
                juce::String cur; { const juce::ScopedLock sl (scan->lock); cur = scan->current; }
                s.setStep (2, U ("VST3 plaginlar skanerlanmoqda\xe2\x80\xa6"), 0.25f + 0.5f * scan->progress.load());
                s.setDetail (juce::File (cur).getFileName());
                if (! scan->isThreadRunning()) { stage = 3; startTimer (200); }
                break;
            }
            case 3:   // kutubxona
                s.setStep (3, U ("Sample kutubxona tayyorlanmoqda\xe2\x80\xa6"), 0.82f);
                s.setDetail (juce::String (app->engine.getPluginManager().knownPluginList.getNumTypes()) + U (" ta VST3 \xc2\xb7 186 ta Airwindows"));
                stage = 4; startTimer (400); break;
            case 4:   // loyiha
            {
                s.setStep (4, U ("Loyiha oynasi ochilmoqda\xe2\x80\xa6"), 0.95f);
                const juce::File last (app->engine.getPropertyStorage().getPropertiesFile().getValue ("azLastEdit"));
                juce::File arg; for (auto& a : args) if (a.unquoted().endsWith (".azmus")) arg = juce::File (a.unquoted());
                if (! (arg.existsAsFile() && app->openProject (arg)) && ! (last.existsAsFile() && app->openProject (last)))
                    app->newProject();
                if (args.contains ("--demo")) makeDemo (*app);
                mainWindow = std::make_unique<MainWindow> (*app);
                stage = 5; startTimer (500); break;
            }
            case 5:
                s.setStep (5, U ("Tayyor"), 1.0f);
                splash->toFront (false);
                stage = 6; startTimer (500); break;
            case 6:
                stopTimer(); splash.reset();
                if (mainWindow) mainWindow->toFront (true);
                if (args.contains ("--screenshot")) { stage = 50; startTimer (2500); }
                break;
            case 50:  // avtomatik skrinshot (test uchun)
            {
                stopTimer();
                const auto out = juce::File::getCurrentWorkingDirectory().getChildFile (args[args.indexOf ("--screenshot") + 1]);
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
    int stage = 0;
};

START_JUCE_APPLICATION (AZMusApp)
