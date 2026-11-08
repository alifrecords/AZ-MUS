// =============================================================================
//  AZ MUS — har bir trek uchun Ozone uslubidagi vositalar (Inspektorda):
//  · "azeq"     — AZ EQ: 6 polosali parametrik ekvalayzer + spektr (Ozone Equalizer)
//  · "azimager" — AZ Imager: 4 polosali stereo kenglik (crossover), Stereoize,
//                 vektorskop (Polar Sample / Polar Level / Lissajous) va korrelyatsiya
//  Har trekda alohida ishlaydi; standart holatda (0 dB / 0 kenglik) ovozga tegmaydi.
// =============================================================================
#pragma once
#include "AppEngine.h"
#include "Theme.h"
#include <complex>

namespace az
{
namespace tt
{
    // RBJ biquad
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        double z1[2] {}, z2[2] {};
        inline float process (float x, int ch)
        {
            const double y = b0 * x + z1[ch];
            z1[ch] = b1 * x - a1 * y + z2[ch];
            z2[ch] = b2 * x - a2 * y;
            return (float) y;
        }
        void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0; }
        std::complex<double> response (double f, double sr) const
        {
            const std::complex<double> z = std::polar (1.0, -juce::MathConstants<double>::twoPi * f / sr), z2c = z * z;
            return (b0 + b1 * z + b2 * z2c) / (1.0 + a1 * z + a2 * z2c);
        }
        // turlar: 0 bell, 1 low shelf, 2 high shelf, 3 low cut (HP), 4 high cut (LP)
        void set (int type, double f, double gDb, double q, double sr)
        {
            f = juce::jlimit (10.0, sr * 0.49, f); q = juce::jmax (0.1, q);
            const double A = std::pow (10.0, gDb / 40.0), w = juce::MathConstants<double>::twoPi * f / sr;
            const double cw = std::cos (w), sw = std::sin (w), al = sw / (2 * q);
            double B0, B1, B2, A0, A1, A2;
            switch (type)
            {
                case 1: { const double s = 2 * std::sqrt (A) * al;
                          B0 = A * ((A + 1) - (A - 1) * cw + s); B1 = 2 * A * ((A - 1) - (A + 1) * cw); B2 = A * ((A + 1) - (A - 1) * cw - s);
                          A0 = (A + 1) + (A - 1) * cw + s; A1 = -2 * ((A - 1) + (A + 1) * cw); A2 = (A + 1) + (A - 1) * cw - s; break; }
                case 2: { const double s = 2 * std::sqrt (A) * al;
                          B0 = A * ((A + 1) + (A - 1) * cw + s); B1 = -2 * A * ((A - 1) + (A + 1) * cw); B2 = A * ((A + 1) + (A - 1) * cw - s);
                          A0 = (A + 1) - (A - 1) * cw + s; A1 = 2 * ((A - 1) - (A + 1) * cw); A2 = (A + 1) - (A - 1) * cw - s; break; }
                case 3: B0 = (1 + cw) / 2; B1 = -(1 + cw); B2 = (1 + cw) / 2; A0 = 1 + al; A1 = -2 * cw; A2 = 1 - al; break;
                case 4: B0 = (1 - cw) / 2; B1 = 1 - cw; B2 = (1 - cw) / 2; A0 = 1 + al; A1 = -2 * cw; A2 = 1 - al; break;
                default: B0 = 1 + al * A; B1 = -2 * cw; B2 = 1 - al * A; A0 = 1 + al / A; A1 = -2 * cw; A2 = 1 - al / A; break;
            }
            b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
        }
    };
    inline double xToHz (float x, float w) { return 20.0 * std::pow (1000.0, juce::jlimit (0.0, 1.0, (double) x / juce::jmax (1.0f, w))); }
    inline float hzToX (double f, float w) { return (float) (std::log (juce::jlimit (20.0, 20000.0, f) / 20.0) / std::log (1000.0)) * w; }
    inline juce::String hzText (double f) { return f >= 1000 ? juce::String (f / 1000.0, f >= 10000 ? 1 : 2) + "k" : juce::String (juce::roundToInt (f)); }

    // mono FIFO (spektr uchun) va stereo FIFO (vektorskop uchun)
    template <int N, int C> struct Fifo
    {
        juce::AbstractFifo fifo { N };
        std::array<float, (size_t) N * C> ring {};
        void push (const juce::AudioBuffer<float>& b, int start, int n)
        {
            int s1, n1, s2, n2; fifo.prepareToWrite (n, s1, n1, s2, n2);
            const int ch = b.getNumChannels();
            auto put = [&] (int dst, int cnt, int src)
            {
                for (int i = 0; i < cnt; ++i)
                {
                    const float l = b.getSample (0, start + src + i), r = ch > 1 ? b.getSample (1, start + src + i) : l;
                    if (C == 1) ring[(size_t) (dst + i)] = 0.5f * (l + r);
                    else { ring[(size_t) (dst + i) * 2] = l; ring[(size_t) (dst + i) * 2 + 1] = r; }
                }
            };
            put (s1, n1, 0); put (s2, n2, n1);
            fifo.finishedWrite (n1 + n2);
        }
        int read (float* dest, int maxN)
        {
            int s1, n1, s2, n2; fifo.prepareToRead (juce::jmin (maxN, fifo.getNumReady()), s1, n1, s2, n2);
            std::copy (ring.begin() + s1 * C, ring.begin() + (s1 + n1) * C, dest);
            std::copy (ring.begin() + s2 * C, ring.begin() + (s2 + n2) * C, dest + n1 * C);
            fifo.finishedRead (n1 + n2);
            return n1 + n2;
        }
    };
}

// =============================================================================  AZ EQ
class AzTrackEqPlugin : public te::Plugin
{
public:
    static const char* getPluginName() { return "AZ EQ"; }
    static inline const char* xmlTypeName = "azeq";
    static constexpr int numBands = 6;
    struct Defaults { int type; double f, g, q; bool on; };
    static constexpr Defaults defs[numBands] = { { 3, 30, 0, 0.71, false }, { 1, 100, 0, 0.71, true }, { 0, 400, 0, 1.0, true },
                                                 { 0, 1500, 0, 1.0, true }, { 0, 5000, 0, 1.0, true }, { 2, 10000, 0, 0.71, true } };

