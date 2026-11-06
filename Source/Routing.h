// =============================================================================
//  AZ MUS — marshrutlash: Control Room, Group/FX kanallar va sendlar,
//  External FX / External Instruments (Cubase "Audio Connections" mantiqi)
// =============================================================================
#pragma once
#include "AppEngine.h"
#include "MonitorMatchEngine.h"

namespace az
{
// ---------------------------------------------------------------- Control Room
// Master oxirida turadi: monitor balandligi, Dim (-20 dB), Mono, Mute.
// Eksport va bounce paytida avtomatik o'chiriladi — miksga ta'sir qilmaydi.
class ControlRoomPlugin : public te::Plugin
{
public:
    static const char* getPluginName() { return "AZ Control Room"; }
    static inline const char* xmlTypeName = "azcontrolroom";

    explicit ControlRoomPlugin (te::PluginCreationInfo info) : te::Plugin (info)
    {
        auto um = getUndoManager();
        level.referTo (state, "crLevel", um, 0.0f);
        dim.referTo (state, "crDim", um, false);
        mono.referTo (state, "crMono", um, false);
        mute.referTo (state, "crMute", um, false);
    }
    ~ControlRoomPlugin() override { notifyListenersOfDeletion(); }

    juce::String getName() const override                 { return getPluginName(); }
    juce::String getPluginType() override                 { return xmlTypeName; }
    juce::String getSelectableDescription() override      { return getName(); }
    void initialise (const te::PluginInitialisationInfo&) override {}
    void deinitialise() override {}
    BusLayout getBusses() const override                  { return BusLayout::singlePassThrough(); }

    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (! isEnabled() || fc.destBuffer == nullptr) return;
        auto& b = *fc.destBuffer;
        const int s = fc.bufferStartSample, n = fc.bufferNumSamples;
        if (mono.get() && b.getNumChannels() >= 2)
        {
            auto* l = b.getWritePointer (0, s); auto* r = b.getWritePointer (1, s);
            for (int i = 0; i < n; ++i) { const float m = 0.5f * (l[i] + r[i]); l[i] = r[i] = m; }
        }
        const float target = mute.get() ? 0.0f : juce::Decibels::decibelsToGain (level.get() - (dim.get() ? 20.0f : 0.0f), -100.0f);
        for (int c = 0; c < b.getNumChannels(); ++c) b.applyGainRamp (c, s, n, lastGain, target);
        lastGain = target;
    }
    void restorePluginStateFromValueTree (const juce::ValueTree& v) override { te::copyPropertiesToCachedValues (v, level, dim, mono, mute); }

    juce::CachedValue<float> level;
    juce::CachedValue<bool> dim, mono, mute;
private:
    float lastGain = 1.0f;
};

// ---------------------------------------------------------------- Monitor Match (Control Room insert)
// Sizning monitoringiz (KRK, ADAM, Yamaha HS5, JBL ...) chastota javobini maqsad monitorga
// (Genelec, Focal, NS-10, Auratone, smartfon, mashina ...) aylantiradi. Kechikishsiz (minimum phase) FIR.
// Faqat Control Room zanjirida turadi: eksport/bounce'ga hech qachon tushmaydi.
class MonitorMatchPlugin : public te::Plugin, private juce::AsyncUpdater
{
public:
    static const char* getPluginName() { return "AZ Monitor Match"; }
    static inline const char* xmlTypeName = "azmonitormatch";

    explicit MonitorMatchPlugin (te::PluginCreationInfo info) : te::Plugin (info)
    {
        auto um = getUndoManager();
        source.referTo (state, "mmSource", um, 0);
        target.referTo (state, "mmTarget", um, 1);
        amount.referTo (state, "mmAmount", um, 1.0f);
        bass.referTo (state, "mmBass", um, 0.85f);
        autoGain.referTo (state, "mmAutoGain", um, true);
        custom.referTo (state, "mmCustom", um, {});
        customName.referTo (state, "mmCustomName", um, {});
    }
    ~MonitorMatchPlugin() override { cancelPendingUpdate(); notifyListenersOfDeletion(); }

