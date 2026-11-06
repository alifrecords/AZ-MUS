// =============================================================================
//  AZ MUS — marshrutlash: Control Room, Group/FX kanallar va sendlar,
//  External FX / External Instruments (Cubase "Audio Connections" mantiqi)
// =============================================================================
#pragma once
#include "AppEngine.h"

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

inline void registerAzPlugins (te::Engine& e) { e.getPluginManager().createBuiltInType<ControlRoomPlugin>(); }

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