    explicit AzTrackEqPlugin (te::PluginCreationInfo info) : te::Plugin (info)
    {
        for (int b = 0; b < numBands; ++b)
            for (auto [k, v] : std::initializer_list<std::pair<const char*, double>> { { "t", (double) defs[b].type }, { "f", defs[b].f }, { "g", defs[b].g }, { "q", defs[b].q }, { "on", defs[b].on ? 1.0 : 0.0 } })
                if (! state.hasProperty (key (b, k))) state.setProperty (key (b, k), v, nullptr);
        pull();
    }
    ~AzTrackEqPlugin() override { notifyListenersOfDeletion(); }
    static juce::Identifier key (int b, const char* p) { return juce::Identifier ("eq" + juce::String (b) + p); }
    juce::String getName() const override                { return getPluginName(); }
    juce::String getPluginType() override                { return xmlTypeName; }
    juce::String getSelectableDescription() override     { return getName(); }
    BusLayout getBusses() const override                 { return BusLayout::singlePassThrough(); }
    void initialise (const te::PluginInitialisationInfo& info) override { sampleRate = info.sampleRate > 0 ? info.sampleRate : 48000.0; dirty = true; for (auto& f : flt) f.reset(); }
    void deinitialise() override {}
    void restorePluginStateFromValueTree (const juce::ValueTree& v) override
    {
        for (int i = 0; i < v.getNumProperties(); ++i) state.setProperty (v.getPropertyName (i), v.getProperty (v.getPropertyName (i)), nullptr);
        pull();
    }
    void valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& id) override
    {
        te::Plugin::valueTreePropertyChanged (v, id);
        if (v == state && id.toString().startsWith ("eq")) pull();
    }
    // UI uchun
    double get (int b, const char* p) const { return state.getProperty (key (b, p)); }
    void set (int b, const char* p, double v, bool undoable = true) { state.setProperty (key (b, p), v, undoable ? getUndoManager() : nullptr); }
    bool bandActive (int b) const { const int t = (int) get (b, "t"); return (bool) (int) get (b, "on") && (t >= 3 || std::abs (get (b, "g")) > 0.01); }
    double responseDb (double f) const
    {
        double db = 0;
        for (int b = 0; b < numBands; ++b)
        {
            if (! bandActive (b)) continue;
            tt::Biquad q; q.set ((int) get (b, "t"), get (b, "f"), get (b, "g"), get (b, "q"), 48000.0);
            db += 20.0 * std::log10 (juce::jmax (1e-6, std::abs (q.response (f, 48000.0))));
        }
        return db;
    }
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return;
        auto& buf = *fc.destBuffer; const int n = fc.bufferNumSamples, s0 = fc.bufferStartSample, ch = juce::jmin (2, buf.getNumChannels());
        if (dirty.exchange (false))
            for (int b = 0; b < numBands; ++b)
            {
                active[b] = p[b][4] > 0.5f && (p[b][0] >= 3 || std::abs (p[b][2]) > 0.01f);
                if (active[b]) flt[b].set ((int) p[b][0], p[b][1], p[b][2], p[b][3], sampleRate);
            }
        for (int b = 0; b < numBands; ++b)
        {
            if (! active[b]) continue;
            for (int c = 0; c < ch; ++c)
            {
                auto* d = buf.getWritePointer (c, s0);
                for (int i = 0; i < n; ++i) d[i] = flt[b].process (d[i], c);
            }
        }
        analysis.push (buf, s0, n);
    }
    std::unique_ptr<EditorComponent> createEditor() override;
    tt::Fifo<1 << 14, 1> analysis;
    double sampleRate = 48000.0;
private:
    void pull()
    {
        for (int b = 0; b < numBands; ++b)
        {
            p[b][0] = (float) (double) state.getProperty (key (b, "t"), defs[b].type); p[b][1] = (float) (double) state.getProperty (key (b, "f"), defs[b].f);
            p[b][2] = (float) (double) state.getProperty (key (b, "g"), defs[b].g); p[b][3] = (float) (double) state.getProperty (key (b, "q"), defs[b].q);
            p[b][4] = (float) (double) state.getProperty (key (b, "on"), defs[b].on ? 1.0 : 0.0);
        }
        dirty = true;
    }
    std::atomic<float> p[numBands][5] {};
    std::atomic<bool> dirty { true };
    bool active[numBands] {};
    tt::Biquad flt[numBands];
};

