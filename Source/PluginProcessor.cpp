#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

namespace
{
inline juce::String U (const char* s) { return juce::String::fromUTF8 (s); }

// Delky not v dobach (ctvrtkach) - poradi musi sedet s nazvy nize.
const float kDelayBeats[]    = { 0.125f, 1.0f / 6.0f, 0.25f, 1.0f / 3.0f, 0.375f, 0.5f,
                                 2.0f / 3.0f, 0.75f, 1.0f, 4.0f / 3.0f, 1.5f, 2.0f };
const float kPreDelayBeats[] = { 1.0f / 32.0f, 1.0f / 16.0f, 1.0f / 12.0f, 1.0f / 8.0f,
                                 1.0f / 6.0f, 0.25f, 1.0f / 3.0f, 0.5f };
const float kDecayBeats[]    = { 1.0f, 2.0f, 3.0f, 4.0f, 8.0f, 16.0f };
const float kLfoBeats[]      = { 0.125f, 0.25f, 1.0f / 3.0f, 0.5f, 2.0f / 3.0f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f };

template <size_t N>
float beatsFor (const float (&table)[N], float index)
{
    return table[(size_t) juce::jlimit (0, (int) N - 1, (int) index)];
}

using StringFn = std::function<juce::String (float, int)>;

juce::NormalisableRange<float> logRange (float lo, float hi)
{
    return { lo, hi,
             [] (float start, float end, float x) { return start * std::pow (end / start, x); },
             [] (float start, float end, float v) { return std::log (v / start) / std::log (end / start); },
             [] (float start, float end, float v) { return juce::jlimit (start, end, v); } };
}

juce::NormalisableRange<float> linRange (float lo, float hi, float centre = 0.0f)
{
    juce::NormalisableRange<float> r (lo, hi);
    if (centre > lo) r.setSkewForCentre (centre);
    return r;
}

std::unique_ptr<juce::AudioParameterFloat> makeFloat (const juce::String& id, int version, const juce::String& name,
                                                      juce::NormalisableRange<float> range, float def,
                                                      const char* unit, int decimals, StringFn toText = {})
{
    if (! toText)
    {
        const juce::String suffix = juce::String (unit).isEmpty() ? juce::String() : " " + juce::String (unit);
        toText = [suffix, decimals] (float v, int) { return juce::String (v, decimals) + suffix; };
    }
    auto attrs = juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction (toText)
        .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue(); });
    return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, version }, name, range, def, attrs);
}

std::unique_ptr<juce::AudioParameterBool> makeBool (const juce::String& id, int version, const juce::String& name, bool def)
{
    return std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, version }, name, def);
}

std::unique_ptr<juce::AudioParameterChoice> makeChoice (const juce::String& id, int version, const juce::String& name,
                                                        const juce::StringArray& items, int def)
{
    return std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, version }, name, items, def);
}

StringFn hzText()
{
    return [] (float v, int) { return v >= 1000.0f ? juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz"
                                                   : juce::String (juce::roundToInt (v)) + " Hz"; };
}

juce::StringArray utf8List (std::initializer_list<const char*> items)
{
    juce::StringArray a;
    for (auto* s : items) a.add (U (s));
    return a;
}
}

