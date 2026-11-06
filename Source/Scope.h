// =============================================================================
//  AZ MUS — har bir trek uchun real-time spektrogramma (3D waterfall / 2D)
//  · "azscope" — ko'rinmas o'tkazgich plagin: trek chiqishidan (post-fader) namunalarni FIFO'ga yozadi
//  · SpectroPanel — mikserda trek yonida ochilib-yopiladigan panel
// =============================================================================
#pragma once
#include "AppEngine.h"
#include "Theme.h"

namespace az
{
class ScopeTapPlugin : public te::Plugin
{
public:
    static const char* getPluginName() { return "AZ Scope"; }
    static inline const char* xmlTypeName = "azscope";
    explicit ScopeTapPlugin (te::PluginCreationInfo info) : te::Plugin (info) {}
    ~ScopeTapPlugin() override { notifyListenersOfDeletion(); }
    juce::String getName() const override                { return getPluginName(); }
    juce::String getPluginType() override                { return xmlTypeName; }
    juce::String getSelectableDescription() override     { return getName(); }
    BusLayout getBusses() const override                 { return BusLayout::singlePassThrough(); }
    void initialise (const te::PluginInitialisationInfo& info) override { sampleRate = info.sampleRate > 0 ? info.sampleRate : 48000.0; }
    void deinitialise() override {}
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return;
        const int n = fc.bufferNumSamples, ch = fc.destBuffer->getNumChannels();
        int s1, n1, s2, n2; fifo.prepareToWrite (n, s1, n1, s2, n2);
        auto put = [&] (int dst, int cnt, int src)
        {
            for (int i = 0; i < cnt; ++i)
            {
                float v = 0; for (int c = 0; c < ch; ++c) v += fc.destBuffer->getSample (c, fc.bufferStartSample + src + i);
                ring[(size_t) (dst + i)] = v / (float) juce::jmax (1, ch);
            }
        };
        put (s1, n1, 0); put (s2, n2, n1);
        fifo.finishedWrite (n1 + n2);
    }
    // xabar oqimidan o'qish
    int read (float* dest, int maxN)
    {
        int s1, n1, s2, n2; fifo.prepareToRead (juce::jmin (maxN, fifo.getNumReady()), s1, n1, s2, n2);
        std::copy (ring.begin() + s1, ring.begin() + s1 + n1, dest);
        std::copy (ring.begin() + s2, ring.begin() + s2 + n2, dest + n1);
        fifo.finishedRead (n1 + n2);
        return n1 + n2;
    }
    double sampleRate = 48000.0;
private:
    static constexpr int ringSize = 1 << 15;
    juce::AbstractFifo fifo { ringSize };
    std::array<float, ringSize> ring {};
};

inline ScopeTapPlugin* findScope (te::AudioTrack& t)
{
    for (auto p : t.pluginList.getPlugins()) if (auto* s = dynamic_cast<ScopeTapPlugin*> (p)) return s;
    return nullptr;
}
inline ScopeTapPlugin* ensureScope (te::AudioTrack& t)
{
    if (auto* s = findScope (t)) return s;
    auto p = t.edit.getPluginCache().createNewPlugin (ScopeTapPlugin::xmlTypeName, {});
    if (p == nullptr) return nullptr;
    t.pluginList.insertPlugin (p, -1, nullptr);   // oxirida: fader va panoramadan keyin
    return dynamic_cast<ScopeTapPlugin*> (p.get());
}
inline void removeScope (te::AudioTrack& t) { if (auto* s = findScope (t)) s->deleteFromParent(); }

// =============================================================================  panel
class SpectroPanel : public juce::Component, private juce::Timer
{
public:
    std::function<void()> onClose;
    SpectroPanel (te::AudioTrack& t) : track (&t)
    {
        ensureScope (t);
        for (auto* b : { &modeB, &freezeB, &closeB }) { addAndMakeVisible (b); b->getProperties().set ("radius", 4.0f); b->setWantsKeyboardFocus (false); }
        modeB.setButtonText ("3D"); freezeB.setButtonText (juce::String::fromUTF8 ("\xe2\x9d\x9a\xe2\x9d\x9a")); closeB.setButtonText (juce::String::fromUTF8 ("\xc3\x97"));
        modeB.setTooltip (U ("3D waterfall / 2D spektrogramma / spektr"));
        freezeB.setTooltip (U ("Muzlatish (rasmni to'xtatish)")); freezeB.setClickingTogglesState (true);
        closeB.setTooltip (U ("Spektrogrammani yopish"));
        modeB.onClick = [this] { mode = (mode + 1) % 3; modeB.setButtonText (mode == 0 ? "3D" : mode == 1 ? "2D" : "FFT"); repaint(); };
        closeB.onClick = [this] { if (onClose) onClose(); };
        for (auto& r : history) r.fill (-120.0f);
        peak.fill (-120.0f);
        startTimerHz (30);
    }
    ~SpectroPanel() override { stopTimer(); }
    te::AudioTrack* getTrack() const { return dynamic_cast<te::AudioTrack*> (track.get()); }