// =============================================================================  AZ Imager
class AzImagerPlugin : public te::Plugin
{
public:
    static const char* getPluginName() { return "AZ Imager"; }
    static inline const char* xmlTypeName = "azimager";
    static constexpr int numBands = 4;
    explicit AzImagerPlugin (te::PluginCreationInfo info) : te::Plugin (info)
    {
        for (auto [k, v] : std::initializer_list<std::pair<const char*, double>> { { "x1", 120 }, { "x2", 1000 }, { "x3", 6000 },
                                                                                    { "w0", 0 }, { "w1", 0 }, { "w2", 0 }, { "w3", 0 }, { "stz", 0 }, { "stzAmt", 50 } })
            if (! state.hasProperty (juce::Identifier ("im_" + juce::String (k)))) state.setProperty (juce::Identifier ("im_" + juce::String (k)), v, nullptr);
        pull();
    }
    ~AzImagerPlugin() override { notifyListenersOfDeletion(); }
    juce::String getName() const override                { return getPluginName(); }
    juce::String getPluginType() override                { return xmlTypeName; }
    juce::String getSelectableDescription() override     { return getName(); }
    BusLayout getBusses() const override                 { return BusLayout::singlePassThrough(); }
    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sampleRate = info.sampleRate > 0 ? info.sampleRate : 48000.0; dirty = true;
        delay.assign ((size_t) (sampleRate * 0.05) + 8, 0.0f); dpos = 0;
        for (auto& f : lp) f.reset(); for (auto& f : hp) f.reset(); for (auto& f : apL) f.reset(); for (auto& f : apH) f.reset(); stzHp.reset();
    }
    void deinitialise() override {}
    void restorePluginStateFromValueTree (const juce::ValueTree& v) override
    {
        for (int i = 0; i < v.getNumProperties(); ++i) state.setProperty (v.getPropertyName (i), v.getProperty (v.getPropertyName (i)), nullptr);
        pull();
    }
    void valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& id) override
    {
        te::Plugin::valueTreePropertyChanged (v, id);
        if (v == state && id.toString().startsWith ("im_")) pull();
    }
    double get (const char* k) const { return state.getProperty (juce::Identifier ("im_" + juce::String (k))); }
    void set (const char* k, double v, bool undoable = true) { state.setProperty (juce::Identifier ("im_" + juce::String (k)), v, undoable ? getUndoManager() : nullptr); }
    double crossover (int i) const { return get (i == 0 ? "x1" : i == 1 ? "x2" : "x3"); }
    void setCrossover (int i, double hz)
    {
        const double lo = i == 0 ? 30.0 : crossover (i - 1) * 1.25, hi = i == 2 ? 16000.0 : crossover (i + 1) / 1.25;
        set (i == 0 ? "x1" : i == 1 ? "x2" : "x3", std::round (juce::jlimit (lo, hi, hz)));
    }
    double width (int b) const { return get (b == 0 ? "w0" : b == 1 ? "w1" : b == 2 ? "w2" : "w3"); }
    void setWidth (int b, double w) { set (b == 0 ? "w0" : b == 1 ? "w1" : b == 2 ? "w2" : "w3", juce::jlimit (-100.0, 100.0, std::round (w * 10) / 10)); }

    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return;
        auto& buf = *fc.destBuffer; const int n = fc.bufferNumSamples, s0 = fc.bufferStartSample;
        if (buf.getNumChannels() >= 2)
        {
            if (dirty.exchange (false))
            {
                for (int k = 0; k < 3; ++k)
                {
                    const double f = xo[k].load();
                    for (int s = 0; s < 2; ++s) { lp[k * 2 + s].set (4, f, 0, 0.7071, sampleRate); hp[k * 2 + s].set (3, f, 0, 0.7071, sampleRate); }
                    for (int s = 0; s < 2; ++s) for (int a = 0; a < 2; ++a)
                    { apL[(k * 2 + s) * 2 + a].set (4, f, 0, 0.7071, sampleRate); apH[(k * 2 + s) * 2 + a].set (3, f, 0, 0.7071, sampleRate); }
                }
                stzHp.set (3, xo[0].load(), 0, 0.7071, sampleRate);
                bool any = std::abs (stzOn.load() ? stzAmt.load() : 0.0f) > 0.01f;
                for (int b = 0; b < numBands; ++b) { g[b] = (wd[b].load() + 100.0f) / 100.0f; any |= std::abs (wd[b].load()) > 0.01f; }
                processing = any;
            }
            if (processing)
            {
                auto* L = buf.getWritePointer (0, s0); auto* R = buf.getWritePointer (1, s0);
                const float stz = stzOn.load() ? stzAmt.load() / 100.0f * 0.7f : 0.0f;
                const int dlen = (int) delay.size(), dsmp = juce::jmax (1, (int) (sampleRate * 0.011));
                for (int i = 0; i < n; ++i)
                {
                    const float m = 0.5f * (L[i] + R[i]); float s = 0.5f * (L[i] - R[i]);
                    // S'ni 4 polosaga bo'lish (LR4 daraxti + allpass kompensatsiyasi)
                    auto LR = [&] (tt::Biquad* f, int k, float x) { return f[k * 2 + 1].process (f[k * 2].process (x, 0), 0); };
                    auto AP = [&] (int k, int slot, float x)
                    {
                        const int base = (k * 2) * 2 + slot * 0;
                        const float a = apL[(size_t) (k * 4 + slot * 2)].process (x, 0);
                        const float lo = apL[(size_t) (k * 4 + slot * 2 + 1)].process (a, 0);
                        const float b = apH[(size_t) (k * 4 + slot * 2)].process (x, 0);
                        const float hi = apH[(size_t) (k * 4 + slot * 2 + 1)].process (b, 0);
                        juce::ignoreUnused (base);
                        return lo + hi;
                    };
                    const float low = LR (lp, 0, s), rest1 = LR (hp, 0, s);
                    const float mid1 = LR (lp, 1, rest1), rest2 = LR (hp, 1, rest1);
                    const float mid2 = LR (lp, 2, rest2), high = LR (hp, 2, rest2);
                    const float lowC = AP (2, 1, AP (1, 0, low));          // past polosa: x2 va x3 allpass
                    const float mid1C = AP (2, 0, mid1);                   // 1-o'rta: x3 allpass
                    float so = lowC * g[0] + mid1C * g[1] + mid2 * g[2] + high * g[3];
                    if (stz > 0)
                    {   // Stereoize: kechiktirilgan va x1 dan yuqori filtrlangan mid'dan sun'iy kenglik
                        const float dm = delay[(size_t) ((dpos - dsmp + dlen) % dlen)];
                        so += stz * stzHp.process (dm, 0);
                    }
                    delay[(size_t) dpos] = m; dpos = (dpos + 1) % dlen;
                    L[i] = m + so; R[i] = m - so;
                }
            }
        }
        scope.push (buf, s0, n);
    }
    std::unique_ptr<EditorComponent> createEditor() override;
    tt::Fifo<1 << 13, 2> scope;
    double sampleRate = 48000.0;
private:
    void pull()
    {
        xo[0] = (float) get ("x1"); xo[1] = (float) get ("x2"); xo[2] = (float) get ("x3");
        for (int b = 0; b < numBands; ++b) wd[b] = (float) width (b);
        stzOn = (bool) (int) get ("stz"); stzAmt = (float) get ("stzAmt");
        dirty = true;
    }
    std::atomic<float> xo[3] {}, wd[numBands] {}, stzAmt { 50 }; std::atomic<bool> stzOn { false }, dirty { true };
    bool processing = false; float g[numBands] { 1, 1, 1, 1 };
    tt::Biquad lp[6], hp[6], apL[12], apH[12], stzHp;
    std::vector<float> delay; int dpos = 0;
};