//==============================================================================
const juce::StringArray& DusiVerbProcessor::divisionNames()
{
    static const juce::StringArray names { "1/32", "1/16T", "1/16", "1/8T", "1/16.", "1/8",
                                           "1/4T", "1/8.", "1/4", "1/2T", "1/4.", "1/2" };
    return names;
}
const juce::StringArray& DusiVerbProcessor::preDelayDivisionNames()
{
    static const juce::StringArray names { "1/128", "1/64", "1/32T", "1/32", "1/16T", "1/16", "1/8T", "1/8" };
    return names;
}
const juce::StringArray& DusiVerbProcessor::decayDivisionNames()
{
    static const juce::StringArray names { "1/4", "1/2", "1/2.", "1 takt", "2 takty", "4 takty" };
    return names;
}
const juce::StringArray& DusiVerbProcessor::lfoDivisionNames()
{
    static const juce::StringArray names { "1/32", "1/16", "1/8T", "1/8", "1/4T", "1/4", "1/2", "1 takt", "2 takty", "4 takty" };
    return names;
}
const juce::StringArray& DusiVerbProcessor::reverbTypeNames()
{
    static const juce::StringArray names { "Plate", "Room", "Chamber", "Hall", "Spring", "Prism" };
    return names;
}
const juce::StringArray& DusiVerbProcessor::eqTypeNames()
{
    static const juce::StringArray names { "Low Cut", "Low Shelf", "Bell", "High Shelf", "High Cut", "Notch" };
    return names;
}
juce::String DusiVerbProcessor::eqId (int band, const char* what) { return "eq" + juce::String (band + 1) + what; }
juce::String DusiVerbProcessor::dqId (int band, const char* what) { return "dq" + juce::String (band + 1) + what; }

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout DusiVerbProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    const auto pct = [] (const char* id, int v, const char* name, float def, float hi = 100.0f)
    { return makeFloat (id, v, name, linRange (0.0f, hi), def, "%", 0); };

    // ---- Delay
    l.add (makeBool   ("delayOn",       2, "Delay On", true));
    l.add (makeFloat  ("delayTime",     1, "Delay Time", linRange (1.0f, 2000.0f, 300.0f), 375.0f, "ms", 0));
    l.add (makeBool   ("delaySync",     1, "Delay Sync", true));
    l.add (makeChoice ("delayDiv",      1, "Delay Division", divisionNames(), 7));
    l.add (pct        ("delayFeedback", 1, "Delay Feedback", 35.0f, 95.0f));
    l.add (makeFloat  ("delayTone",     1, "Delay Tone", linRange (200.0f, 20000.0f, 3000.0f), 6000.0f, "Hz", 0, hzText()));
    l.add (makeBool   ("pingPong",      1, "Ping-Pong", false));
    l.add (pct        ("delayMix",      1, "Delay Mix", 20.0f));

    // ---- Reverb
    l.add (makeBool   ("reverbOn",      2, "Reverb On", true));
    l.add (makeChoice ("reverbType",    3, "Reverb Type", reverbTypeNames(), 0));
    l.add (makeFloat  ("preDelay",      1, "Pre-Delay", linRange (0.0f, 500.0f, 60.0f), 20.0f, "ms", 0));
    l.add (makeBool   ("preDelaySync",  2, "Pre-Delay Sync", false));
    l.add (makeChoice ("preDelayDiv",   2, "Pre-Delay Division", preDelayDivisionNames(), 1));
    l.add (makeFloat  ("size",          1, "Size", linRange (30.0f, 150.0f), 100.0f, "%", 0));
    l.add (makeFloat  ("decay",         1, "Decay", linRange (0.2f, 20.0f, 3.0f), 2.5f, "s", 2));
    l.add (makeBool   ("decaySync",     2, "Decay Sync", false));
    l.add (makeChoice ("decayDiv",      2, "Decay Division", decayDivisionNames(), 3));
    l.add (makeFloat  ("highCut",       1, "High Cut", linRange (500.0f, 20000.0f, 5000.0f), 8000.0f, "Hz", 0, hzText()));
    l.add (makeFloat  ("lowCut",        1, "Low Cut", linRange (10.0f, 2000.0f, 200.0f), 150.0f, "Hz", 0, hzText()));
    l.add (pct        ("mod",           1, "Modulation", 30.0f));
    l.add (pct        ("early",         3, "Early Reflections", 25.0f));
    l.add (pct        ("depth",         2, "Depth", 35.0f));
    l.add (pct        ("reverbMix",     1, "Reverb Mix", 30.0f));

    // ---- Decay EQ (delka dozvuku po frekvencich)
    l.add (makeBool ("decayEqOn", 3, "Decay EQ On", false));
    const float dqFreq[] = { 200.0f, 700.0f, 2500.0f, 7000.0f };
    const char* dqName[] = { "Decay Low", "Decay Mid 1", "Decay Mid 2", "Decay High" };
    for (int b = 0; b < dusi::kNumDecayBands; ++b)
    {
        l.add (makeFloat (dqId (b, "Freq"), 3, juce::String (dqName[b]) + " Freq", logRange (20.0f, 20000.0f), dqFreq[b], "Hz", 0, hzText()));
        l.add (makeFloat (dqId (b, "Mult"), 3, juce::String (dqName[b]) + " Mult", logRange (0.25f, 4.0f), 1.0f, "", 2,
                          [] (float v, int) { return juce::String::fromUTF8 ("×") + juce::String (v, 2); }));
        if (b == 1 || b == 2)
            l.add (makeFloat (dqId (b, "Q"), 3, juce::String (dqName[b]) + " Q", logRange (0.2f, 6.0f), 0.8f, "", 2));
    }

    // ---- Prostor / stereo
    l.add (makeFloat  ("pan", 2, "Pan", linRange (-100.0f, 100.0f), 0.0f, "", 0,
                       [] (float v, int)
                       {
                           const int i = juce::roundToInt (v);
                           return i == 0 ? juce::String ("C") : (i < 0 ? "L " + juce::String (-i) : "R " + juce::String (i));
                       }));
    l.add (pct        ("width",    1, "Width", 100.0f, 150.0f));
    l.add (makeFloat  ("monoBass", 2, "Bass Mono", linRange (20.0f, 500.0f, 150.0f), 150.0f, "Hz", 0,
                       [] (float v, int) { return v < 21.0f ? juce::String ("vyp") : juce::String (juce::roundToInt (v)) + " Hz"; }));
    l.add (makeBool   ("safe",     2, "Stereo Safe", true));

    // ---- Filtr s LFO
    l.add (makeBool   ("filterOn",   3, "Filter On", false));
    l.add (makeChoice ("filterType", 3, "Filter Type", juce::StringArray { "LP", "BP", "HP" }, 0));
    l.add (makeFloat  ("filterFreq", 3, "Filter Freq", logRange (20.0f, 20000.0f), 1200.0f, "Hz", 0, hzText()));
    l.add (pct        ("filterReso", 3, "Filter Resonance", 30.0f));
    l.add (makeBool   ("lfoSync",    3, "LFO Sync", true));
    l.add (makeChoice ("lfoDiv",     3, "LFO Division", lfoDivisionNames(), 7));
    l.add (makeFloat  ("lfoRate",    3, "LFO Rate", logRange (0.05f, 20.0f), 1.0f, "Hz", 2));
    l.add (pct        ("lfoDepth",   3, "LFO Depth", 50.0f));
    l.add (makeChoice ("lfoShape",   3, "LFO Shape", utf8List ({ "Sinus", "Trojúhelník", "Pila", "Obdélník", "Náhodný" }), 0));
    l.add (makeFloat  ("lfoStereo",  3, "LFO Stereo", linRange (0.0f, 180.0f), 0.0f, "", 0,
                       [] (float v, int) { return juce::String (juce::roundToInt (v)) + juce::String::fromUTF8 ("°"); }));
    l.add (pct        ("filterMix",  3, "Filter Mix", 100.0f));

    // ---- Vystupni EQ (8 pasem)
    l.add (makeBool ("eqOn", 3, "EQ On", true));
    const float eqFreq[] = { 40.0f, 100.0f, 250.0f, 600.0f, 1500.0f, 3500.0f, 8000.0f, 16000.0f };
    for (int b = 0; b < dusi::kNumEqBands; ++b)
    {
        const auto name = "EQ " + juce::String (b + 1);
        const int defType = b == 0 ? 0 : (b == dusi::kNumEqBands - 1 ? 4 : 2);
        l.add (makeBool   (eqId (b, "On"),    3, name + " On", false));
        l.add (makeChoice (eqId (b, "Type"),  3, name + " Type", eqTypeNames(), defType));
        l.add (makeFloat  (eqId (b, "Freq"),  3, name + " Freq", logRange (20.0f, 20000.0f), eqFreq[b], "Hz", 0, hzText()));
        l.add (makeFloat  (eqId (b, "Gain"),  3, name + " Gain", linRange (-18.0f, 18.0f), 0.0f, "dB", 1));
        l.add (makeFloat  (eqId (b, "Q"),     3, name + " Q", logRange (0.1f, 18.0f), defType == 2 ? 1.0f : 0.71f, "", 2));
        l.add (makeChoice (eqId (b, "Slope"), 3, name + " Slope", utf8List ({ "12 dB/okt", "24 dB/okt", "48 dB/okt" }), 1));
    }

    // ---- Retez a vystup
    // ---- Dynamika efektu
    l.add (pct        ("duckAmount",  4, "Ducking", 0.0f));
    l.add (makeFloat  ("duckRelease", 4, "Ducking Release", logRange (20.0f, 1000.0f), 250.0f, "ms", 0));
    l.add (pct        ("compAmount",  4, "Wet Compression", 0.0f));

    // ---- Reverb navic
    l.add (makeBool   ("freeze",  4, "Freeze", false));
    l.add (pct        ("shimmer", 4, "Shimmer", 0.0f));

    // ---- Chorus
    l.add (makeBool   ("chorusOn",    4, "Chorus On", false));
    l.add (makeFloat  ("chorusRate",  4, "Chorus Rate", logRange (0.05f, 5.0f), 0.6f, "Hz", 2));
    l.add (pct        ("chorusDepth", 4, "Chorus Depth", 40.0f));
    l.add (pct        ("chorusMix",   4, "Chorus Mix", 50.0f));

    // ---- Lo-fi
    l.add (makeBool   ("lofiOn",   4, "Lo-fi On", false));
    l.add (makeFloat  ("lofiBits", 4, "Lo-fi Bits", juce::NormalisableRange<float> (4.0f, 16.0f, 1.0f), 10.0f, "bit", 0));
    l.add (makeFloat  ("lofiRate", 4, "Lo-fi Sample Rate", logRange (1000.0f, 48000.0f), 12000.0f, "Hz", 0, hzText()));
    l.add (makeFloat  ("lofiTone", 4, "Lo-fi Tone", logRange (500.0f, 20000.0f), 7000.0f, "Hz", 0, hzText()));
    l.add (pct        ("lofiMix",  4, "Lo-fi Mix", 100.0f));

    // ---- Retez a vystup (vsech 120 poradi 5 efektu)
    {
        juce::StringArray orders;
        for (const auto& o : dusi::chainOrders())
        {
            juce::StringArray names;
            for (auto m : o) names.add (m == dusi::Module::delay ? "Delay" : m == dusi::Module::reverb ? "Reverb"
                                       : m == dusi::Module::filter ? "Filter" : m == dusi::Module::chorus ? "Chorus" : "Lo-fi");
            orders.add (names.joinIntoString (" > "));
        }
        l.add (makeChoice ("chainOrder", 3, "Chain Order", orders, 0));
    }
    l.add (makeFloat  ("outGain", 1, "Output", linRange (-24.0f, 12.0f, 0.0f), 0.0f, "dB", 1));
    return l;
}