    juce::String getName() const override                 { return getPluginName(); }
    juce::String getPluginType() override                 { return xmlTypeName; }
    juce::String getSelectableDescription() override      { return getName(); }
    BusLayout getBusses() const override                  { return BusLayout::singlePassThrough(); }

    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sampleRate = info.sampleRate > 1000 ? info.sampleRate : 48000.0;
        conv.prepare ({ sampleRate, (juce::uint32) juce::jmax (info.blockSizeSamples, 8192), 2 });
        rebuild();
    }
    void deinitialise() override {}

    void valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& id) override
    {
        te::Plugin::valueTreePropertyChanged (v, id);
        if (v == state && id.toString().startsWith ("mm")) triggerAsyncUpdate();
    }

    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (! isEnabled() || fc.destBuffer == nullptr || ! ready.load()) return;
        juce::dsp::AudioBlock<float> block (*fc.destBuffer);
        auto sub = block.getSubBlock ((size_t) fc.bufferStartSample, (size_t) fc.bufferNumSamples)
                        .getSubsetChannelBlock (0, std::min<size_t> (2, block.getNumChannels()));
        conv.process (juce::dsp::ProcessContextReplacing<float> (sub));
    }
    void restorePluginStateFromValueTree (const juce::ValueTree& v) override
    {
        te::copyPropertiesToCachedValues (v, source, target, amount, bass, autoGain, custom, customName);
        triggerAsyncUpdate();
    }

    // ---- foydalanuvchi o'lchovi (REW: File → Export → Export measurement as text)
    bool loadMeasurement (const juce::File& f)
    {
        auto pts = mm::parseMeasurementText (f.loadFileAsString().toStdString());
        if (pts.size() < 4) return false;
        juce::String sstr;
        for (auto& p : pts) sstr << juce::String (p.hz, 1) << ":" << juce::String (p.db, 2) << ";";
        customName = f.getFileNameWithoutExtension();
        custom = sstr;
        source = (int) mm::sourceProfiles().size();
        return true;
    }
    std::vector<mm::CurvePoint> customPoints() const
    {
        std::vector<mm::CurvePoint> r;
        for (auto& t : juce::StringArray::fromTokens (custom.get(), ";", {}))
            if (t.contains (":")) r.push_back ({ t.upToFirstOccurrenceOf (":", false, false).getDoubleValue(), t.fromFirstOccurrenceOf (":", false, false).getDoubleValue() });
        return r;
    }
    mm::Settings settings() const
    {
        mm::Settings s;
        s.source = juce::jlimit (0, (int) mm::sourceProfiles().size(), source.get());
        s.target = juce::jlimit (0, (int) mm::targetProfiles().size() - 1, target.get());
        s.amount = juce::jlimit (0.0f, 1.0f, amount.get()); s.bass = juce::jlimit (0.0f, 1.0f, bass.get());
        s.minPhase = true; s.autoGain = autoGain.get();
        return s;
    }
    double correctionDbAt (double hz) const { mm::CorrectionModel m; m.custom = customPoints(); return m.correctionDb (settings(), hz); }

    static juce::String sourceName (int i)
    {
        auto& v = mm::sourceProfiles();
        if (i >= 0 && i < (int) v.size()) return juce::String (v[(size_t) i].brand) + " " + v[(size_t) i].model;
        return juce::String::fromUTF8 ("O'lchov fayli (REW)");
    }
    static juce::String targetName (int i)
    {
        auto& v = mm::targetProfiles(); i = juce::jlimit (0, (int) v.size() - 1, i);
        return juce::String (v[(size_t) i].brand) + " " + v[(size_t) i].model;
    }

    juce::CachedValue<int> source, target;
    juce::CachedValue<float> amount, bass;
    juce::CachedValue<bool> autoGain;
    juce::CachedValue<juce::String> custom, customName;
    std::atomic<bool> ready { false };
