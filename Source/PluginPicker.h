// =============================================================================
//  AZ MUS — plagin qidiruvi (bosh harf / nom / ishlab chiqaruvchi bo'yicha)
//  ↑ ↓ — tanlash · Enter — qo'yish · Esc — yopish · Tab — kategoriya
// =============================================================================
#pragma once
#include "Routing.h"
#include "AirwindowsList.h"

namespace az
{
struct PickItem
{
    juce::String name, vendor, category;  // category: AZ / Airwindows / VST3 / Instrument / External
    int kind = 0;                          // 0 builtin, 1 airwindows, 2 vst3, 3 external
    int index = 0;
    bool instrument = false;
};

class PluginPicker : public juce::Component, private juce::ListBoxModel, private juce::TextEditor::Listener, private juce::KeyListener
{
public:
    std::function<void (const PickItem&)> onPick;

    PluginPicker (AppEngine& a, bool instrumentsFirst = false) : app (a)
    {
        for (int i = 0; i < (int) std::size (builtIns); ++i)
            all.add ({ juce::String::fromUTF8 (builtIns[i].name), "AZ", builtIns[i].synth ? "Instrument" : "AZ", 0, i, builtIns[i].synth });
        for (int i = 0; i < (int) std::size (airwindowsPlugins); ++i)
            all.add ({ airwindowsPlugins[i].name, "Airwindows", "Airwindows", 1, i, false });
        auto types = app.engine.getPluginManager().knownPluginList.getTypes();
        for (int i = 0; i < types.size(); ++i)
            all.add ({ types[i].name, types[i].manufacturerName, types[i].isInstrument ? "Instrument" : "VST3", 2, i, types[i].isInstrument });
        Routing rt (app);
        auto ext = rt.externals();
        for (int i = 0; i < ext.size(); ++i)
            all.add ({ ext[i].name, ext[i].kind == "inst" ? U ("Tashqi instrument") : U ("Tashqi FX"), "External", 3, i, ext[i].kind == "inst" });
        category = instrumentsFirst ? 4 : 0;

        search.setTextToShowWhenEmpty (U ("Qidirish: nomi yoki bosh harfi..."), dim);
        search.setFont (font (14));
        search.setIndents (30, 7);
        search.addListener (this);
        search.addKeyListener (this);
        addAndMakeVisible (search);
        const char* cats[] = { "Hammasi", "AZ", "Airwindows", "VST3", "Instrument", "External" };
        for (int i = 0; i < 6; ++i)
        {
            auto* b = catB.add (new juce::TextButton (cats[i]));
            b->getProperties().set ("radius", 5.0f);
            b->onClick = [this, i] { category = i; refilter(); };
            addAndMakeVisible (b);
        }
        list.setModel (this); list.setRowHeight (28);
        list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        list.addKeyListener (this);
        addAndMakeVisible (list);
        setSize (440, 520);
        refilter();
    }
    void visibilityChanged() override { if (isShowing()) search.grabKeyboardFocus(); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10);
        search.setBounds (r.removeFromTop (34));
        r.removeFromTop (8);
        auto row = r.removeFromTop (24);
        const int w = row.getWidth() / 6;
        for (auto* b : catB) b->setBounds (row.removeFromLeft (w).reduced (2, 0));
        r.removeFromTop (8);
        list.setBounds (r);
    }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff121014));
        auto s = search.getBounds().toFloat();
        LookAndFeel::drawIcon (g, "#search", { s.getX() + 4, s.getY() + 5, 24, 24 }, dim);
        label (g, juce::String (shown.size()) + " / " + juce::String (all.size()), getLocalBounds().toFloat().reduced (14, 6).removeFromBottom (14), 9.5f, dim2, juce::Justification::centredRight);
    }

