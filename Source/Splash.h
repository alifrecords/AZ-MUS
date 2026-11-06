// =============================================================================
//  AZ MUS — ochilish oynasi (splash), namunadagi dizayn
// =============================================================================
#pragma once
#include "Theme.h"

namespace az
{
class SplashComponent : public juce::Component, private juce::Timer
{
public:
    juce::StringArray steps { "AUDIO DRAYVER", "MIDI QURILMALAR", "VST3 PLAGINLAR", "SAMPLE KUTUBXONA", "LOYIHA OYNASI" };

    SplashComponent() { setSize (900, 520); setOpaque (false); startTimerHz (30); setInterceptsMouseClicks (false, false); }

    void setStep (int s, const juce::String& status, float prog)
    {
        step = s; statusText = status; target = prog;
        repaint();
    }
    void setDetail (const juce::String& d) { detail = d; repaint(); }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        const float W = (float) getWidth(), H = (float) getHeight();
        Path clip; clip.addRoundedRectangle (0.0f, 0.0f, W, H, 14.0f);
        g.reduceClipRegion (clip);
        g.fillAll (Colour (0xff07070a));

        // qizil nur
        ColourGradient glow (redHot.withAlpha (0.33f), W - 190, 170, redHot.withAlpha (0.0f), W - 190 + 330, 170, true);
        g.setGradientFill (glow); g.fillEllipse (W - 520, -160, 660, 660);

        // to'lqin san'ati
        for (int k = 0; k < 46; ++k)
        {
            const float t = (float) k / 45.0f;
            Path p;
            for (float x = 440; x <= W + 10; x += 3)
            {
                const float u = (x - 440) / (W - 440);
                const float env = std::sin (MathConstants<float>::pi * jmin (1.0f, u * 1.15f)) * (0.35f + 0.65f * std::exp (-std::pow ((u - 0.55f) / 0.3f, 2.0f)));
                const float y = H * 0.56f + std::sin (u * 11 + t * 5.2f + phase) * 58 * env * (0.4f + t * 0.8f)
                              + std::sin (u * 27 + t * 9 + phase * 1.7f) * 10 * env + (t - 0.5f) * 70 * env;
                if (x == 440) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            const float a = 0.05f + 0.5f * std::pow (1.0f - std::abs (t - 0.5f) * 2.0f, 2.2f);
            g.setColour (k % 7 == 0 ? Colours::white.withAlpha (a * 0.7f) : red.withAlpha (a));
            g.strokePath (p, PathStrokeType (k % 7 == 0 ? 1.1f : 0.8f));
        }
        // spektr ustunlari
        for (int i = 0; i < 64; ++i)
        {
            const float h = 4 + std::abs (std::sin ((float) i * 0.37f + phase * 0.3f) * std::cos ((float) i * 0.13f)) * 34 + (i % 5 == 0 ? 10 : 0);
            g.setColour (red.withAlpha (0.08f + h / 260.0f));
            g.fillRect (56 + (float) i * 12.3f, H - 120 - h, 6.0f, h);
        }

        // logotip va nom
        logoMark (g, { 56, 62, 58, 58 });
        g.setFont (font (96, 700, 0.06f));
        g.setColour (Colour (0xfff3efef)); g.drawText ("AZ", Rectangle<float> (52, 140, 200, 110), Justification::centredLeft);
        const float azw = textWidth (font (96, 700, 0.06f), "AZ ");
        g.setColour (red); g.drawText ("MUS", Rectangle<float> (52 + azw, 140, 320, 110), Justification::centredLeft);
        label (g, "AZIMXODJAYEV MUSIC", { 58, 254, 420, 20 }, 14, Colour (0xff9b9395), Justification::centredLeft, 500, 0.42f);
        auto tag = Rectangle<float> (56, 292, 212, 24);
        g.setColour (red.withAlpha (0.4f)); g.drawRoundedRectangle (tag, 6, 1);
        caps (g, "DIGITAL AUDIO WORKSTATION", tag, 10.5f, redSoft, Justification::centred);
        label (g, "Version " + juce::String (JUCE_APPLICATION_VERSION_STRING) + U (" \xc2\xb7 64-bit"), { 280, 292, 220, 24 }, 12, Colour (0xff7d7678));

        // bosqichlar
        for (int i = 0; i < steps.size(); ++i)
        {
            auto r = Rectangle<float> (W - 360, 70.0f + (float) i * 22, 304, 20);
            const Colour c = i < step ? dim : i == step ? redSoft : Colour (0xff4e494b);
            caps (g, steps[i] + (i < step ? U ("  \xe2\x9c\x93") : String()), r, 10.5f, c, Justification::centredRight);
        }

        // holat va progress
        label (g, statusText, { 56, H - 92, 600, 16 }, 11.5f, Colour (0xffa49c9e), Justification::centredLeft, 400);
        if (detail.isNotEmpty())
        {
            const float sw = textWidth (font (11.5f), statusText) + 10;
            label (g, detail, { 56 + sw, H - 92, 600 - sw, 16 }, 11.5f, redSoft, Justification::centredLeft, 400);
        }
        if (step == 2) caps (g, U ("BOSING \xe2\x80\x94 O'TKAZIB YUBORISH"), { W - 360, H - 92, 220, 16 }, 9.5f, Colour (0xff7d7678), Justification::centredRight);
        label (g, juce::String (roundToInt (shown * 100)) + "%", { W - 156, H - 92, 100, 16 }, 11.5f, Colour (0xffa49c9e), Justification::centredRight);
        auto bar = Rectangle<float> (56, H - 70, W - 112, 3);
        g.setColour (Colours::white.withAlpha (0.07f)); g.fillRoundedRectangle (bar, 1.5f);
        auto fill = bar.withWidth (bar.getWidth() * shown);
        g.setGradientFill (ColourGradient (redDeep, fill.getX(), 0, Colour (0xffffb0a0), fill.getRight(), 0, false));
        g.fillRoundedRectangle (fill, 1.5f);
        caps (g, U ("\xc2\xa9 2026 AZIMXODJAYEV MUSIC \xc2\xb7 ALIF RECORDS"), { 56, H - 54, 420, 14 }, 10, Colour (0xff5d5759));
        caps (g, "POWERED BY TRACKTION ENGINE", { W - 356, H - 54, 300, 14 }, 10, Colour (0xff5d5759), Justification::centredRight);
    }

private:
    int step = 0; float target = 0, shown = 0, phase = 0;
    juce::String statusText { U ("Ishga tushirilmoqda\xe2\x80\xa6") }, detail;
    void timerCallback() override { shown += (target - shown) * 0.2f; phase += 0.03f; repaint(); }
};

class SplashWindow : public juce::Component
{
public:
    std::atomic<bool> skipRequested { false };
    void mouseDown (const juce::MouseEvent&) override { skipRequested = true; }
    SplashWindow()
    {
        addAndMakeVisible (content);
        setSize (content.getWidth(), content.getHeight());
        setOpaque (false);
        addToDesktop (juce::ComponentPeer::windowIsTemporary | juce::ComponentPeer::windowHasDropShadow);
        centreWithSize (getWidth(), getHeight());
        setVisible (true); toFront (false);
    }
    void resized() override { content.setBounds (getLocalBounds()); }
    SplashComponent content;
};
}