DusiVerbProcessor::DusiVerbProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "PARAMS", createLayout())
{
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            paramPtr[r->paramID.toStdString()] = apvts.getRawParameterValue (r->paramID);
    const char* eqParts[] = { "On", "Type", "Freq", "Gain", "Q", "Slope" };
    for (int b = 0; b < dusi::kNumEqBands; ++b)
        for (int k = 0; k < 6; ++k)
            eqPtr[b][k] = apvts.getRawParameterValue (eqId (b, eqParts[k]));
    for (int b = 0; b < dusi::kNumDecayBands; ++b)
    {
        dqPtr[b][0] = apvts.getRawParameterValue (dqId (b, "Freq"));
        dqPtr[b][1] = apvts.getRawParameterValue (dqId (b, "Mult"));
        dqPtr[b][2] = (b == 1 || b == 2) ? apvts.getRawParameterValue (dqId (b, "Q")) : nullptr;
    }
}

bool DusiVerbProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    const bool inOk  = in  == juce::AudioChannelSet::mono() || in  == juce::AudioChannelSet::stereo();
    const bool outOk = out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
    return inOk && outOk && in.size() <= out.size();   // mono, mono->stereo, stereo
}

void DusiVerbProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    lastSampleRate = sampleRate;
    engine.prepare (sampleRate);
    engine.setParams (readParams(), samplesPerBlock);
    scratch.setSize (1, samplesPerBlock);
}