inline AzTrackEqPlugin* findTrackEq (te::AudioTrack& t) { for (auto p : t.pluginList.getPlugins()) if (auto* e = dynamic_cast<AzTrackEqPlugin*> (p)) return e; return nullptr; }
inline AzImagerPlugin* findImager (te::AudioTrack& t)   { for (auto p : t.pluginList.getPlugins()) if (auto* e = dynamic_cast<AzImagerPlugin*> (p)) return e; return nullptr; }
// EQ va Imager'ni trekka qo'yish (fader oldidan, foydalanuvchi insertlaridan keyin)
inline void ensureTrackTools (te::AudioTrack& t)
{
    auto volIdx = [&t] { const int v = t.pluginList.getPlugins().indexOf (t.getVolumePlugin()); return v >= 0 ? v : t.pluginList.size(); };
    if (findTrackEq (t) == nullptr)
        if (auto p = t.edit.getPluginCache().createNewPlugin (AzTrackEqPlugin::xmlTypeName, {})) t.pluginList.insertPlugin (p, volIdx(), nullptr);
    if (findImager (t) == nullptr)
        if (auto p = t.edit.getPluginCache().createNewPlugin (AzImagerPlugin::xmlTypeName, {})) t.pluginList.insertPlugin (p, volIdx(), nullptr);
}