    static constexpr int fftOrder = 11, fftSize = 1 << fftOrder, bins = 120, rows = 56;
    static double binHz (int b) { return 20.0 * std::pow (1000.0, (double) b / (bins - 1)); }   // 20 Hz .. 20 kHz (log)

    void timerCallback() override
    {
        auto* at = dynamic_cast<te::AudioTrack*> (track.get());
        if (at == nullptr) return;
        auto* sc = findScope (*at);
        if (sc == nullptr) { sc = ensureScope (*at); if (sc == nullptr) return; }
        float tmp[4096];
        int got = sc->read (tmp, 4096);
        for (int i = 0; i < got; ++i) { input[(size_t) inPos] = tmp[i]; inPos = (inPos + 1) % fftSize; ++fresh; }
        if (freezeB.getToggleState()) return;
        bool any = false;
        while (fresh >= fftSize / 2)       // 50% overlap
        {
            fresh -= fftSize / 2;
            analyse (sc->sampleRate); any = true;
        }
        if (! any)                          // jimlik: sekin so'nish
        {
            auto row = history[(size_t) head];
            for (auto& v : row) v = juce::jmax (-120.0f, v - 3.0f);
            push (row);
        }
        repaint();
    }
    void analyse (double sr)
    {
        std::array<float, fftSize * 2> buf {};
        for (int i = 0; i < fftSize; ++i)
        {
            const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (fftSize - 1));
            buf[(size_t) i] = input[(size_t) ((inPos + i) % fftSize)] * w;
        }
        fft.performFrequencyOnlyForwardTransform (buf.data());
        std::array<float, bins> row {};
        for (int b = 0; b < bins; ++b)
        {
            const double f0 = binHz (b) / std::pow (1000.0, 0.5 / (bins - 1)), f1 = binHz (b) * std::pow (1000.0, 0.5 / (bins - 1));
            const int k0 = juce::jlimit (1, fftSize / 2 - 1, (int) (f0 / sr * fftSize)), k1 = juce::jlimit (k0, fftSize / 2 - 1, (int) (f1 / sr * fftSize));
            float m = 0; for (int k = k0; k <= k1; ++k) m = juce::jmax (m, buf[(size_t) k]);
            float db = juce::Decibels::gainToDecibels (m / (fftSize * 0.25f), -120.0f);
            db += 3.0f * (float) std::log2 (binHz (b) / 1000.0);         // 3 dB/oktava egilish (Insight kabi pushti shovqin tekis)
            const float prev = history[(size_t) head][(size_t) b];
            row[(size_t) b] = db > prev ? db : prev + (db - prev) * 0.45f;   // tez ko'tarilish, sekin tushish
            peak[(size_t) b] = juce::jmax (peak[(size_t) b] - 0.15f, row[(size_t) b]);
        }
        push (row);
    }
    void push (const std::array<float, bins>& r) { head = (head + 1) % rows; history[(size_t) head] = r; }
    const std::array<float, bins>& rowAgo (int k) const { return history[(size_t) ((head - k + rows * 4) % rows)]; }

    void resized() override
    {
        auto r = getLocalBounds().reduced (6).removeFromTop (20);
        closeB.setBounds (r.removeFromRight (20)); r.removeFromRight (3);
        freezeB.setBounds (r.removeFromRight (24)); r.removeFromRight (3);
        modeB.setBounds (r.removeFromRight (34));
    }
    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        auto b = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (Colour (0xff0b0a0e)); g.fillRoundedRectangle (b, 10);
        g.setColour (Colours::white.withAlpha (0.09f)); g.drawRoundedRectangle (b, 10, 1);
        caps (g, track ? track->getName() : String(), { 10, 9, b.getWidth() - 110, 12 }, 9, track ? track->getColour().brighter (0.3f) : silver);
        auto area = b.reduced (10, 0).withTrimmedTop (32).withTrimmedBottom (22);
        const float lo = -90.0f, hi = 0.0f;
        auto norm = [&] (float db) { return jlimit (0.0f, 1.0f, (db - lo) / (hi - lo)); };
        if (mode == 0)       // 3D waterfall
        {
            const float depth = area.getHeight() * 0.48f, skew = area.getWidth() * 0.22f;
            for (int k = rows - 1; k >= 0; --k)
            {
                const float d = (float) k / (rows - 1);
                const float baseY = area.getBottom() - d * depth;
                const float x0 = area.getX() + d * skew, w = area.getWidth() * (1.0f - d * 0.32f);
                const float hScale = (area.getHeight() - depth) * (1.0f - d * 0.25f);
                const auto& row = rowAgo (k);
                float rmax = 0; for (auto v : row) rmax = jmax (rmax, norm (v));
                const float act = 0.15f + 0.85f * jmin (1.0f, rmax * 2.5f);   // jimlikda xira
                Path p; p.startNewSubPath (x0, baseY);
                for (int i = 0; i < bins; ++i) p.lineTo (x0 + w * i / (bins - 1), baseY - norm (row[(size_t) i]) * hScale);
                p.lineTo (x0 + w, baseY); p.closeSubPath();
                g.setColour (Colour (0xff0b0a0e).withAlpha (0.92f)); g.fillPath (p);
                const float bright = 1.0f - d * 0.85f;
                g.setColour (Colour::fromHSV (0.58f - 0.58f * (1.0f - d), 0.15f + 0.25f * d, 0.35f + 0.65f * bright, (0.55f + 0.45f * bright) * act));
                g.strokePath (p, PathStrokeType (k == 0 ? 1.5f : 0.8f));
            }
        }
        else if (mode == 1)  // 2D spektrogramma (vaqt pastga oqadi)
        {
            const float rh = area.getHeight() / rows, cw = area.getWidth() / bins;
            for (int k = 0; k < rows; ++k)
            {
                const auto& row = rowAgo (k);
                for (int i = 0; i < bins; ++i)
                {
                    const float v = norm (row[(size_t) i]);
                    if (v < 0.02f) continue;
                    g.setColour (heat (v)); g.fillRect (area.getX() + i * cw, area.getY() + k * rh, cw + 0.6f, rh + 0.6f);
                }
            }
        }
        else                 // spektr + cho'qqi
        {
            const auto& row = rowAgo (0);
            Path p, pk; p.startNewSubPath (area.getX(), area.getBottom());
            for (int i = 0; i < bins; ++i)
            {
                const float x = area.getX() + area.getWidth() * i / (bins - 1);
                p.lineTo (x, area.getBottom() - norm (row[(size_t) i]) * area.getHeight());
                if (i == 0) pk.startNewSubPath (x, area.getBottom() - norm (peak[(size_t) i]) * area.getHeight());
                else pk.lineTo (x, area.getBottom() - norm (peak[(size_t) i]) * area.getHeight());
            }
            p.lineTo (area.getRight(), area.getBottom()); p.closeSubPath();
            g.setGradientFill (ColourGradient (Colour (0xff4ad6ff).withAlpha (0.45f), 0, area.getY(), Colour (0xff4ad6ff).withAlpha (0.05f), 0, area.getBottom(), false));
            g.fillPath (p);
            g.setColour (Colour (0xff4ad6ff)); g.strokePath (p, PathStrokeType (1.2f));
            g.setColour (Colours::white.withAlpha (0.5f)); g.strokePath (pk, PathStrokeType (0.8f));
            for (float db : { -12.0f, -24.0f, -48.0f, -72.0f })
            {
                const float y = area.getBottom() - norm (db) * area.getHeight();
                g.setColour (Colours::white.withAlpha (0.05f)); g.drawHorizontalLine ((int) y, area.getX(), area.getRight());
                label (g, String ((int) db), { area.getRight() - 24, y - 10, 22, 9 }, 7.5f, dim2, Justification::centredRight, 400);
            }
        }
        // chastota o'qi
        for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
        {
            const float x = area.getX() + area.getWidth() * (float) (std::log (f / 20.0) / std::log (1000.0));
            label (g, f >= 1000 ? String ((int) (f / 1000)) + "k" : String ((int) f), { x - 14, b.getBottom() - 20, 28, 12 }, 7.5f, dim, Justification::centred, 500);
        }
        // sichqoncha ostidagi chastota
        if (hoverX >= 0 && area.contains (hoverX, hoverY))
        {
            const int i = jlimit (0, bins - 1, (int) ((hoverX - area.getX()) / area.getWidth() * (bins - 1) + 0.5f));
            g.setColour (Colours::white.withAlpha (0.25f)); g.drawVerticalLine ((int) hoverX, area.getY(), area.getBottom());
            const double hz = binHz (i);
            label (g, (hz >= 1000 ? String (hz / 1000.0, 2) + " kHz" : String ((int) hz) + " Hz") + "  " + String (rowAgo (0)[(size_t) i], 1) + " dB",
                   { area.getX(), area.getY() - 2, area.getWidth(), 12 }, 8.5f, Colours::white, Justification::centredLeft, 600);
        }
    }
    void mouseMove (const juce::MouseEvent& e) override { hoverX = e.position.x; hoverY = e.position.y; }
    void mouseExit (const juce::MouseEvent&) override { hoverX = -1; }
    static juce::Colour heat (float v)
    {
        if (v < 0.35f) return juce::Colour (0xff1c2c6e).interpolatedWith (juce::Colour (0xff6b2fbf), v / 0.35f);
        if (v < 0.7f)  return juce::Colour (0xff6b2fbf).interpolatedWith (juce::Colour (0xffff3347), (v - 0.35f) / 0.35f);
        return juce::Colour (0xffff3347).interpolatedWith (juce::Colour (0xffffe9a8), (v - 0.7f) / 0.3f);
    }
private:
    te::Track::Ptr track;
    juce::dsp::FFT fft { fftOrder };
    std::array<float, fftSize> input {}; int inPos = 0, fresh = 0;
    std::array<std::array<float, bins>, rows> history {}; int head = 0;
    std::array<float, bins> peak {};
    int mode = 0; float hoverX = -1, hoverY = 0;
    juce::TextButton modeB, freezeB, closeB;
};
}