//------------------------------------------------------------------------------
// Prevod knofliku na parametry DSP vcetne vypoctu podle tempa.
dusi::Params DusiVerbProcessor::currentParams() const
{
    // ukazatele na parametry jsou pripravene predem - zadne hledani podle jmena ani alokace v audio vlakne
    auto v  = [this] (const char* id) { return paramPtr.at (id)->load(); };
    auto on = [&v] (const char* id) { return v (id) > 0.5f; };

    const double msPerBeat = 60000.0 / lastBpm;   // delka ctvrtove noty v ms

    dusi::Params p;
    p.delayOn   = on ("delayOn");
    p.delayMs   = on ("delaySync") ? (float) (msPerBeat * beatsFor (kDelayBeats, v ("delayDiv"))) : v ("delayTime");
    p.delayMs   = juce::jlimit (1.0f, 2000.0f, p.delayMs);
    p.feedback  = v ("delayFeedback") * 0.01f;
    p.delayTone = v ("delayTone");
    p.pingPong  = on ("pingPong");
    p.delayMix  = v ("delayMix") * 0.01f;

    p.reverbOn   = on ("reverbOn");
    p.reverbType = (dusi::ReverbType) juce::jlimit (0, 5, (int) v ("reverbType"));
    // Pre-delay v notach: napr. 1/64 pri 140 BPM = 428.6 / 16 = 26.8 ms
    p.preDelayMs = on ("preDelaySync") ? (float) (msPerBeat * beatsFor (kPreDelayBeats, v ("preDelayDiv"))) : v ("preDelay");
    p.preDelayMs = juce::jlimit (0.0f, 500.0f, p.preDelayMs);
    // Decay do rytmu: decay = delka noty - pre-delay
    p.decaySec = on ("decaySync") ? (float) ((msPerBeat * beatsFor (kDecayBeats, v ("decayDiv")) - p.preDelayMs) * 0.001)
                                  : v ("decay");
    p.decaySec  = juce::jlimit (0.2f, 20.0f, p.decaySec);
    p.size      = v ("size") * 0.01f;
    p.highCut   = v ("highCut");
    p.lowCut    = v ("lowCut");
    p.mod       = v ("mod") * 0.01f;
    p.early     = v ("early") * 0.01f;
    p.depth     = v ("depth") * 0.01f;
    p.reverbMix = v ("reverbMix") * 0.01f;
    p.decayEqOn = on ("decayEqOn");
    for (int b = 0; b < dusi::kNumDecayBands; ++b)
    {
        auto& d = p.decayEq[(size_t) b];
        d.freq = dqPtr[b][0]->load();
        d.mult = dqPtr[b][1]->load();
        d.q    = dqPtr[b][2] != nullptr ? dqPtr[b][2]->load() : 0.7f;
    }

    p.pan      = v ("pan") * 0.01f;
    p.autoSafe = on ("safe");
    p.width    = v ("width") * 0.01f;
    if (p.autoSafe)   // bezpecna sirka: efekt se nevejde za steny mistnosti
        p.width = std::min (p.width, dusi::space::safeWidthLimit (p.pan));
    p.monoBassHz = v ("monoBass") < 21.0f ? 10.0f : v ("monoBass");

    p.filterOn   = on ("filterOn");
    p.filterType = (dusi::FilterType) juce::jlimit (0, 2, (int) v ("filterType"));
    p.filterFreq = v ("filterFreq");
    p.filterReso = v ("filterReso") * 0.01f;
    p.lfoSynced  = on ("lfoSync");
    p.lfoBeats   = p.lfoSynced ? beatsFor (kLfoBeats, v ("lfoDiv")) : 0.0f;
    p.lfoHz      = p.lfoSynced ? (float) (lastBpm / 60.0 / p.lfoBeats) : v ("lfoRate");
    p.lfoDepth   = v ("lfoDepth") * 0.01f;
    p.lfoShape   = (dusi::LfoShape) juce::jlimit (0, 4, (int) v ("lfoShape"));
    p.lfoStereo  = v ("lfoStereo");
    p.filterMix  = v ("filterMix") * 0.01f;

    p.eqOn = on ("eqOn");
    for (int b = 0; b < dusi::kNumEqBands; ++b)
    {
        auto& e = p.eq[(size_t) b];
        e.on    = eqPtr[b][0]->load() > 0.5f;
        e.type  = (dusi::EqType) juce::jlimit (0, 5, (int) eqPtr[b][1]->load());
        e.freq  = eqPtr[b][2]->load();
        e.gain  = eqPtr[b][3]->load();
        e.q     = eqPtr[b][4]->load();
        e.slope = juce::jlimit (0, 2, (int) eqPtr[b][5]->load());
    }

    p.duckAmount  = v ("duckAmount") * 0.01f;
    p.duckRelease = v ("duckRelease");
    p.compAmount  = v ("compAmount") * 0.01f;
    p.freeze      = on ("freeze");
    p.shimmer     = v ("shimmer") * 0.01f;
    p.chorusOn    = on ("chorusOn");
    p.chorusRate  = v ("chorusRate");
    p.chorusDepth = v ("chorusDepth") * 0.01f;
    p.chorusMix   = v ("chorusMix") * 0.01f;
    p.lofiOn      = on ("lofiOn");
    p.lofiBits    = v ("lofiBits");
    p.lofiRate    = v ("lofiRate");
    p.lofiTone    = v ("lofiTone");
    p.lofiMix     = v ("lofiMix") * 0.01f;

    p.chainOrder = juce::jlimit (0, (int) dusi::chainOrders().size() - 1, (int) v ("chainOrder"));
    p.outGainDb  = v ("outGain");
    p.bpm = lastBpm;
    p.ppq = lastPpq;
    p.playing = lastPlaying;
    return p;
}