// =============================================================================  EQ ko'rinishi (Ozone Equalizer)
class EqView : public juce::Component, private juce::Timer
{
public:
    explicit EqView (bool compactMode = true) : compact (compactMode)
    {
        addAndMakeVisible (bypassB); addAndMakeVisible (bigB);
        bypassB.setButtonText ("EQ"); bypassB.setClickingTogglesState (true); bypassB.getProperties().set ("radius", 4.0f);
        bypassB.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff4ad6ff));
        bypassB.setTooltip (U ("AZ EQ yoqish / bypass"));
        bypassB.onClick = [this] { if (eq) eq->setEnabled (bypassB.getToggleState()); };
        bigB.setButtonText (juce::String::fromUTF8 ("\xe2\xa4\xa2")); bigB.getProperties().set ("radius", 4.0f);
        bigB.setTooltip (U ("Katta oynada ochish")); bigB.onClick = [this] { if (eq) eq->showWindowExplicitly(); };
        bigB.setVisible (compact);
        for (auto& v : spec) v = -120.0f;
        startTimerHz (25);
    }
    void setPlugins (AzTrackEqPlugin* e, AzImagerPlugin* im)
    {
        eq = e; imager = im;
        bypassB.setToggleState (eq != nullptr && eq->isEnabled(), juce::dontSendNotification);
        for (auto& v : spec) v = -120.0f;
        repaint();
    }
    void resized() override
    {
        bigB.setBounds (getWidth() - 24, 3, 20, 16);
        bypassB.setBounds (getWidth() - (compact ? 56 : 34), 3, 30, 16);
    }
    juce::Rectangle<float> plot() const { return getLocalBounds().toFloat().reduced (2).withTrimmedTop (compact ? 20.0f : 24.0f).withTrimmedBottom (compact ? 2.0f : 16.0f); }
    float gainToY (double db) const { auto p = plot(); return p.getCentreY() - (float) (db / range) * p.getHeight() * 0.5f; }
    double yToGain (float y) const { auto p = plot(); return (p.getCentreY() - y) / (p.getHeight() * 0.5f) * range; }
    juce::Point<float> nodePos (int b) const
    {
        auto p = plot();
        const int t = (int) eq->get (b, "t");
        return { p.getX() + tt::hzToX (eq->get (b, "f"), p.getWidth()), gainToY (t >= 3 ? 0.0 : eq->get (b, "g")) };
    }
    int nodeAt (juce::Point<float> pt) const
    {
        if (! eq) return -1;
        for (int b = AzTrackEqPlugin::numBands - 1; b >= 0; --b) if (nodePos (b).getDistanceFrom (pt) < (compact ? 7.0f : 9.0f)) return b;
        return -1;
    }
    int crossoverAt (juce::Point<float> pt) const
    {
        if (! imager) return -1;
        auto p = plot();
        if (pt.y > p.getY() + 14) return -1;
        for (int i = 0; i < 3; ++i) if (std::abs (p.getX() + tt::hzToX (imager->crossover (i), p.getWidth()) - pt.x) < 6) return i;
        return -1;
    }
    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        auto b = getLocalBounds().toFloat();
        g.setColour (Colour (0xff09080c)); g.fillRoundedRectangle (b, 7);
        g.setColour (Colours::white.withAlpha (0.08f)); g.drawRoundedRectangle (b.reduced (0.5f), 7, 1);
        caps (g, compact ? U ("AZ EQ \xc2\xb7 CROSSOVER") : U ("AZ EQ  \xc2\xb7  OZONE USLUBIDA"), { 8, 4, 200, 12 }, 8, Colour (0xff4ad6ff));
        auto p = plot();
        // to'r
        for (double f : { 100.0, 1000.0, 10000.0 })
        {
            const float x = p.getX() + tt::hzToX (f, p.getWidth());
            g.setColour (Colours::white.withAlpha (0.06f)); g.drawVerticalLine ((int) x, p.getY(), p.getBottom());
            if (! compact) label (g, tt::hzText (f), { x + 2, p.getBottom() + 2, 40, 12 }, 8.5f, dim2, Justification::centredLeft, 500);
        }
        for (double d : { -12.0, 0.0, 12.0 }) { g.setColour (Colours::white.withAlpha (d == 0 ? 0.10f : 0.04f)); g.drawHorizontalLine ((int) gainToY (d), p.getX(), p.getRight()); }
        if (! eq) { label (g, U ("Trek tanlang"), p, 10, dim, Justification::centred, 500); return; }
        // spektr
        {
            Path s; bool first = true;
            for (int i = 0; i < bins; ++i)
            {
                const float x = p.getX() + p.getWidth() * i / (bins - 1);
                const float y = jlimit (p.getY(), p.getBottom(), p.getBottom() - (spec[(size_t) i] + 90.0f) / 90.0f * p.getHeight());
                if (first) { s.startNewSubPath (x, p.getBottom()); first = false; }
                s.lineTo (x, y);
            }
            s.lineTo (p.getRight(), p.getBottom()); s.closeSubPath();
            g.setGradientFill (ColourGradient (Colour (0xff4ad6ff).withAlpha (0.35f), 0, p.getY(), Colour (0xff4ad6ff).withAlpha (0.04f), 0, p.getBottom(), false));
            g.fillPath (s);
        }
        // crossover chiziqlari (Imager)
        if (imager)
            for (int i = 0; i < 3; ++i)
            {
                const float x = p.getX() + tt::hzToX (imager->crossover (i), p.getWidth());
                Path dl; dl.startNewSubPath (x, p.getY()); dl.lineTo (x, p.getBottom());
                PathStrokeType st (1.0f); float dash[] = { 3, 3 }; Path d2; st.createDashedStroke (d2, dl, dash, 2);
                g.setColour ((dragX == i ? Colours::white : Colour (0xffb98aff)).withAlpha (0.7f)); g.fillPath (d2);
                auto hb = Rectangle<float> (x - 6, p.getY(), 12, 11);
                g.setColour (Colour (0xffb98aff)); g.fillRoundedRectangle (hb, 2);
                label (g, String (i + 1), hb, 7.5f, Colours::black, Justification::centred, 800);
            }
        // EQ egri chizig'i
        {
            Path c;
            for (int i = 0; i <= (int) p.getWidth(); i += 2)
            {
                const double f = tt::xToHz ((float) i, p.getWidth());
                const float y = jlimit (p.getY(), p.getBottom(), gainToY (eq->responseDb (f)));
                if (i == 0) c.startNewSubPath (p.getX() + i, y); else c.lineTo (p.getX() + i, y);
            }
            g.setColour (eq->isEnabled() ? Colour (0xffffc04a) : dim); g.strokePath (c, PathStrokeType (compact ? 1.5f : 2.0f));
        }
        // nuqtalar
        for (int bd = 0; bd < AzTrackEqPlugin::numBands; ++bd)
        {
            const auto np = nodePos (bd); const bool on = (bool) (int) eq->get (bd, "on");
            const float r = (compact ? 5.5f : 8.0f) * (hover == bd || drag == bd ? 1.25f : 1.0f);
            g.setColour (on ? trackColours[(bd + 1) % 8] : Colours::white.withAlpha (0.15f)); g.fillEllipse (np.x - r, np.y - r, r * 2, r * 2);
            g.setColour (Colours::black.withAlpha (0.6f)); g.drawEllipse (np.x - r, np.y - r, r * 2, r * 2, 1.0f);
            label (g, String (bd + 1), { np.x - r, np.y - r, r * 2, r * 2 }, compact ? 7.5f : 9.5f, on ? Colours::black : dim, Justification::centred, 800);
        }
        if (drag >= 0 || hover >= 0)
        {
            const int bd = drag >= 0 ? drag : hover; const int t = (int) eq->get (bd, "t");
            const char* tn[] = { "Bell", "Low shelf", "High shelf", "Low cut", "High cut" };
            String s = String (bd + 1) + ": " + tn[jlimit (0, 4, t)] + "  " + tt::hzText (eq->get (bd, "f")) + " Hz";
            if (t < 3) s << "  " << String (eq->get (bd, "g"), 1) << " dB";
            s << "  Q " << String (eq->get (bd, "q"), 2);
            label (g, s, { p.getX() + 4, p.getBottom() - 13, p.getWidth() - 8, 12 }, 8.5f, text, Justification::centredLeft, 600);
        }
        else if (dragX >= 0 && imager) label (g, "Crossover " + String (dragX + 1) + ": " + tt::hzText (imager->crossover (dragX)) + " Hz", { p.getX() + 4, p.getBottom() - 13, p.getWidth() - 8, 12 }, 8.5f, text, Justification::centredLeft, 600);
    }
    void mouseMove (const juce::MouseEvent& e) override { const int h = nodeAt (e.position); if (h != hover) { hover = h; repaint(); } }
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! eq) return;
        drag = nodeAt (e.position); dragX = drag < 0 ? crossoverAt (e.position) : -1;
        if (drag >= 0 && e.mods.isPopupMenu()) { typeMenu (drag); drag = -1; return; }
        if (drag >= 0 || dragX >= 0) eq->edit.getUndoManager().beginNewTransaction ("AZ EQ");
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! eq) return;
        auto p = plot();
        if (dragX >= 0 && imager) { imager->setCrossover (dragX, tt::xToHz (e.position.x - p.getX(), p.getWidth())); repaint(); return; }
        if (drag < 0) return;
        eq->set (drag, "f", std::round (juce::jlimit (20.0, 20000.0, tt::xToHz (e.position.x - p.getX(), p.getWidth()))));
        if ((int) eq->get (drag, "t") < 3) eq->set (drag, "g", std::round (juce::jlimit (-range, range, yToGain (e.position.y)) * 10.0) / 10.0);
        if (! (bool) (int) eq->get (drag, "on")) eq->set (drag, "on", 1.0);
        repaint();
    }
    void mouseUp (const juce::MouseEvent&) override { drag = -1; dragX = -1; repaint(); }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (! eq) return;
        if (const int b = nodeAt (e.position); b >= 0)
        {
            if (std::abs (eq->get (b, "g")) > 0.01) eq->set (b, "g", 0.0);       // 0 dB ga qaytarish
            else eq->set (b, "on", (bool) (int) eq->get (b, "on") ? 0.0 : 1.0);    // yoqish / o'chirish
            repaint();
        }
    }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (! eq) return;
        const int b = nodeAt (e.position);
        if (b < 0) { Component::mouseWheelMove (e, w); return; }
        const double q = eq->get (b, "q") * std::pow (1.15, w.deltaY > 0 ? 1.0 : -1.0);
        eq->set (b, "q", juce::jlimit (0.2, 12.0, q)); repaint();
    }
    void typeMenu (int b)
    {
        juce::PopupMenu m; const int t = (int) eq->get (b, "t");
        const char* tn[] = { "Bell", "Low shelf", "High shelf", "Low cut (HP)", "High cut (LP)" };
        for (int i = 0; i < 5; ++i) m.addItem (i + 1, tn[i], true, t == i);
        m.addSeparator(); m.addItem (10, (bool) (int) eq->get (b, "on") ? U ("O'chirish") : U ("Yoqish")); m.addItem (11, U ("Tiklash (0 dB)"));
        juce::Component::SafePointer<EqView> sp (this);
        m.showMenuAsync ({}, [sp, b] (int r)
        {
            if (sp == nullptr || sp->eq == nullptr || r == 0) return;
            auto* e = sp->eq;
            if (r <= 5) { e->set (b, "t", r - 1); e->set (b, "on", 1.0); }
            if (r == 10) e->set (b, "on", (bool) (int) e->get (b, "on") ? 0.0 : 1.0);
            if (r == 11) { e->set (b, "g", 0.0); e->set (b, "q", AzTrackEqPlugin::defs[b].q); }
            sp->repaint();
        });
    }
