// =============================================================================
//  AZ MUS — Piano roll (MIDI muharrir)
//  Bo'sh joy: nota qo'yish · sudrash: ko'chirish · o'ng chet: uzunlik
//  O'ng tugma / 2x bosish: o'chirish · Ctrl+g'ildirak: zoom · Shift+g'ildirak: gorizontal
// =============================================================================
#pragma once
#include "Arrangement.h"

namespace az
{
class PianoRollView : public juce::Component
{
public:
    PianoRollView (te::MidiClip& c) : clip (&c) { setWantsKeyboardFocus (true); }

    static constexpr int keyW = 56;
    float beatW = 96.0f, noteH = 14.0f; int topNote = 84; double gridBeats = 0.25;

    float beatToX (double b) const { return keyW + (float) (b - scrollBeat) * beatW; }
    double xToBeat (float x) const { return scrollBeat + (double) (x - keyW) / beatW; }
    float noteToY (int n) const    { return (float) (topNote - n) * noteH; }
    int yToNote (float y) const    { return topNote - (int) std::floor (y / noteH); }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        g.fillAll (bg0);
        if (clip == nullptr) return;
        const float W = (float) getWidth(), H = (float) getHeight();
        const double len = clip->getLengthInBeats().inBeats();
        // qatorlar
        for (int n = topNote; noteToY (n) < H; --n)
        {
            const float y = noteToY (n);
            const bool black = MidiMessage::isMidiNoteBlack (n);
            g.setColour (black ? Colours::white.withAlpha (0.015f) : Colours::white.withAlpha (0.035f)); g.fillRect ((float) keyW, y, W, noteH);
            g.setColour (Colours::white.withAlpha (n % 12 == 0 ? 0.12f : 0.04f)); g.fillRect ((float) keyW, y + noteH - 1, W, 1.0f);
            // klaviatura
            g.setColour (black ? Colour (0xff1b181c) : Colour (0xffd9d3d4)); g.fillRect (0.0f, y, (float) keyW - 2, noteH - 1);
            if (n % 12 == 0) label (g, MidiMessage::getMidiNoteName (n, true, true, 3), { 4, y, (float) keyW - 10, noteH }, 9, Colour (0xff3a3335), Justification::centredRight, 600);
        }
        // grid
        for (double b = std::floor (scrollBeat); beatToX (b) < W; b += gridBeats)
        {
            const bool bar = std::fmod (b, 4.0) == 0.0, beat = std::fmod (b, 1.0) == 0.0;
            g.setColour (Colours::white.withAlpha (bar ? 0.16f : beat ? 0.08f : 0.03f)); g.fillRect (beatToX (b), 0.0f, 1.0f, H);
        }
        // klip chegarasi
        g.setColour (Colours::black.withAlpha (0.45f)); g.fillRect (beatToX (len), 0.0f, W, H);
        // notalar
        for (auto* n : clip->getSequence().getNotes())
        {
            auto r = noteRect (*n);
            const float v = (float) n->getVelocity() / 127.0f;
            g.setColour (red.interpolatedWith (Colour (0xffffb0a0), 1.0f - v).withAlpha (0.9f)); g.fillRoundedRectangle (r, 3);
            g.setColour (Colours::white.withAlpha (selected.contains (n) ? 0.9f : 0.25f)); g.drawRoundedRectangle (r, 3, selected.contains (n) ? 1.5f : 1.0f);
        }
        caps (g, U ("PIANO ROLL \xc2\xb7 ") + clip->getName().toUpperCase() + U ("  \xc2\xb7  GRID 1/") + String ((int) std::round (4.0 / gridBeats)), { (float) keyW + 10, 6, 600, 14 }, 9.5f, dim);
    }

