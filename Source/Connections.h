// =============================================================================
//  AZ MUS — Audio ulanishlar (F4), Cubase "Audio Connections" kabi:
//  Inputs · Outputs · Group/FX · External FX · External Instruments · Control Room
// =============================================================================
#pragma once
#include "PluginPicker.h"

namespace az
{
// Umumiy jadval: ustunlar + qatorlar (Cubase uslubida)
class ConnTable : public juce::Component
{
public:
    struct Row { juce::StringArray cells; bool on = true, hasToggle = false, highlight = false; };
    juce::StringArray columns; juce::Array<float> widths;   // ulush
    juce::Array<Row> rows;
    std::function<void (int row, int col, const juce::MouseEvent&)> onClick;
    std::function<void (int row, int col)> onDoubleClick;
    int selected = -1;

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        g.fillAll (Colour (0xff141216));
        const float W = (float) getWidth();
        auto hdr = Rectangle<float> (0, 0, W, 26);
        g.setColour (Colour (0xff1c1a1f)); g.fillRect (hdr);
        float x = 0;
        for (int c = 0; c < columns.size(); ++c)
        {
            const float w = W * widths[c];
            label (g, columns[c], { x + 10, 0, w - 12, 26 }, 11, silver, Justification::centredLeft, 600);
            g.setColour (Colours::white.withAlpha (0.06f)); g.drawVerticalLine ((int) (x + w) - 1, 3, 23);
            x += w;
        }
        for (int r = 0; r < rows.size(); ++r)
        {
            const float y = 26.0f + (float) r * 26;
            if (r == selected) { g.setColour (red.withAlpha (0.14f)); g.fillRect (0.0f, y, W, 26.0f); }
            else if (r % 2) { g.setColour (Colours::white.withAlpha (0.018f)); g.fillRect (0.0f, y, W, 26.0f); }
            x = 0;
            for (int c = 0; c < columns.size(); ++c)
            {
                const float w = W * widths[c];
                float tx = x + 10;
                if (c == 0 && rows[r].hasToggle)
                {
                    auto b = Rectangle<float> (x + 8, y + 7, 12, 12);
                    g.setColour (rows[r].on ? (rows[r].highlight ? red : green) : Colours::white.withAlpha (0.12f)); g.fillRoundedRectangle (b, 3);
                    tx += 18;
                }
                const auto col = c == 0 ? text : (rows[r].on ? Colour (0xffa49c9e) : dim2);
                label (g, rows[r].cells[c], { tx, y, w - (tx - x) - 6, 26 }, 11.5f, col, Justification::centredLeft, c == 0 ? 500 : 400);
                x += w;
            }
        }
        if (rows.isEmpty()) label (g, U ("Hozircha bo'sh"), { 0, 40, W, 30 }, 12, dim, Justification::centred);
    }
    int rowAt (float y) const { const int r = (int) ((y - 26) / 26); return (y > 26 && r < rows.size()) ? r : -1; }
    int colAt (float x) const { float acc = 0; for (int c = 0; c < widths.size(); ++c) { acc += (float) getWidth() * widths[c]; if (x < acc) return c; } return widths.size() - 1; }
    void mouseDown (const juce::MouseEvent& e) override
    {
        selected = rowAt (e.position.y); repaint();
        if (selected >= 0 && onClick) onClick (selected, colAt (e.position.x), e);
    }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int r = rowAt (e.position.y);
        if (r >= 0 && onDoubleClick) onDoubleClick (r, colAt (e.position.x));
    }
};

class ConnectionsComponent : public juce::Component, private juce::Timer
{
public:
    std::function<void()> onChanged, onDeviceSetup;