dusi::Params DusiVerbProcessor::readParams()
{
    auto p = currentParams();
    uiDelayMs.store (p.delayMs);
    uiPreDelayMs.store (p.preDelayMs);
    uiDecaySec.store (p.decaySec);
    uiLfoHz.store (p.lfoHz);
    return p;
}

void DusiVerbProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // tempo a pozice z Logicu (sync delaye, reverbu a LFO)
    lastPlaying = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm())
                if (*bpm > 1.0) lastBpm = *bpm;
            if (auto ppq = pos->getPpqPosition()) lastPpq = *ppq;
            lastPlaying = pos->getIsPlaying();
        }
    uiBpm.store (lastBpm);

    const int n      = buffer.getNumSamples();
    engine.setParams (readParams(), n);

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();

    if (numOut >= 2)
    {
        if (numIn < 2)                       // mono -> stereo
            buffer.copyFrom (1, 0, buffer, 0, 0, n);
        engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n);
    }
    else if (numOut == 1)                    // mono: zpracuj stereo a sectu do mona
    {
        if (scratch.getNumSamples() < n)
            scratch.setSize (1, n, false, false, true);
        scratch.copyFrom (0, 0, buffer, 0, 0, n);
        engine.process (buffer.getWritePointer (0), scratch.getWritePointer (0), n);
        buffer.addFrom (0, 0, scratch, 0, 0, n);
        buffer.applyGain (0, 0, n, 0.5f);
    }

    // analyzator: mono soucet vystupu do kruhoveho bufferu (bez zamku)
    {
        const float* l = buffer.getReadPointer (0);
        const float* r = buffer.getReadPointer (numOut >= 2 ? 1 : 0);
        int start1, size1, start2, size2;
        const int toWrite = juce::jmin (n, fifo.getFreeSpace());
        fifo.prepareToWrite (toWrite, start1, size1, start2, size2);
        for (int i = 0; i < size1; ++i) fifoData[(size_t) (start1 + i)] = 0.5f * (l[i] + r[i]);
        for (int i = 0; i < size2; ++i) fifoData[(size_t) (start2 + i)] = 0.5f * (l[size1 + i] + r[size1 + i]);
        fifo.finishedWrite (size1 + size2);

        // analyzator dozvuku: jen mokry signal reverbu
        const float* t = engine.getTail();
        const int tn = juce::jmin (engine.getTailCount(), tailFifo.getFreeSpace());
        tailFifo.prepareToWrite (tn, start1, size1, start2, size2);
        for (int i = 0; i < size1; ++i) tailData[(size_t) (start1 + i)] = t[i];
        for (int i = 0; i < size2; ++i) tailData[(size_t) (start2 + i)] = t[size1 + i];
        tailFifo.finishedWrite (size1 + size2);
    }

    uiCorrelation.store (engine.getCorrelation());
    uiAutoGain.store (engine.getAutoGain());
    uiLevel.store (engine.getLevel());
    uiLfoPhase.store (engine.getLfoPhase());
    uiDuckGain.store (engine.getDuckGain());
}

