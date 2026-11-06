// =============================================================================
//  AZ MUS — vizual uslub (splash va AZ VERB bilan bir xil brend)
// =============================================================================
#pragma once
#include <JuceHeader.h>

namespace te = tracktion;
using namespace std::literals;

namespace az
{
    const juce::Colour bg0 { 0xff07070a }, bg1 { 0xff0d0c0f }, bg2 { 0xff121116 }, bg3 { 0xff17151b };
    const juce::Colour red { 0xffff3347 }, redHot { 0xffff2a3d }, redSoft { 0xffff8a95 }, redDeep { 0xff9a1020 };
    const juce::Colour text { 0xffe9e6e6 }, dim { 0xff8d8587 }, dim2 { 0xff5d5759 }, silver { 0xffcfc8c9 };
    const juce::Colour green { 0xff3ddc97 }, yellow { 0xffffc04a }, blue { 0xff8ab4ff }, violet { 0xffb98aff };
    const juce::Colour line = juce::Colours::white.withAlpha (0.06f);

    inline const juce::Colour trackColours[] = { red, yellow, blue, green, violet, redSoft, juce::Colour (0xff4ad6ff), juce::Colour (0xffff9a4a) };

    inline juce::Font font (float h, int weight = 400, float kern = 0.0f)
    {
        static juce::Typeface::Ptr reg  = juce::Typeface::createSystemTypefaceFor (BinaryData::InterRegular_otf,  BinaryData::InterRegular_otfSize);
        static juce::Typeface::Ptr med  = juce::Typeface::createSystemTypefaceFor (BinaryData::InterMedium_otf,   BinaryData::InterMedium_otfSize);
        static juce::Typeface::Ptr semi = juce::Typeface::createSystemTypefaceFor (BinaryData::InterSemiBold_otf, BinaryData::InterSemiBold_otfSize);
        static juce::Typeface::Ptr bold = juce::Typeface::createSystemTypefaceFor (BinaryData::InterBold_otf,     BinaryData::InterBold_otfSize);
        auto tf = weight >= 700 ? bold : weight >= 600 ? semi : weight >= 500 ? med : reg;
        return juce::Font (juce::FontOptions().withTypeface (tf).withHeight (h)).withExtraKerningFactor (kern);
    }

    inline float textWidth (const juce::Font& f, const juce::String& s)
    {
        juce::GlyphArrangement ga; ga.addLineOfText (f, s, 0, 0);
        return ga.getBoundingBox (0, -1, true).getWidth();
    }

    inline void label (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r, float h, juce::Colour c,
                       juce::Justification j = juce::Justification::centredLeft, int w = 500, float kern = 0.0f)
    {
        g.setColour (c); g.setFont (font (h, w, kern)); g.drawText (s, r, j, true);
    }
    inline void caps (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r, float h = 9.0f, juce::Colour c = dim2,
                      juce::Justification j = juce::Justification::centredLeft)
    {
        label (g, s, r, h, c, j, 500, 0.22f);
    }

    inline juce::String U (const char* s) { return juce::String::fromUTF8 (s); }