private:
    void timerCallback() override
    {
        if (! eq || ! isShowing()) return;
        float tmp[8192]; const int got = eq->analysis.read (tmp, 8192);
        for (int i = 0; i < got; ++i) { input[(size_t) inPos] = tmp[i]; inPos = (inPos + 1) % fftSize; ++fresh; }
        if (fresh >= fftSize / 2)
        {
            fresh = 0;
            std::array<float, fftSize * 2> buf {};
            for (int i = 0; i < fftSize; ++i)
                buf[(size_t) i] = input[(size_t) ((inPos + i) % fftSize)] * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (fftSize - 1)));
            fft.performFrequencyOnlyForwardTransform (buf.data());
            const double sr = eq->sampleRate;
            for (int b = 0; b < bins; ++b)
            {
                const double f = 20.0 * std::pow (1000.0, (double) b / (bins - 1));
                const int k0 = juce::jlimit (1, fftSize / 2 - 1, (int) (f / std::pow (1000.0, 0.5 / bins) / sr * fftSize)), k1 = juce::jlimit (k0, fftSize / 2 - 1, (int) (f * std::pow (1000.0, 0.5 / bins) / sr * fftSize));
                float m = 0; for (int k = k0; k <= k1; ++k) m = juce::jmax (m, buf[(size_t) k]);
                const float db = juce::Decibels::gainToDecibels (m / (fftSize * 0.25f), -120.0f) + 3.0f * (float) std::log2 (f / 1000.0);
                spec[(size_t) b] = db > spec[(size_t) b] ? db : spec[(size_t) b] + (db - spec[(size_t) b]) * 0.35f;
            }
        }
        else for (auto& v : spec) v = juce::jmax (-120.0f, v - 1.5f);
        repaint();
    }
    static constexpr int fftOrder = 11, fftSize = 1 << fftOrder, bins = 96;
    static constexpr double range = 18.0;
    bool compact = true;
    AzTrackEqPlugin* eq = nullptr; AzImagerPlugin* imager = nullptr;
    juce::TextButton bypassB, bigB;
    int hover = -1, drag = -1, dragX = -1;
    juce::dsp::FFT fft { fftOrder };
    std::array<float, fftSize> input {}; int inPos = 0, fresh = 0;
    std::array<float, bins> spec {};
};