int DusiVerbProcessor::readAnalyzer (float* dest, int maxSamples, bool reverbTail)
{
    auto& f = reverbTail ? tailFifo : fifo;
    auto& data = reverbTail ? tailData : fifoData;
    int start1, size1, start2, size2;
    const int ready = f.getNumReady();
    // starsi data zahodit, at analyzator ukazuje "ted"
    if (ready > maxSamples)
    {
        f.prepareToRead (ready - maxSamples, start1, size1, start2, size2);
        f.finishedRead (size1 + size2);
    }
    f.prepareToRead (juce::jmin (maxSamples, f.getNumReady()), start1, size1, start2, size2);
    for (int i = 0; i < size1; ++i) dest[i] = data[(size_t) (start1 + i)];
    for (int i = 0; i < size2; ++i) dest[size1 + i] = data[(size_t) (start2 + i)];
    f.finishedRead (size1 + size2);
    return size1 + size2;
}

//------------------------------------------------------------------------------
int DusiVerbProcessor::getNumPrograms()
{
    return (int) factoryPresets().size();
}

const juce::String DusiVerbProcessor::getProgramName (int index)
{
    if (juce::isPositiveAndBelow (index, (int) factoryPresets().size()))
        return U (factoryPresets()[(size_t) index].name);
    return {};
}

void DusiVerbProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) factoryPresets().size()))
        return;

    undoManager.beginNewTransaction (U ("Preset"));
    currentPreset = index;
    const auto& preset = factoryPresets()[(size_t) index];
    presetName = U (preset.name);

    for (auto* p : getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (ranged == nullptr) continue;

        float norm = ranged->getDefaultValue();
        for (const auto& [id, value] : preset.values)
            if (ranged->paramID == id)
                norm = ranged->convertTo0to1 (value);
        ranged->setValueNotifyingHost (norm);
    }
}

void DusiVerbProcessor::applyValues (const juce::ValueTree& state)
{
    undoManager.beginNewTransaction (U ("Preset"));
    for (auto* p : getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (ranged == nullptr) continue;
        float norm = ranged->getDefaultValue();
        const auto child = state.getChildWithProperty ("id", ranged->paramID);
        if (child.isValid() && child.hasProperty ("value"))
            norm = ranged->convertTo0to1 ((float) child.getProperty ("value"));
        ranged->setValueNotifyingHost (norm);
    }
}

void DusiVerbProcessor::loadUserPreset (const juce::File& file)
{
    const auto state = presets.load (file);
    if (! state.isValid()) return;
    applyValues (state);
    currentPreset = -1;
    presetName = file.getFileNameWithoutExtension();
}