private:
    void handleAsyncUpdate() override { rebuild(); }
    void rebuild()
    {
        mm::CorrectionModel m; m.custom = customPoints();
        auto s = settings();
        if (s.source >= (int) mm::sourceProfiles().size() && m.custom.empty()) s.source = 0;
        const int n = mm::firLengthFor (sampleRate);
        const auto ir = m.buildImpulse (s, sampleRate, n);
        juce::AudioBuffer<float> b (1, n);
        std::copy (ir.begin(), ir.end(), b.getWritePointer (0));
        conv.loadImpulseResponse (std::move (b), sampleRate, juce::dsp::Convolution::Stereo::no,
                                  juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
        ready = true;
    }
    juce::dsp::Convolution conv { juce::dsp::Convolution::NonUniform { 512 } };
    double sampleRate = 48000.0;
};

inline void registerAzPlugins (te::Engine& e)
{
    e.getPluginManager().createBuiltInType<ControlRoomPlugin>();
    e.getPluginManager().createBuiltInType<MonitorMatchPlugin>();
}

// ---------------------------------------------------------------- tashqi qurilmalar ta'rifi
struct ExternalDef { juce::String kind, name, out, in; };   // kind: "fx" | "inst"

struct Routing
{
    explicit Routing (AppEngine& a) : app (a) {}
    AppEngine& app;

    // ---- Control Room
    ControlRoomPlugin* controlRoom()
    {
        if (! app.edit) return nullptr;
        for (auto p : app.edit->getMasterPluginList().getPlugins())
            if (auto* cr = dynamic_cast<ControlRoomPlugin*> (p)) return cr;
        auto p = app.edit->getPluginCache().createNewPlugin (ControlRoomPlugin::xmlTypeName, {});
        if (p == nullptr) return nullptr;
        app.edit->getMasterPluginList().insertPlugin (p, -1, nullptr);
        return dynamic_cast<ControlRoomPlugin*> (p.get());
    }
    // Control Room zanjiri: azcontrolroom'dan KEYINgi plaginlar (faqat monitor)
    static juce::Array<te::Plugin*> controlRoomInserts (te::Edit& e)
    {
        juce::Array<te::Plugin*> r; bool after = false;
        for (auto p : e.getMasterPluginList().getPlugins())
        {
            if (p->getPluginType() == ControlRoomPlugin::xmlTypeName) { after = true; continue; }
            if (after) r.add (p);
        }
        return r;
    }
    MonitorMatchPlugin* monitorMatch (bool create)
    {
        if (! app.edit) return nullptr;
        for (auto p : controlRoomInserts (*app.edit)) if (auto* mmp = dynamic_cast<MonitorMatchPlugin*> (p)) return mmp;
        if (! create) return nullptr;
        controlRoom();
        auto p = app.edit->getPluginCache().createNewPlugin (MonitorMatchPlugin::xmlTypeName, {});
        if (p == nullptr) return nullptr;
        app.edit->getMasterPluginList().insertPlugin (p, -1, nullptr);   // CR'dan keyin
        return dynamic_cast<MonitorMatchPlugin*> (p.get());
    }

    // ---- Group/FX kanallar
    juce::Array<te::AudioTrack*> fxTracks() const
    {
        juce::Array<te::AudioTrack*> r;
        if (app.edit) for (auto* t : te::getAudioTracks (*app.edit)) if (fxBus (*t) >= 0) r.add (t);
        return r;
    }
    static int fxBus (te::AudioTrack& t) { return (int) t.state.getProperty ("azFxBus", -1); }