    juce::Rectangle<float> noteRect (te::MidiNote& n) const
    {
        const float x1 = beatToX (n.getStartBeat().inBeats()), x2 = beatToX (n.getEndBeat().inBeats());
        return { x1 + 0.5f, noteToY (n.getNoteNumber()) + 1, juce::jmax (3.0f, x2 - x1 - 1), noteH - 2 };
    }
    te::MidiNote* noteAt (juce::Point<float> p) const
    {
        for (auto* n : clip->getSequence().getNotes()) if (noteRect (*n).contains (p)) return n;
        return nullptr;
    }
    double snapB (double b) const { return std::round (b / gridBeats) * gridBeats; }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (clip == nullptr || e.x < keyW) { previewKey (e); return; }
        auto* um = &clip->edit.getUndoManager();
        um->beginNewTransaction ("Nota");
        if (auto* n = noteAt (e.position))
        {
            if (e.mods.isPopupMenu()) { clip->getSequence().removeNote (*n, um); selected.clear(); repaint(); return; }
            if (! e.mods.isShiftDown()) selected.clear();
            selected.addIfNotAlreadyThere (n);
            dragNote = n; dragStart = n->getStartBeat().inBeats(); dragPitch = n->getNoteNumber(); dragLen = n->getLengthBeats().inBeats();
            resizing = e.position.x > noteRect (*n).getRight() - 6;
        }
        else if (! e.mods.isPopupMenu())
        {
            const double b = std::floor (xToBeat (e.position.x) / gridBeats) * gridBeats;
            auto* n = clip->getSequence().addNote (yToNote (e.position.y), te::BeatPosition::fromBeats (juce::jmax (0.0, b)),
                                                   te::BeatDuration::fromBeats (lastLen), 100, 0, um);
            selected.clear(); selected.add (n);
            dragNote = n; dragStart = n->getStartBeat().inBeats(); dragPitch = n->getNoteNumber(); dragLen = lastLen; resizing = false;
        }
        repaint();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragNote == nullptr) return;
        auto* um = &clip->edit.getUndoManager();
        const double db = (double) e.getDistanceFromDragStartX() / beatW;
        if (resizing)
        {
            const double l = juce::jmax (gridBeats, snapB (dragLen + db));
            dragNote->setStartAndLength (dragNote->getStartBeat(), te::BeatDuration::fromBeats (l), um); lastLen = l;
        }
        else
        {
            const double s = juce::jmax (0.0, snapB (dragStart + db));
            const int p = juce::jlimit (0, 127, dragPitch - (int) std::round ((float) e.getDistanceFromDragStartY() / noteH));
            dragNote->setStartAndLength (te::BeatPosition::fromBeats (s), dragNote->getLengthBeats(), um);
            dragNote->setNoteNumber (p, um);
        }
        repaint();
    }
    void mouseUp (const juce::MouseEvent&) override { dragNote = nullptr; }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (auto* n = noteAt (e.position)) { clip->getSequence().removeNote (*n, &clip->edit.getUndoManager()); selected.clear(); repaint(); }
    }
    void mouseMove (const juce::MouseEvent& e) override
    {
        auto* n = noteAt (e.position);
        setMouseCursor (n && e.position.x > noteRect (*n).getRight() - 6 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
    }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (e.mods.isCommandDown()) beatW = juce::jlimit (16.0f, 600.0f, beatW * (w.deltaY > 0 ? 1.15f : 1 / 1.15f));
        else if (e.mods.isShiftDown()) scrollBeat = juce::jmax (0.0, scrollBeat - w.deltaY * 8.0);
        else topNote = juce::jlimit (20, 127, topNote + (w.deltaY > 0 ? 2 : -2));
        repaint();
    }
    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey)
        {
            for (auto* n : selected) clip->getSequence().removeNote (*n, &clip->edit.getUndoManager());
            selected.clear(); repaint(); return true;
        }
        if (k.getTextCharacter() >= '1' && k.getTextCharacter() <= '4')
        {
            const double g[] = { 1.0, 0.5, 0.25, 0.125 }; gridBeats = g[k.getTextCharacter() - '1']; repaint(); return true;
        }
        if (k == juce::KeyPress ('q', juce::ModifierKeys::noModifiers, 0))    // kvantlash
        {
            for (auto* n : clip->getSequence().getNotes())
                n->setStartAndLength (te::BeatPosition::fromBeats (snapB (n->getStartBeat().inBeats())), n->getLengthBeats(), &clip->edit.getUndoManager());
            repaint(); return true;
        }
        return false;
    }
    void previewKey (const juce::MouseEvent&) {}

    te::MidiClip::Ptr clip;
private:
    double scrollBeat = 0, dragStart = 0, dragLen = 0.25, lastLen = 0.25; int dragPitch = 60; bool resizing = false;
    te::MidiNote* dragNote = nullptr; juce::Array<te::MidiNote*> selected;
};

class PianoRollWindow : public juce::DocumentWindow
{
public:
    PianoRollWindow (te::MidiClip& c, std::function<void()> onClose)
        : DocumentWindow ("Piano Roll  \xc2\xb7  " + c.getName() + "  \xc2\xb7  AZ MUS", bg0, DocumentWindow::allButtons), closed (std::move (onClose))
    {
        setUsingNativeTitleBar (true);
        setContentOwned (new PianoRollView (c), false);
        setResizable (true, false);
        centreWithSize (1100, 560);
        setVisible (true);
    }
    void closeButtonPressed() override { if (closed) closed(); }
    std::function<void()> closed;
};
}