    explicit ConnectionsComponent (AppEngine& a) : app (a), rt (a)
    {
        const char* tabs[] = { "Inputs", "Outputs", "Group/FX", "External FX", "External Instruments", "Control Room" };
        for (int i = 0; i < 6; ++i)
        {
            auto* b = tabB.add (new juce::TextButton (tabs[i]));
            b->getProperties().set ("radius", 4.0f);
            b->onClick = [this, i] { tab = i; rebuild(); };
            addAndMakeVisible (b);
        }
        for (auto* b : { &addB, &removeB, &setupB }) { addAndMakeVisible (b); b->getProperties().set ("radius", 5.0f); }
        setupB.setButtonText (U ("Qurilma sozlamalari..."));
        setupB.onClick = [this] { if (onDeviceSetup) onDeviceSetup(); };
        addB.onClick = [this] { add(); };
        removeB.onClick = [this] { remove(); };
        addAndMakeVisible (table);
        table.onClick = [this] (int r, int c, const juce::MouseEvent& e) { clicked (r, c, e); };
        table.onDoubleClick = [this] (int r, int c) { doubleClicked (r, c); };
        // Control Room boshqaruvlari
        crLevel.setSliderStyle (juce::Slider::LinearHorizontal); crLevel.setRange (-60.0, 6.0, 0.1); crLevel.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 22);
        crLevel.setTextValueSuffix (" dB"); crLevel.setDoubleClickReturnValue (true, 0.0);
        crLevel.onValueChange = [this] { if (auto* cr = rt.controlRoom()) cr->level = (float) crLevel.getValue(); };
        for (auto* b : { &crDim, &crMono, &crMute, &crOn }) { addChildComponent (b); b->setClickingTogglesState (true); b->getProperties().set ("radius", 6.0f); }
        crOn.setButtonText ("Control Room"); crDim.setButtonText ("DIM  -20 dB"); crMono.setButtonText ("MONO"); crMute.setButtonText ("MUTE");
        crDim.onClick  = [this] { if (auto* cr = rt.controlRoom()) cr->dim  = crDim.getToggleState(); };
        crMono.onClick = [this] { if (auto* cr = rt.controlRoom()) cr->mono = crMono.getToggleState(); };
        crMute.onClick = [this] { if (auto* cr = rt.controlRoom()) cr->mute = crMute.getToggleState(); };
        crOn.onClick   = [this] { if (auto* cr = rt.controlRoom()) cr->setEnabled (crOn.getToggleState()); };
        addChildComponent (crLevel); addChildComponent (crOut);
        crOut.onChange = [this] { const int i = crOut.getSelectedItemIndex(); if (auto* d = app.engine.getDeviceManager().getWaveOutDevice (i)) app.engine.getDeviceManager().setDefaultWaveOutDevice (d->getDeviceID()); rebuild(); };
        setSize (860, 460);
        rebuild();
        startTimerHz (2);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10);
        auto tabs = r.removeFromTop (28);
        for (auto* b : tabB) b->setBounds (tabs.removeFromLeft (juce::jmax (90, (int) textWidth (font (12.5f, 600), b->getButtonText()) + 30)).reduced (2, 0));
        r.removeFromTop (8);
        auto bar = r.removeFromTop (30);
        addB.setBounds (bar.removeFromLeft (180)); bar.removeFromLeft (6);
        removeB.setBounds (bar.removeFromLeft (110));
        setupB.setBounds (bar.removeFromRight (200));
        r.removeFromTop (8);
        table.setBounds (r);
        auto c = r.reduced (20, 20);
        crOn.setBounds (c.removeFromTop (32).removeFromLeft (180)); c.removeFromTop (16);
        crOut.setBounds (c.removeFromTop (30).withTrimmedLeft (180).removeFromLeft (360)); c.removeFromTop (16);
        crLevel.setBounds (c.removeFromTop (30).withTrimmedLeft (180).removeFromLeft (420)); c.removeFromTop (20);
        auto row = c.removeFromTop (40).withTrimmedLeft (180);
        for (auto* b : { &crDim, &crMono, &crMute }) { b->setBounds (row.removeFromLeft (120)); row.removeFromLeft (10); }
    }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (bg1);
        if (tab == 5)
        {
            auto c = table.getBounds().reduced (20, 20).toFloat();
            label (g, U ("Monitor chiqishi"), { c.getX(), c.getY() + 48, 170, 30 }, 12, silver);
            label (g, U ("Monitor balandligi"), { c.getX(), c.getY() + 94, 170, 30 }, 12, silver);
            label (g, U ("Control Room faqat monitorlarda eshitiladi: eksport va bounce'ga ta'sir qilmaydi."),
                   { c.getX(), c.getY() + 200, c.getWidth(), 20 }, 11, dim, juce::Justification::centredLeft, 400);
        }
    }