// =============================================================================  Imager ko'rinishi (Ozone Imager)
class ImagerView : public juce::Component, private juce::Timer
{
public:
    explicit ImagerView (bool compactMode = true) : compact (compactMode)
    {
        const char* modes[] = { "Polar Sample", "Polar Level", "Lissajous" };
        for (int i = 0; i < 3; ++i)
        {
            auto* b = modeB.add (new juce::TextButton (modes[i])); addAndMakeVisible (b);
            b->setClickingTogglesState (true); b->setRadioGroupId (8811); b->getProperties().set ("radius", 3.0f);
            b->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff4ad6ff));
            b->onClick = [this, i] { if (modeB[i]->getToggleState()) { mode = i; repaint(); } };
        }
        modeB[0]->setToggleState (true, juce::dontSendNotification);
        addAndMakeVisible (stzB); stzB.setButtonText ("Stereoize"); stzB.setClickingTogglesState (true); stzB.getProperties().set ("radius", 3.0f);
        stzB.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff4ad6ff));
        stzB.setTooltip (U ("Stereoize: mono / tor yozuvga sun'iy kenglik (pastki polosa mono qoladi)"));
        stzB.onClick = [this] { if (im) im->set ("stz", stzB.getToggleState() ? 1.0 : 0.0); };
        addAndMakeVisible (bypassB); bypassB.setButtonText ("IMG"); bypassB.setClickingTogglesState (true); bypassB.getProperties().set ("radius", 4.0f);
        bypassB.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff4ad6ff)); bypassB.setTooltip (U ("AZ Imager yoqish / bypass"));
        bypassB.onClick = [this] { if (im) im->setEnabled (bypassB.getToggleState()); };
        addAndMakeVisible (bigB); bigB.setButtonText (juce::String::fromUTF8 ("\xe2\xa4\xa2")); bigB.getProperties().set ("radius", 4.0f);
        bigB.setTooltip (U ("Katta oynada ochish")); bigB.onClick = [this] { if (im) im->showWindowExplicitly(); };
        bigB.setVisible (compact);
        startTimerHz (30);
    }
    void setPlugin (AzImagerPlugin* p)
    {
        im = p; pts.clear();
        stzB.setToggleState (im && (bool) (int) im->get ("stz"), juce::dontSendNotification);
        bypassB.setToggleState (im && im->isEnabled(), juce::dontSendNotification);
        repaint();
    }
    juce::Rectangle<float> scopeArea() const { auto b = getLocalBounds().toFloat().reduced (6); return b.withTrimmedTop (18).withHeight (b.getHeight() * 0.44f); }
    juce::Rectangle<float> bandsArea() const { auto b = getLocalBounds().toFloat().reduced (6); return b.withTrimmedTop (scopeArea().getBottom() - b.getY() + (compact ? 50.0f : 58.0f)).withTrimmedBottom (compact ? 16.0f : 22.0f); }
    juce::Rectangle<float> bandCol (int i) const { auto a = bandsArea(); const float w = a.getWidth() / 4; return { a.getX() + i * w, a.getY(), w, a.getHeight() }; }
    float widthToY (int i, double w) const { auto c = bandCol (i).reduced (0, 10); return c.getBottom() - (float) ((w + 100.0) / 200.0) * c.getHeight(); }
    double yToWidth (int i, float y) const { auto c = bandCol (i).reduced (0, 10); return (c.getBottom() - y) / c.getHeight() * 200.0 - 100.0; }
    void resized() override
    {
        bigB.setBounds (getWidth() - 24, 3, 20, 16);
        bypassB.setBounds (getWidth() - (compact ? 58 : 36), 3, 32, 16);
        auto sa = scopeArea();
        const int my = (int) sa.getBottom() + 18, mw = (int) ((getWidth() - 12) / 3);
        for (int i = 0; i < 3; ++i) modeB[i]->setBounds (6 + i * mw, my, mw - 3, compact ? 15 : 18);
        stzB.setBounds (getWidth() - 6 - (compact ? 62 : 80), (int) bandsArea().getBottom() + 2, compact ? 62 : 80, compact ? 14 : 18);
    }
    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        auto b = getLocalBounds().toFloat();
        g.setColour (Colour (0xff09080c)); g.fillRoundedRectangle (b, 7);
        g.setColour (Colours::white.withAlpha (0.08f)); g.drawRoundedRectangle (b.reduced (0.5f), 7, 1);
        caps (g, compact ? juce::String ("AZ IMAGER") : U ("AZ IMAGER  \xc2\xb7  OZONE USLUBIDA"), { 8, 4, 200, 12 }, 8, Colour (0xff4ad6ff));
        // vektorskop
        auto sa = scopeArea();
        const float cx = sa.getCentreX(), cy = mode == 2 ? sa.getCentreY() : sa.getBottom() - 2, R = mode == 2 ? jmin (sa.getWidth(), sa.getHeight()) * 0.5f : jmin (sa.getWidth() * 0.5f, sa.getHeight() - 4);
        g.setColour (Colours::white.withAlpha (0.10f));
        if (mode == 2) { g.drawEllipse (cx - R, cy - R, R * 2, R * 2, 1); g.drawLine (cx - R, cy, cx + R, cy, 1); g.drawLine (cx, cy - R, cx, cy + R, 1); }
        else
        {
            Path arc; arc.addCentredArc (cx, cy, R, R, 0, -MathConstants<float>::halfPi, MathConstants<float>::halfPi, true); g.strokePath (arc, PathStrokeType (1));
            for (float a : { -0.785f, 0.0f, 0.785f }) g.drawLine (cx, cy, cx + std::sin (a) * R, cy - std::cos (a) * R, 1);
            label (g, "L", { cx - R * 0.78f - 8, cy - R * 0.78f - 6, 12, 12 }, 8, dim, Justification::centred, 700);
            label (g, "R", { cx + R * 0.78f - 4, cy - R * 0.78f - 6, 12, 12 }, 8, dim, Justification::centred, 700);
        }
        if (mode == 1)
        {   // Polar Level: burchak bo'yicha o'rtacha daraja nurlari
            for (int k = 0; k < levelBins; ++k)
            {
                const float a = -MathConstants<float>::halfPi + MathConstants<float>::pi * (k + 0.5f) / levelBins;
                const float len = jlimit (0.0f, 1.0f, levels[(size_t) k]) * R;
                g.setColour (Colour (0xff4ad6ff).withAlpha (0.75f));
                g.drawLine (cx, cy, cx + std::sin (a) * len, cy - std::cos (a) * len, jmax (1.5f, R * 0.04f));
            }
        }
        else
        {
            g.setColour (Colour (0xff4ad6ff).withAlpha (0.55f));
            for (auto& pt : pts)
            {
                float x, y;
                if (mode == 2) { x = cx + (pt.x - pt.y) * 0.7071f * R; y = cy - (pt.x + pt.y) * 0.7071f * R; }   // Lissajous (45° burilgan)
                else
                {
                    const float m = (pt.x + pt.y) * 0.7071f, s = (pt.x - pt.y) * 0.7071f;
                    const float rad = jmin (1.0f, std::sqrt (m * m + s * s)), ang = std::atan2 (-s, std::abs (m));   // -pi/2..pi/2 (chap = L)
                    x = cx + std::sin (ang) * rad * R; y = cy - std::cos (ang) * rad * R;
                }
                g.fillRect (x, y, 1.4f, 1.4f);
            }
        }
        // korrelyatsiya
        {
            auto cr = Rectangle<float> (sa.getX() + 18, sa.getBottom() + 4, sa.getWidth() - 36, 6);
            g.setColour (Colours::white.withAlpha (0.08f)); g.fillRoundedRectangle (cr, 3);
            const float x = cr.getX() + (corr + 1.0f) * 0.5f * cr.getWidth();
            g.setColour (corr < 0 ? red : corr < 0.3f ? yellow : green); g.fillRoundedRectangle (x - 3, cr.getY() - 1, 6, cr.getHeight() + 2, 2);
            label (g, "-1", { sa.getX(), cr.getY() - 3, 16, 12 }, 7.5f, dim2, Justification::centred, 600);
            label (g, "+1", { cr.getRight() + 2, cr.getY() - 3, 16, 12 }, 7.5f, dim2, Justification::centred, 600);
        }
        if (! im) { label (g, U ("Trek tanlang"), bandsArea(), 10, dim, Justification::centred, 500); return; }
        // polosalar (kenglik)
        auto ba = bandsArea();
        g.setColour (Colours::white.withAlpha (0.03f)); g.fillRoundedRectangle (ba, 4);
        for (int i = 0; i < 4; ++i)
        {
            auto c = bandCol (i);
            const float lx = c.getCentreX();
            g.setColour (Colours::white.withAlpha (0.12f)); g.drawVerticalLine ((int) lx, c.getY() + 10, c.getBottom() - 10);
            g.setColour (Colours::white.withAlpha (0.18f)); g.drawHorizontalLine ((int) widthToY (i, 0.0), lx - 6, lx + 6);
            const double w = im->width (i); const float y = widthToY (i, w);
            const Colour col = w > 0.05 ? Colour (0xff4ad6ff) : w < -0.05 ? yellow : Colours::white;
            g.setColour (col.withAlpha (0.35f)); g.fillRect (lx - 1.5f, jmin (y, widthToY (i, 0.0)), 3.0f, std::abs (y - widthToY (i, 0.0)));
            const float r = compact ? 5.0f : 7.0f;
            g.setColour (col); g.fillEllipse (lx - r, y - r, r * 2, r * 2);
            g.setColour (Colours::black.withAlpha (0.5f)); g.drawEllipse (lx - r, y - r, r * 2, r * 2, 1);
            label (g, String (w, 1), { c.getX(), c.getBottom() - 10, c.getWidth(), 12 }, compact ? 9.0f : 11.0f, text, Justification::centred, 700);
            label (g, String (i + 1), { c.getX() + 3, c.getY(), 10, 10 }, 7.5f, dim2, Justification::centredLeft, 700);
            if (i < 3)
            {
                label (g, tt::hzText (im->crossover (i)), { c.getRight() - 16, c.getY() - 12, 32, 10 }, 7.5f, Colour (0xffb98aff), Justification::centred, 700);
                g.setColour (Colour (0xffb98aff).withAlpha (0.3f)); g.drawVerticalLine ((int) c.getRight(), c.getY(), c.getBottom());
            }
        }
        label (g, U ("kenglik: -100 mono \xc2\xb7 0 asl \xc2\xb7 +100 keng"), { ba.getX(), ba.getBottom() + 2, ba.getWidth() - (compact ? 64.0f : 84.0f), 14 }, 7.5f, dim2, Justification::centredLeft, 500);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        dragBand = -1; dragX = -1;
        if (! im) return;
        for (int i = 0; i < 4; ++i)
        {
            auto c = bandCol (i);
            if (i < 3 && std::abs (e.position.x - c.getRight()) < 4 && e.position.y < c.getY() + 12) { dragX = i; break; }
            if (c.contains (e.position)) { dragBand = i; break; }
        }
        if (dragBand >= 0 || dragX >= 0) { im->edit.getUndoManager().beginNewTransaction ("AZ Imager"); xStart = dragX >= 0 ? im->crossover (dragX) : 0; mouseDrag (e); }
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! im) return;
        if (dragBand >= 0) { im->setWidth (dragBand, yToWidth (dragBand, e.position.y)); repaint(); }
        if (dragX >= 0) { im->setCrossover (dragX, xStart * std::pow (2.0, e.getDistanceFromDragStartX() / 40.0)); repaint(); }
    }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (! im) return;
        for (int i = 0; i < 4; ++i) if (bandCol (i).contains (e.position)) { im->setWidth (i, 0.0); repaint(); }
    }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (! im) return;
        for (int i = 0; i < 4; ++i)
            if (bandCol (i).contains (e.position)) { im->setWidth (i, im->width (i) + (w.deltaY > 0 ? 2.0 : -2.0)); repaint(); return; }
        Component::mouseWheelMove (e, w);
    }
    float correlation() const { return corr; }