    te::AudioTrack* addFxChannel (const juce::String& effectName, const juce::String& xmlType, const juce::PluginDescription& desc)
    {
        if (! app.edit) return nullptr;
        int bus = 0;
        for (auto* t : fxTracks()) bus = juce::jmax (bus, fxBus (*t) + 1);
        if (bus > 31) return nullptr;
        auto& um = app.edit->getUndoManager();
        um.beginNewTransaction ("FX");
        auto* t = app.addAudioTrack ("FX " + juce::String (bus + 1) + " - " + effectName);
        if (t == nullptr) return nullptr;
        t->state.setProperty ("azFxBus", bus, &um);
        t->setColour (juce::Colour (0xff5d5759));
        if (auto ret = app.edit->getPluginCache().createNewPlugin (te::AuxReturnPlugin::xmlTypeName, {}))
        {
            if (auto* ar = dynamic_cast<te::AuxReturnPlugin*> (ret.get())) ar->busNumber = bus;
            t->pluginList.insertPlugin (ret, 0, nullptr);
        }
        if (xmlType.isNotEmpty())
            if (auto fx = app.edit->getPluginCache().createNewPlugin (xmlType, desc))
                t->pluginList.insertPlugin (fx, 1, nullptr);
        app.edit->setAuxBusName (bus, effectName);
        return t;
    }

    static juce::Array<te::AuxSendPlugin*> sends (te::AudioTrack& t)
    {
        juce::Array<te::AuxSendPlugin*> r;
        for (auto p : t.pluginList.getPlugins()) if (auto* s = dynamic_cast<te::AuxSendPlugin*> (p)) r.add (s);
        return r;
    }

    te::AuxSendPlugin* addSend (te::AudioTrack& t, int bus, float db = -10.0f)
    {
        for (auto* s : sends (t)) if (s->getBusNumber() == bus) return s;
        auto p = app.edit->getPluginCache().createNewPlugin (te::AuxSendPlugin::xmlTypeName, {});
        auto* s = dynamic_cast<te::AuxSendPlugin*> (p.get());
        if (s == nullptr) return nullptr;
        s->busNumber = bus;
        // post-fader: volume plaginidan keyin
        int idx = t.pluginList.getPlugins().indexOf (t.getVolumePlugin());
        app.edit->getUndoManager().beginNewTransaction ("Send");
        t.pluginList.insertPlugin (p, idx < 0 ? -1 : idx + 1, nullptr);
        s->setGainDb (db);
        return s;
    }

    // ---- External FX / Instruments
    juce::Array<ExternalDef> externals() const
    {
        juce::Array<ExternalDef> r;
        if (auto xml = juce::parseXML (app.engine.getPropertyStorage().getPropertiesFile().getValue ("azExternal")))
            for (auto* e : xml->getChildIterator())
                r.add ({ e->getStringAttribute ("kind"), e->getStringAttribute ("name"), e->getStringAttribute ("out"), e->getStringAttribute ("in") });
        return r;
    }
    void setExternals (const juce::Array<ExternalDef>& list)
    {
        juce::XmlElement root ("EXTERNAL");
        for (auto& d : list)
        {
            auto* e = root.createNewChildElement ("DEV");
            e->setAttribute ("kind", d.kind); e->setAttribute ("name", d.name); e->setAttribute ("out", d.out); e->setAttribute ("in", d.in);
        }
        app.engine.getPropertyStorage().getPropertiesFile().setValue ("azExternal", root.toString());
    }
    juce::StringArray deviceNames (bool forInput)
    {
        juce::StringArray d, a; juce::BigInteger au, mi;
        te::InsertPlugin::getPossibleDeviceNames (app.engine, d, a, au, mi, forInput);
        return d;
    }
    te::Plugin::Ptr insertExternal (te::AudioTrack& t, const ExternalDef& d)
    {
        auto p = app.edit->getPluginCache().createNewPlugin (te::InsertPlugin::xmlTypeName, {});
        if (auto* ins = dynamic_cast<te::InsertPlugin*> (p.get()))
        {
            ins->name = d.name; ins->outputDevice = d.out; ins->inputDevice = d.in;
            ins->updateDeviceTypes();
            app.edit->getUndoManager().beginNewTransaction ("External");
            t.pluginList.insertPlugin (p, AppEngine::userPluginCount (t), nullptr);
        }
        return p;
    }
};
}