private:
    void timerCallback() override { if (tab == 0 || tab == 1) rebuild(); }

    void rebuild()
    {
        for (int i = 0; i < tabB.size(); ++i) tabB[i]->setToggleState (i == tab, juce::dontSendNotification);
        auto& dm = app.engine.getDeviceManager();
        auto* dev = dm.deviceManager.getCurrentAudioDevice();
        const juce::String devName = dev ? dev->getName() : U ("Audio yo'q");
        const int keepSel = table.selected;
        table.rows.clear(); table.selected = -1;
        const bool cr = tab == 5;
        table.setVisible (! cr);
        for (auto* c : std::initializer_list<juce::Component*> { &crOn, &crOut, &crLevel, &crDim, &crMono, &crMute }) c->setVisible (cr);
        addB.setVisible (tab >= 2 && tab <= 4); removeB.setVisible (tab >= 2 && tab <= 4);
        setupB.setVisible (tab <= 1 || tab == 5);
        switch (tab)
        {
            case 0:
                table.columns = { "Bus Name", "Speakers", "Audio Device", "Device Port" }; table.widths = { 0.3f, 0.15f, 0.3f, 0.25f };
                for (int i = 0; i < dm.getNumWaveInDevices(); ++i)
                    if (auto* w = dm.getWaveInDevice (i))
                        table.rows.add ({ { w->getAliasOrName(), w->isStereoPair() ? "Stereo" : "Mono", devName, w->getName() }, w->isEnabled(), true });
                break;
            case 1:
                table.columns = { "Bus Name", "Speakers", "Audio Device", "Device Port" }; table.widths = { 0.3f, 0.15f, 0.3f, 0.25f };
                for (int i = 0; i < dm.getNumWaveOutDevices(); ++i)
                    if (auto* w = dm.getWaveOutDevice (i))
                    {
                        const bool main = w->getDeviceID() == dm.getDefaultWaveOutDeviceID();
                        table.rows.add ({ { w->getAliasOrName() + (main ? "  (Main Mix)" : ""), w->isStereoPair() ? "Stereo" : "Mono", devName, w->getName() }, w->isEnabled(), true, main });
                    }
                break;
            case 2:
                table.columns = { "FX / Group", "Bus", U ("Effekt"), "Sends" }; table.widths = { 0.35f, 0.1f, 0.35f, 0.2f };
                addB.setButtonText (U ("+ FX kanal qo'shish")); removeB.setButtonText (U ("O'chirish"));
                for (auto* t : rt.fxTracks())
                {
                    int n = 0;
                    for (auto* s : te::getAudioTracks (*app.edit)) for (auto* snd : Routing::sends (*s)) if (snd->getBusNumber() == Routing::fxBus (*t)) ++n;
                    auto ps = AppEngine::userPlugins (*t);
                    table.rows.add ({ { t->getName(), juce::String (Routing::fxBus (*t) + 1), ps.isEmpty() ? juce::String ("-") : pluginDisplayName (*ps[0]), juce::String (n) }, true, true });
                }
                break;
            case 3: case 4:
            {
                const juce::String kind = tab == 3 ? "fx" : "inst";
                table.columns = { "Name", tab == 3 ? "Send (Out)" : "MIDI / Out", "Return (In)", "" }; table.widths = { 0.3f, 0.3f, 0.3f, 0.1f };
                addB.setButtonText (tab == 3 ? U ("+ External FX qo'shish") : U ("+ External instrument")); removeB.setButtonText (U ("O'chirish"));
                for (auto& d : rt.externals()) if (d.kind == kind) table.rows.add ({ { d.name, d.out, d.in, "" }, true, true });
                break;
            }
            case 5:
            {
                if (auto* c = rt.controlRoom())
                {
                    crOn.setToggleState (c->isEnabled(), juce::dontSendNotification);
                    crLevel.setValue (c->level.get(), juce::dontSendNotification);
                    crDim.setToggleState (c->dim.get(), juce::dontSendNotification);
                    crMono.setToggleState (c->mono.get(), juce::dontSendNotification);
                    crMute.setToggleState (c->mute.get(), juce::dontSendNotification);
                }
                crOut.clear (juce::dontSendNotification);
                for (int i = 0; i < dm.getNumWaveOutDevices(); ++i)
                    if (auto* w = dm.getWaveOutDevice (i))
                    {
                        crOut.addItem (w->getAliasOrName() + U (" \xc2\xb7 ") + w->getName(), i + 1);
                        if (w->getDeviceID() == dm.getDefaultWaveOutDeviceID()) crOut.setSelectedId (i + 1, juce::dontSendNotification);
                    }
                break;
            }
        }
        if (keepSel < table.rows.size()) table.selected = keepSel;
        resized(); table.repaint(); repaint();
    }

    void clicked (int r, int c, const juce::MouseEvent&)
    {
        auto& dm = app.engine.getDeviceManager();
        if (tab == 0 && c == 0) if (auto* w = dm.getWaveInDevice (r)) { w->setEnabled (! w->isEnabled()); if (app.edit) app.setupInputs(); rebuild(); }
        if (tab == 1 && c == 0) if (auto* w = dm.getWaveOutDevice (r)) { dm.setDefaultWaveOutDevice (w->getDeviceID()); rebuild(); }
    }
    void doubleClicked (int r, int c)
    {
        auto& dm = app.engine.getDeviceManager();
        if ((tab == 0 || tab == 1) && c == 0)    // Bus nomini o'zgartirish
        {
            te::IODevice* d = tab == 0 ? (te::IODevice*) dm.getWaveInDevice (r) : (te::IODevice*) dm.getWaveOutDevice (r);
            if (d == nullptr) return;
            auto* w = new juce::AlertWindow ("Bus Name", {}, juce::MessageBoxIconType::NoIcon);
            w->addTextEditor ("n", d->getAliasOrName());
            w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey)); w->addButton (U ("Bekor"), 0);
            w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w, d] (int res) { if (res) d->setAlias (w->getTextEditorContents ("n")); rebuild(); if (onChanged) onChanged(); }), true);
        }
        if (tab == 2) if (auto* t = rt.fxTracks()[r]) { auto ps = AppEngine::userPlugins (*t); if (! ps.isEmpty()) ps[0]->showWindowExplicitly(); }
    }

    void add()
    {
        if (tab == 2)
        {
            showPluginPicker (app, localAreaToGlobal (addB.getBounds()), [this] (const PickItem& it)
            {
                juce::String xml; juce::PluginDescription d;
                if (it.kind == 0) xml = builtIns[it.index].xmlType;
                else if (it.kind == 1) xml = airwindowsPlugins[it.index].xmlType;
                else if (it.kind == 2) { xml = te::ExternalPlugin::xmlTypeName; d = app.engine.getPluginManager().knownPluginList.getTypes()[it.index]; }
                rt.addFxChannel (it.name, xml, d);
                rebuild(); if (onChanged) onChanged();
            });
            return;
        }
        // External FX / Instrument: nomi, chiqish (send) va kirish (return)
        const bool inst = tab == 4;
        auto* w = new juce::AlertWindow (inst ? "External Instrument" : "External FX", U ("Apparat qurilmani ulash: send (chiqish) va return (kirish) portlarini tanlang"), juce::MessageBoxIconType::NoIcon);
        w->addTextEditor ("name", inst ? "Synth (hardware)" : "Hardware FX", U ("Nomi"));
        auto outs = rt.deviceNames (false), ins = rt.deviceNames (true);
        w->addComboBox ("out", outs, inst ? "MIDI / Out" : "Send (Out)");
        w->addComboBox ("in", ins, "Return (In)");
        w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey)); w->addButton (U ("Bekor"), 0);
        w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w, inst] (int res)
        {
            if (! res) return;
            auto list = rt.externals();
            list.add ({ inst ? "inst" : "fx", w->getTextEditorContents ("name"), w->getComboBoxComponent ("out")->getText(), w->getComboBoxComponent ("in")->getText() });
            rt.setExternals (list); rebuild();
        }), true);
    }
    void remove()
    {
        const int r = table.selected; if (r < 0) return;
        if (tab == 2) { if (auto* t = rt.fxTracks()[r]) { app.edit->getUndoManager().beginNewTransaction ("FX"); app.edit->deleteTrack (t); } }
        if (tab == 3 || tab == 4)
        {
            const juce::String kind = tab == 3 ? "fx" : "inst";
            auto list = rt.externals(); int k = -1;
            for (int i = 0; i < list.size(); ++i) if (list[i].kind == kind && ++k == r) { list.remove (i); break; }
            rt.setExternals (list);
        }
        rebuild(); if (onChanged) onChanged();
    }

    AppEngine& app; Routing rt;
    juce::OwnedArray<juce::TextButton> tabB;
    juce::TextButton addB, removeB, setupB, crDim, crMono, crMute, crOn;
    juce::Slider crLevel; juce::ComboBox crOut;
    ConnTable table; int tab = 0;
};

class ConnectionsWindow : public juce::DocumentWindow
{
public:
    ConnectionsWindow (AppEngine& a, std::function<void()> onClose, std::function<void()> deviceSetup, std::function<void()> changed)
        : DocumentWindow (U ("Audio ulanishlar") + "  \xc2\xb7  AZ MUS", bg1, DocumentWindow::closeButton), closed (std::move (onClose))
    {
        setUsingNativeTitleBar (true);
        auto* c = new ConnectionsComponent (a);
        c->onDeviceSetup = std::move (deviceSetup); c->onChanged = std::move (changed);
        setContentOwned (c, true);
        setResizable (true, false);
        centreWithSize (900, 500);
        setVisible (true);
    }
    void closeButtonPressed() override { if (closed) closed(); }
    std::function<void()> closed;
};
}