private:
    void timerCallback() override
    {
        if (! im || ! isShowing()) return;
        float tmp[4096 * 2]; const int got = im->scope.read (tmp, 4096);
        double sLR = 0, sLL = 0, sRR = 0;
        pts.clear();
        for (auto& l : levels) l *= 0.85f;
        const int step = juce::jmax (1, got / 600);
        for (int i = 0; i < got; ++i)
        {
            const float l = tmp[i * 2], r = tmp[i * 2 + 1];
            sLR += l * r; sLL += l * l; sRR += r * r;
            if (i % step == 0)
            {
                const float k = 1.6f;
                pts.add ({ juce::jlimit (-1.2f, 1.2f, l * k), juce::jlimit (-1.2f, 1.2f, r * k) });
                const float m = (l + r) * 0.7071f, s = (l - r) * 0.7071f, mag = std::sqrt (m * m + s * s);
                if (mag > 1e-4f)
                {
                    const float ang = std::atan2 (-s, std::abs (m));
                    const int bin = juce::jlimit (0, levelBins - 1, (int) ((ang / juce::MathConstants<float>::pi + 0.5f) * levelBins));
                    levels[(size_t) bin] = juce::jmax (levels[(size_t) bin], juce::jmin (1.0f, mag * 2.0f));
                }
            }
        }
        if (got > 64 && sLL + sRR > 1e-9) { const float c = (float) (sLR / std::sqrt (sLL * sRR + 1e-18)); corr += (c - corr) * 0.3f; }
        else corr += (1.0f - corr) * 0.05f;
        repaint();
    }
    static constexpr int levelBins = 31;
    bool compact = true; int mode = 0;
    AzImagerPlugin* im = nullptr;
    juce::OwnedArray<juce::TextButton> modeB; juce::TextButton stzB, bypassB, bigB;
    juce::Array<juce::Point<float>> pts; std::array<float, levelBins> levels {};
    float corr = 1.0f; int dragBand = -1, dragX = -1; double xStart = 0;
};

// katta oynalar uchun muharrirlar
class EqEditor : public te::Plugin::EditorComponent
{
public:
    explicit EqEditor (AzTrackEqPlugin& p) : view (false)
    {
        addAndMakeVisible (view);
        AzImagerPlugin* im = nullptr;
        if (auto* t = dynamic_cast<te::AudioTrack*> (p.getOwnerTrack())) im = findImager (*t);
        view.setPlugins (&p, im);
        setSize (760, 360);
    }
    bool allowWindowResizing() override { return false; }
    juce::ComponentBoundsConstrainer* getBoundsConstrainer() override { return nullptr; }
    void resized() override { view.setBounds (getLocalBounds().reduced (8)); }
    void paint (juce::Graphics& g) override { g.fillAll (bg0); }
    EqView view;
};
class ImagerEditor : public te::Plugin::EditorComponent
{
public:
    explicit ImagerEditor (AzImagerPlugin& p) : view (false) { addAndMakeVisible (view); view.setPlugin (&p); setSize (460, 520); }
    bool allowWindowResizing() override { return false; }
    juce::ComponentBoundsConstrainer* getBoundsConstrainer() override { return nullptr; }
    void resized() override { view.setBounds (getLocalBounds().reduced (8)); }
    void paint (juce::Graphics& g) override { g.fillAll (bg0); }
    ImagerView view;
};
inline std::unique_ptr<te::Plugin::EditorComponent> AzTrackEqPlugin::createEditor() { return std::make_unique<EqEditor> (*this); }
inline std::unique_ptr<te::Plugin::EditorComponent> AzImagerPlugin::createEditor()  { return std::make_unique<ImagerEditor> (*this); }
}