    inline void panel (juce::Graphics& g, juce::Rectangle<float> r, float rad = 8.0f, float fillA = 0.025f, float strokeA = 0.07f)
    {
        g.setColour (juce::Colours::white.withAlpha (fillA)); g.fillRoundedRectangle (r, rad);
        g.setColour (juce::Colours::white.withAlpha (strokeA)); g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.0f);
    }

    inline void logoMark (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour (redHot.withAlpha (0.22f)); g.fillRoundedRectangle (r.expanded (r.getWidth() * 0.12f), r.getWidth() * 0.34f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffff5a3a), r.getX(), r.getY(), redDeep, r.getRight(), r.getBottom(), false));
        g.fillRoundedRectangle (r, r.getWidth() * 0.27f);
        label (g, "AZ", r, r.getHeight() * 0.4f, juce::Colours::white, juce::Justification::centred, 700);
    }

    // ---------------------------------------------------------------------------
    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel()
        {
            using namespace juce;
            setColourScheme ({ bg1, bg0, bg2, Colour (0xff2a2629), text, red, Colours::white, bg2, text });
            setColour (ResizableWindow::backgroundColourId, bg0);
            setColour (PopupMenu::backgroundColourId, Colour (0xff141216));
            setColour (PopupMenu::textColourId, text);
            setColour (PopupMenu::highlightedBackgroundColourId, redHot.withAlpha (0.25f));
            setColour (PopupMenu::highlightedTextColourId, Colours::white);
            setColour (PopupMenu::headerTextColourId, redSoft);
            setColour (TextEditor::backgroundColourId, Colours::white.withAlpha (0.04f));
            setColour (TextEditor::outlineColourId, Colours::white.withAlpha (0.1f));
            setColour (TextEditor::focusedOutlineColourId, red.withAlpha (0.6f));
            setColour (TextEditor::textColourId, text);
            setColour (TextEditor::highlightColourId, red.withAlpha (0.3f));
            setColour (CaretComponent::caretColourId, red);
            setColour (Label::textColourId, text);
            setColour (ComboBox::backgroundColourId, Colours::white.withAlpha (0.04f));
            setColour (ComboBox::outlineColourId, Colours::white.withAlpha (0.1f));
            setColour (ComboBox::textColourId, text);
            setColour (ComboBox::arrowColourId, dim);
            setColour (ListBox::backgroundColourId, bg1);
            setColour (ListBox::textColourId, silver);
            setColour (TreeView::backgroundColourId, Colours::transparentBlack);
            setColour (TreeView::selectedItemBackgroundColourId, Colours::white.withAlpha (0.06f));
            setColour (DirectoryContentsDisplayComponent::highlightColourId, Colours::white.withAlpha (0.07f));
            setColour (DirectoryContentsDisplayComponent::textColourId, silver);
            setColour (DirectoryContentsDisplayComponent::highlightedTextColourId, Colours::white);
            setColour (ScrollBar::thumbColourId, Colours::white.withAlpha (0.14f));
            setColour (TextButton::buttonColourId, Colours::white.withAlpha (0.04f));
            setColour (TextButton::textColourOffId, silver);
            setColour (TextButton::buttonOnColourId, redHot.withAlpha (0.2f));
            setColour (TextButton::textColourOnId, redSoft);
            setColour (ToggleButton::textColourId, silver);
            setColour (ToggleButton::tickColourId, red);
            setColour (Slider::thumbColourId, Colours::white);
            setColour (Slider::trackColourId, red);
            setColour (AlertWindow::backgroundColourId, Colour (0xff121014));
            setColour (AlertWindow::textColourId, text);
            setColour (DocumentWindow::textColourId, text);
        }

        juce::Font getPopupMenuFont() override                              { return font (14.0f); }
        juce::Font getMenuBarFont (juce::MenuBarComponent&, int, const juce::String&) override { return font (13.0f, 400); }
        juce::Font getTextButtonFont (juce::TextButton&, int h) override    { return font (juce::jmin (13.0f, h * 0.55f), 500); }
        juce::Font getLabelFont (juce::Label& l) override                   { return font (l.getFont().getHeight(), 400); }
        juce::Font getComboBoxFont (juce::ComboBox&) override               { return font (13.0f); }
        juce::Font getAlertWindowMessageFont() override                     { return font (14.0f); }
        juce::Font getAlertWindowTitleFont() override                       { return font (17.0f, 700); }
        juce::Typeface::Ptr getTypefaceForFont (const juce::Font& f) override
        {
            const bool b = f.isBold();
            return font (f.getHeight(), b ? 700 : 400).getTypefacePtr();
        }

        void drawMenuBarBackground (juce::Graphics&, int, int, bool, juce::MenuBarComponent&) override {}
        void drawMenuBarItem (juce::Graphics& g, int w, int h, int, const juce::String& t, bool over, bool open, bool, juce::MenuBarComponent&) override
        {
            if (over || open) { g.setColour (juce::Colours::white.withAlpha (0.06f)); g.fillRoundedRectangle (1, 4, (float) w - 2, (float) h - 8, 6); }
            label (g, t, { 0, 0, (float) w, (float) h }, 13.0f, (over || open) ? juce::Colours::white : juce::Colour (0xffa49c9e), juce::Justification::centred, 400);
        }
        int getMenuBarItemWidth (juce::MenuBarComponent&, int, const juce::String& t) override { return (int) textWidth (font (13.0f), t) + 18; }

        void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
        {
            g.fillAll (juce::Colour (0xff141216));
            g.setColour (juce::Colours::white.withAlpha (0.08f)); g.drawRect (0, 0, w, h);
        }

        void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
        {
            using namespace juce;
            const auto accent = s.findColour (Slider::rotarySliderFillColourId);
            const float sz = (float) jmin (w, h), cx = (float) x + (float) w * 0.5f, cy = (float) y + (float) h * 0.5f;
            const float R = sz * 0.42f, th = jmax (2.5f, sz * 0.075f), ang = a0 + pos * (a1 - a0);
            Path tr; tr.addCentredArc (cx, cy, R, R, 0, a0, a1, true);
            g.setColour (Colours::white.withAlpha (0.08f)); g.strokePath (tr, PathStrokeType (th, PathStrokeType::curved, PathStrokeType::rounded));
            const bool bip = s.getProperties()["bipolar"];
            Path v; v.addCentredArc (cx, cy, R, R, 0, bip ? (a0 + a1) * 0.5f : a0, ang, true);
            g.setColour (accent); g.strokePath (v, PathStrokeType (th, PathStrokeType::curved, PathStrokeType::rounded));
            const float br = sz * 0.28f;
            g.setGradientFill (ColourGradient (Colour (0xff3a3335), cx - br * 0.3f, cy - br * 0.6f, Colour (0xff151213), cx + br * 0.4f, cy + br, true));
            g.fillEllipse (cx - br, cy - br, br * 2, br * 2);
            g.setColour (Colours::white);
            g.drawLine (cx + std::sin (ang) * br * 0.2f, cy - std::cos (ang) * br * 0.2f, cx + std::sin (ang) * br * 0.75f, cy - std::cos (ang) * br * 0.75f, jmax (1.5f, sz * 0.045f));
        }

        void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle st, juce::Slider& s) override
        {
            using namespace juce;
            if (st != Slider::LinearVertical) { LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0, 0, st, s); return; }
            const float cx = (float) x + (float) w * 0.5f;
            g.setColour (Colours::white.withAlpha (0.08f)); g.fillRoundedRectangle (cx - 2, (float) y, 4, (float) h, 2);
            // 0 dB belgisi
            if (s.getMaximum() > 0.0 && s.getMinimum() < 0.0)
            {
                const float zy = (float) s.getPositionOfValue (0.0);
                g.setColour (Colours::white.withAlpha (0.25f)); g.drawHorizontalLine ((int) zy, cx - 9, cx + 9);
            }
            auto cap = Rectangle<float> (cx - 10, pos - 6, 20, 12);
            g.setColour (Colours::black.withAlpha (0.6f)); g.fillRoundedRectangle (cap.translated (0, 2), 3);
            g.setGradientFill (ColourGradient (Colour (0xffdcd6d7), 0, cap.getY(), Colour (0xff8d8587), 0, cap.getBottom(), false));
            g.fillRoundedRectangle (cap, 3);
            g.setColour (Colours::black.withAlpha (0.4f)); g.drawHorizontalLine ((int) cap.getCentreY(), cap.getX() + 3, cap.getRight() - 3);
        }

        void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
        {
            using namespace juce;
            auto r = b.getLocalBounds().toFloat().reduced (0.5f);
            const bool on = b.getToggleState();
            const auto acc = b.getProperties().contains ("accent") ? Colour ((uint32) (int64) b.getProperties()["accent"]) : red;
            const float rad = b.getProperties().getWithDefault ("radius", 7.0f);
            g.setColour (on ? acc.withAlpha (0.16f) : Colours::white.withAlpha (down ? 0.09f : over ? 0.06f : 0.03f));
            g.fillRoundedRectangle (r, rad);
            g.setColour (on ? acc.withAlpha (0.55f) : Colours::white.withAlpha (0.08f));
            g.drawRoundedRectangle (r, rad, 1.0f);
        }

        void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
        {
            using namespace juce;
            const bool on = b.getToggleState();
            const auto acc = b.getProperties().contains ("accent") ? Colour ((uint32) (int64) b.getProperties()["accent"]) : redSoft;
            auto c = on ? acc.brighter (0.3f) : silver;
            if (! b.isEnabled()) c = c.withAlpha (0.35f);
            const auto t = b.getButtonText();
            if (auto icon = drawIcon (g, t, b.getLocalBounds().toFloat(), c)) return;
            label (g, t, b.getLocalBounds().toFloat(), jmin (12.5f, (float) b.getHeight() * 0.48f), c, Justification::centred, 600);
        }

        // Transport va asboblar uchun vektor ikonkalar
        static bool drawIcon (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, juce::Colour c)
        {
            using namespace juce;
            const float cx = r.getCentreX(), cy = r.getCentreY(), s = jmin (r.getWidth(), r.getHeight()) * 0.26f;
            Path p; bool fill = true;
            if (t == "#play")      { p.addTriangle (cx - s * 0.7f, cy - s, cx - s * 0.7f, cy + s, cx + s, cy); }
            else if (t == "#stop") { p.addRoundedRectangle (cx - s * 0.8f, cy - s * 0.8f, s * 1.6f, s * 1.6f, 1.5f); }
            else if (t == "#rec")  { p.addEllipse (cx - s * 0.85f, cy - s * 0.85f, s * 1.7f, s * 1.7f); }
            else if (t == "#rew")  { p.addTriangle (cx, cy - s * 0.8f, cx, cy + s * 0.8f, cx - s * 1.1f, cy); p.addTriangle (cx + s * 1.1f, cy - s * 0.8f, cx + s * 1.1f, cy + s * 0.8f, cx, cy); }
            else if (t == "#ff")   { p.addTriangle (cx, cy - s * 0.8f, cx, cy + s * 0.8f, cx + s * 1.1f, cy); p.addTriangle (cx - s * 1.1f, cy - s * 0.8f, cx - s * 1.1f, cy + s * 0.8f, cx, cy); }
            else if (t == "#start"){ p.addRectangle (cx - s * 1.0f, cy - s * 0.8f, s * 0.3f, s * 1.6f); p.addTriangle (cx + s * 0.9f, cy - s * 0.8f, cx + s * 0.9f, cy + s * 0.8f, cx - s * 0.6f, cy); }
            else if (t == "#loop")
            {
                fill = false;
                p.addCentredArc (cx, cy, s * 1.1f, s * 0.8f, 0, 0.6f, MathConstants<float>::twoPi - 0.2f, true);
                Path a; a.addTriangle (cx + s * 1.1f, cy - s * 0.55f, cx + s * 0.55f, cy - s * 1.0f, cx + s * 1.45f, cy - s * 1.15f);
                g.setColour (c); g.fillPath (a);
            }
            else if (t == "#click")
            {
                p.addTriangle (cx - s * 0.8f, cy + s, cx + s * 0.8f, cy + s, cx, cy - s * 1.1f);
                Path in; in.addTriangle (cx - s * 0.45f, cy + s * 0.65f, cx + s * 0.45f, cy + s * 0.65f, cx, cy - s * 0.5f);
                p.addPath (in); p.setUsingNonZeroWinding (false);
                Path arm; arm.startNewSubPath (cx, cy + s * 0.5f); arm.lineTo (cx + s * 0.8f, cy - s * 0.9f);
                g.setColour (c); g.strokePath (arm, PathStrokeType (1.4f));
            }
            else if (t == "#plus")  { fill = false; p.startNewSubPath (cx - s, cy); p.lineTo (cx + s, cy); p.startNewSubPath (cx, cy - s); p.lineTo (cx, cy + s); }
            else if (t == "#undo" || t == "#redo")
            {
                fill = false; const float d = t == "#undo" ? 1.0f : -1.0f;
                p.addCentredArc (cx, cy + 1, s * 1.1f, s, 0, d > 0 ? -2.2f : 2.2f, d > 0 ? 1.6f : -1.6f, true);
                p.startNewSubPath (cx - s * 1.0f * d - s * 0.6f * d, cy - s * 0.5f); p.lineTo (cx - s * 1.0f * d, cy - s * 0.1f); p.lineTo (cx - s * 0.85f * d, cy - s * 0.85f);
            }
            else if (t == "#pencil") { fill = false; p.startNewSubPath (cx - s, cy + s); p.lineTo (cx + s * 0.8f, cy - s * 0.8f); p.startNewSubPath (cx - s, cy + s); p.lineTo (cx - s * 0.6f, cy + s * 0.3f); }
            else if (t == "#scissors") { fill = false; p.addEllipse (cx - s, cy + s * 0.2f, s * 0.7f, s * 0.7f); p.addEllipse (cx + s * 0.3f, cy + s * 0.2f, s * 0.7f, s * 0.7f); p.startNewSubPath (cx - s * 0.5f, cy + s * 0.3f); p.lineTo (cx + s * 0.6f, cy - s); p.startNewSubPath (cx + s * 0.5f, cy + s * 0.3f); p.lineTo (cx - s * 0.6f, cy - s); }
            else if (t == "#grid") { fill = false; for (int i = -1; i <= 1; i += 2) { p.startNewSubPath (cx + i * s * 0.45f, cy - s); p.lineTo (cx + i * s * 0.45f, cy + s); p.startNewSubPath (cx - s, cy + i * s * 0.45f); p.lineTo (cx + s, cy + i * s * 0.45f); } }
            else if (t == "#gear") { fill = false; p.addEllipse (cx - s * 0.45f, cy - s * 0.45f, s * 0.9f, s * 0.9f); for (int i = 0; i < 8; ++i) { const float a = (float) i * MathConstants<float>::pi / 4; p.startNewSubPath (cx + std::cos (a) * s * 0.75f, cy + std::sin (a) * s * 0.75f); p.lineTo (cx + std::cos (a) * s * 1.1f, cy + std::sin (a) * s * 1.1f); } }
            else return false;
            g.setColour (c);
            if (fill) g.fillPath (p); else g.strokePath (p, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));
            return true;
        }

        void drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vert, int tp, int ts, bool over, bool) override
        {
            auto r = vert ? juce::Rectangle<float> ((float) x + (float) w * 0.5f - 2.5f, (float) tp, 5, (float) ts) : juce::Rectangle<float> ((float) tp, (float) y + (float) h * 0.5f - 2.5f, (float) ts, 5);
            g.setColour (juce::Colours::white.withAlpha (over ? 0.25f : 0.13f)); g.fillRoundedRectangle (r, 2.5f);
        }

        void drawDocumentWindowTitleBar (juce::DocumentWindow& w, juce::Graphics& g, int wd, int ht, int, int, const juce::Image*, bool) override
        {
            g.fillAll (bg1);
            label (g, w.getName(), { 0, 0, (float) wd, (float) ht }, 13.0f, text, juce::Justification::centred, 500);
        }

        void fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor& e) override
        {
            g.setColour (e.findColour (juce::TextEditor::backgroundColourId)); g.fillRoundedRectangle (0, 0, (float) w, (float) h, 7);
        }
        void drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& e) override
        {
            g.setColour (e.hasKeyboardFocus (true) ? red.withAlpha (0.6f) : juce::Colours::white.withAlpha (0.1f));
            g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1, (float) h - 1, 7, 1);
        }

        void drawTreeviewPlusMinusBox (juce::Graphics& g, const juce::Rectangle<float>& a, juce::Colour, bool isOpen, bool over) override
        {
            juce::Path p; const float cx = a.getCentreX(), cy = a.getCentreY(), s = 3.5f;
            if (isOpen) { p.startNewSubPath (cx - s, cy - s * 0.5f); p.lineTo (cx, cy + s * 0.5f); p.lineTo (cx + s, cy - s * 0.5f); }
            else        { p.startNewSubPath (cx - s * 0.5f, cy - s); p.lineTo (cx + s * 0.5f, cy); p.lineTo (cx - s * 0.5f, cy + s); }
            g.setColour (over ? redSoft : dim); g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        void drawFileBrowserRow (juce::Graphics& g, int w, int h, const juce::File& f, const juce::String& name, juce::Image*,
                                 const juce::String&, const juce::String&, bool isDir, bool sel, int, juce::DirectoryContentsDisplayComponent&) override
        {
            using namespace juce;
            if (sel) { g.setColour (Colours::white.withAlpha (0.07f)); g.fillRoundedRectangle (2, 1, (float) w - 4, (float) h - 2, 5); }
            auto ic = Rectangle<float> (6, (float) h * 0.5f - 9, 18, 18);
            g.setColour (Colours::white.withAlpha (0.05f)); g.fillRoundedRectangle (ic, 4);
            const bool midi = f.hasFileExtension ("mid;midi");
            if (isDir) { g.setColour (dim); g.fillRoundedRectangle (ic.reduced (4, 5), 1.5f); }
            else drawIcon (g, midi ? "#grid" : "#play", ic, midi ? yellow : redSoft);
            label (g, name, { 30, 0, (float) w - 34, (float) h }, 12.0f, sel ? Colours::white : silver, Justification::centredLeft, 400);
        }
    };
}