private:
    int getNumRows() override { return shown.size(); }
    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool sel) override
    {
        if (! juce::isPositiveAndBelow (row, shown.size())) return;
        auto& it = all.getReference (shown[row]);
        if (sel) { g.setColour (red.withAlpha (0.18f)); g.fillRoundedRectangle (2, 1, (float) w - 4, (float) h - 2, 5); }
        const juce::Colour cc = it.category == "AZ" ? red : it.category == "Airwindows" ? blue : it.category == "Instrument" ? yellow : it.category == "External" ? violet : green;
        g.setColour (cc); g.fillRoundedRectangle (10, (float) h * 0.5f - 3, 6, 6, 3);
        // mos keladigan qismni ajratib ko'rsatish
        const auto q = search.getText().trim();
        const int at = q.isEmpty() ? -1 : it.name.indexOfIgnoreCase (q);
        g.setFont (font (13, 500));
        float x = 24;
        auto draw = [&] (const juce::String& s, juce::Colour c) { g.setColour (c); const float tw = textWidth (font (13, 500), s); g.drawText (s, juce::Rectangle<float> (x, 0, tw + 2, (float) h), juce::Justification::centredLeft); x += tw; };
        if (at >= 0) { draw (it.name.substring (0, at), text); draw (it.name.substring (at, at + q.length()), redSoft); draw (it.name.substring (at + q.length()), text); }
        else draw (it.name, text);
        label (g, it.vendor + juce::String::fromUTF8 ("  \xc2\xb7  ") + it.category, { (float) w - 190, 0, 180, (float) h }, 10, dim, juce::Justification::centredRight, 400);
    }
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override { pick (row); }
    void returnKeyPressed (int row) override { pick (row); }

    void textEditorTextChanged (juce::TextEditor&) override { refilter(); }
    void textEditorReturnKeyPressed (juce::TextEditor&) override { pick (juce::jmax (0, list.getSelectedRow())); }
    void textEditorEscapeKeyPressed (juce::TextEditor&) override { close(); }

    bool keyPressed (const juce::KeyPress& k, juce::Component*) override
    {
        if (k == juce::KeyPress::downKey) { list.selectRow (juce::jmin (shown.size() - 1, list.getSelectedRow() + 1)); return true; }
        if (k == juce::KeyPress::upKey)   { list.selectRow (juce::jmax (0, list.getSelectedRow() - 1)); return true; }
        if (k == juce::KeyPress::tabKey)  { category = (category + 1) % 6; refilter(); return true; }
        if (k == juce::KeyPress::escapeKey) { close(); return true; }
        if (k == juce::KeyPress::returnKey) { pick (juce::jmax (0, list.getSelectedRow())); return true; }
        return false;
    }

    void refilter()
    {
        const auto q = search.getText().trim().toLowerCase();
        const char* cats[] = { "", "AZ", "Airwindows", "VST3", "Instrument", "External" };
        struct Sc { int idx, score; };
        std::vector<Sc> v;
        for (int i = 0; i < all.size(); ++i)
        {
            auto& it = all.getReference (i);
            if (category > 0 && it.category != cats[category]) continue;
            int score = 0;
            if (q.isNotEmpty())
            {
                const auto n = it.name.toLowerCase();
                // bosh harflar ("vr" -> "Valhalla Room", "az v" -> "AZ VERB")
                juce::String initials; bool start = true;
                for (auto c : n) { if (start && juce::CharacterFunctions::isLetterOrDigit (c)) initials += c; start = ! juce::CharacterFunctions::isLetterOrDigit (c); }
                if (n.startsWith (q)) score = 0;
                else if (n.contains (" " + q) || n.contains ("-" + q)) score = 1;
                else if (initials.startsWith (q.removeCharacters (" "))) score = 2;
                else if (n.contains (q)) score = 3;
                else if (it.vendor.toLowerCase().contains (q)) score = 4;
                else continue;
            }
            v.push_back ({ i, score });
        }
        std::stable_sort (v.begin(), v.end(), [this] (const Sc& a, const Sc& b)
        {
            if (a.score != b.score) return a.score < b.score;
            return all.getReference (a.idx).name.compareIgnoreCase (all.getReference (b.idx).name) < 0;
        });
        shown.clearQuick();
        for (auto& s : v) shown.add (s.idx);
        for (int i = 0; i < catB.size(); ++i) catB[i]->setToggleState (i == category, juce::dontSendNotification);
        list.updateContent(); list.selectRow (0); repaint();
    }
    void pick (int row)
    {
        if (! juce::isPositiveAndBelow (row, shown.size())) return;
        auto it = all[shown[row]];
        auto cb = onPick;
        close();
        if (cb) cb (it);
    }
    void close() { if (auto* box = findParentComponentOfClass<juce::CallOutBox>()) box->dismiss(); }

    AppEngine& app;
    juce::TextEditor search; juce::ListBox list;
    juce::OwnedArray<juce::TextButton> catB;
    juce::Array<PickItem> all; juce::Array<int> shown; int category = 0;
};

// Tanlangan elementni trekka qo'yish
inline te::Plugin::Ptr insertPicked (AppEngine& app, te::AudioTrack& t, const PickItem& it)
{
    if (it.kind == 0) return app.insertPlugin (t, builtIns[it.index].xmlType, {});
    if (it.kind == 1) return app.insertPlugin (t, airwindowsPlugins[it.index].xmlType, {});
    if (it.kind == 2) return app.insertPlugin (t, te::ExternalPlugin::xmlTypeName, app.engine.getPluginManager().knownPluginList.getTypes()[it.index]);
    Routing rt (app);
    return rt.insertExternal (t, rt.externals()[it.index]);
}

inline void showPluginPicker (AppEngine& app, juce::Rectangle<int> screenArea, std::function<void (const PickItem&)> onPick, bool instruments = false)
{
    auto p = std::make_unique<PluginPicker> (app, instruments);
    p->onPick = std::move (onPick);
    juce::CallOutBox::launchAsynchronously (std::move (p), screenArea, nullptr);
}
}