bool DusiVerbProcessor::saveUserPreset (const juce::String& name, juce::String& error)
{
    if (! presets.save (name, apvts.copyState(), error))
        return false;
    presetName = juce::File::createLegalFileName (name.trim());
    currentPreset = -1;
    return true;
}

void DusiVerbProcessor::randomize()
{
    undoManager.beginNewTransaction (U ("Náhodný"));
    juce::Random rnd;
    auto set = [this] (const char* id, float value)
    {
        if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (value));
    };
    auto between = [&rnd] (float a, float b) { return a + (b - a) * rnd.nextFloat(); };

    set ("reverbOn", 1);
    set ("reverbType", (float) rnd.nextInt (6));
    set ("preDelaySync", 0);
    set ("decaySync", 0);
    set ("preDelay", between (0, 60));
    set ("size", between (40, 150));
    set ("decay", between (0.6f, 5.0f));
    set ("highCut", between (4000, 12000));
    set ("lowCut", between (100, 400));
    set ("mod", between (5, 60));
    set ("early", between (0, 60));
    set ("depth", between (15, 75));
    set ("reverbMix", between (15, 45));
    set ("decayEqOn", rnd.nextBool() ? 1.0f : 0.0f);
    set (dqId (0, "Mult").toRawUTF8(), between (0.5f, 1.2f));
    set (dqId (3, "Mult").toRawUTF8(), between (0.5f, 1.5f));

    const bool delay = rnd.nextBool();
    set ("delayOn", delay ? 1.0f : 0.0f);
    const int divs[] = { 5, 7, 8, 10 };
    set ("delaySync", 1);
    set ("delayDiv", (float) divs[rnd.nextInt (4)]);
    set ("delayFeedback", between (15, 50));
    set ("delayTone", between (2500, 8000));
    set ("delayMix", between (10, 30));
    set ("pingPong", rnd.nextBool() ? 1.0f : 0.0f);

    const bool filter = rnd.nextInt (4) == 0;
    set ("filterOn", filter ? 1.0f : 0.0f);
    set ("filterType", (float) rnd.nextInt (3));
    set ("filterFreq", between (500, 4000));
    set ("filterReso", between (10, 60));
    set ("lfoSync", 1);
    set ("lfoDiv", (float) (3 + rnd.nextInt (6)));
    set ("lfoDepth", between (20, 70));
    set ("lfoShape", (float) rnd.nextInt (5));
    set ("filterMix", 100);
    set ("chainOrder", (float) rnd.nextInt (6));
    set ("chorusOn", rnd.nextInt (4) == 0 ? 1.0f : 0.0f);
    set ("lofiOn", 0);
    set ("freeze", 0);
    set ("shimmer", rnd.nextInt (5) == 0 ? between (20, 50) : 0.0f);

    currentPreset = -1;
    presetName = U ("Náhodný");
}

void DusiVerbProcessor::selectAb (int slot)
{
    slot = juce::jlimit (0, 1, slot);
    if (slot == abSlot) return;
    abStates[abSlot] = apvts.copyState();
    if (abStates[slot].isValid())
        applyValues (abStates[slot]);
    abSlot = slot;
}

void DusiVerbProcessor::copyAbToOther()
{
    abStates[1 - abSlot] = apvts.copyState();
}

//------------------------------------------------------------------------------
juce::AudioProcessorEditor* DusiVerbProcessor::createEditor()
{
    return new DusiVerbEditor (*this);
}

void DusiVerbProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset, nullptr);
    state.setProperty ("presetName", presetName, nullptr);
    state.setProperty ("eco", eco.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void DusiVerbProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            currentPreset = juce::jlimit (-1, getNumPrograms() - 1, (int) state.getProperty ("preset", 0));
            presetName = state.getProperty ("presetName", getProgramName (juce::jmax (0, currentPreset))).toString();
            eco.store ((bool) state.getProperty ("eco", false));
            apvts.replaceState (state);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DusiVerbProcessor();
}
