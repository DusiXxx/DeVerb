#include "EffectsPage.h"

using ui::U;

namespace
{
constexpr int kChainH = 72, kPanelH = 424, kDynH = 140;
}

//==============================================================================
ModulePanel::ModulePanel (DusiVerbProcessor& p, const juce::String& t, juce::Colour a, const char* onId, const juce::String& h, int module)
    : proc (p), title (t), hint (h), accent (a), onParam (onId), power (a)
{
    if (onParam != nullptr)
    {
        addAndMakeVisible (power);
        powerAtt = std::make_unique<APVTS::ButtonAttachment> (p.apvts, onParam, power);
    }
    if (module >= 0)
    {
        viz = std::make_unique<EffectViz> (p, (dusi::Module) module);
        addAndMakeVisible (*viz);
    }
}

bool ModulePanel::isOn() const
{
    return onParam == nullptr || proc.apvts.getRawParameterValue (onParam)->load() > 0.5f;
}

juce::Label* ModulePanel::addCaption (const juce::String& text)
{
    auto* l = captions.add (new juce::Label ({}, text));
    l->setFont (ui::font (10.0f, 500));
    l->setColour (juce::Label::textColourId, ui::textDim);
    l->setJustificationType (juce::Justification::topLeft);
    l->setBorderSize ({});
    l->setInterceptsMouseClicks (false, false);
    addAndMakeVisible (l);
    return l;
}

void ModulePanel::paint (juce::Graphics& g)
{
    ui::paintPanel (g, getLocalBounds(), {}, accent);
    auto bar = getLocalBounds().withHeight (ui::panelTitle).withTrimmedLeft (12).withTrimmedRight (52);
    const auto tf = ui::font (12.0f, 600, 0.08f);
    g.setFont (tf);
    g.setColour (isOn() ? accent : ui::disabled);
    g.drawText (title, bar, juce::Justification::centredLeft);
    if (hint.isNotEmpty())
    {
        g.setColour (ui::textDim);
        g.setFont (ui::font (11.0f, 500));
        g.drawFittedText (hint, bar.withTrimmedLeft (juce::GlyphArrangement::getStringWidthInt (tf, title) + 16),
                          juce::Justification::centredLeft, 1, 0.9f);
    }
}

void ModulePanel::resized()
{
    power.setBounds (getWidth() - 12 - 28, 9, 28, 22);
    if (viz != nullptr)
        viz->setBounds (vizRows == 1 ? juce::Rectangle<int> (13, 159, getWidth() - 26, 240)
                                     : juce::Rectangle<int> (13, 279, getWidth() - 26, 120));
    layout();
}

namespace
{
//==============================================================================
class DelayPanel : public ModulePanel
{
public:
    explicit DelayPanel (DusiVerbProcessor& p)
        : ModulePanel (p, "DELAY", ui::delay, "delayOn",
                       U ("Ozvěna v tempu. Tip: 1/8. (tečkovaná osmina) je klasika na vokály a leady, Ping-Pong rozhodí opakování do stran."),
                       (int) dusi::Module::delay),
          time (*this, p.apvts, "delayTime", "delayDiv", "TIME", ui::delay),
          feedback (*this, p.apvts, "delayFeedback", "FEEDBACK", ui::delay),
          tone (*this, p.apvts, "delayTone", "TONE", ui::delay),
          mix (*this, p.apvts, "delayMix", "MIX", ui::delay),
          sync (*this, p.apvts, "delaySync", U ("Sync na tempo"), ui::delay),
          pingPong (*this, p.apvts, "pingPong", "Ping-Pong", ui::delay)
    {
        setVizRows (1);
    }

    void layout() override
    {
        time.setTopLeft (knobPos (0, 0).x, knobPos (0, 0).y);
        feedback.setTopLeft (knobPos (1, 0).x, knobPos (1, 0).y);
        tone.setTopLeft (knobPos (2, 0).x, knobPos (2, 0).y);
        mix.setTopLeft (knobPos (3, 0).x, knobPos (3, 0).y);
        const int x = knobPos (4, 0).x + 24;
        sync.button.setBounds (x, 62, 180, 22);
        pingPong.button.setBounds (x, 96, 180, 22);
    }

    void refresh() override
    {
        time.update (proc.apvts.getRawParameterValue ("delaySync")->load() > 0.5f,
                     "= " + juce::String (proc.uiDelayMs.load(), 1) + " ms");
        const bool off = ! isOn();
        time.setDimmed (off);
        for (auto* k : { &feedback, &tone, &mix }) k->setDimmed (off);
        for (auto* t : { &sync, &pingPong }) t->button.setAlpha (off ? 0.45f : 1.0f);
        updateTitle();
    }

private:
    SyncCell time;
    Knob feedback, tone, mix;
    Toggle sync, pingPong;
};

//==============================================================================
class ReverbPanel : public ModulePanel
{
public:
    explicit ReverbPanel (DusiVerbProcessor& p)
        : ModulePanel (p, "REVERB", ui::verb, "reverbOn",
                       U ("Dozvuk umístí zvuk do prostoru. SIZE a DECAY = velikost a délka, HIGH / LOW CUT čistí ocas, SHIMMER přidá jas o oktávu výš."),
                       (int) dusi::Module::reverb),
          preDelay (*this, p.apvts, "preDelay", "preDelayDiv", "PRE-DELAY", ui::verb),
          decay (*this, p.apvts, "decay", "decayDiv", "DECAY", ui::verb),
          size (*this, p.apvts, "size", "SIZE", ui::verb),
          highCut (*this, p.apvts, "highCut", "HIGH CUT", ui::verb),
          lowCut (*this, p.apvts, "lowCut", "LOW CUT", ui::verb),
          shimmer (*this, p.apvts, "shimmer", "SHIMMER", ui::verb),
          early (*this, p.apvts, "early", "EARLY", ui::verb),
          mod (*this, p.apvts, "mod", "MOD", ui::verb),
          depth (*this, p.apvts, "depth", "HLOUBKA", ui::verb),
          mix (*this, p.apvts, "reverbMix", "MIX", ui::verb),
          type (*this, p.apvts, "reverbType", "TYP"),
          preSync (*this, p.apvts, "preDelaySync", U ("Pre-delay do tempa"), ui::verb),
          decaySync (*this, p.apvts, "decaySync", U ("Decay do tempa"), ui::verb)
    {
        freeze.setTooltip (U ("Zmrazí dozvuk – hraje donekonečna, nový zvuk už do něj nejde"));
        addAndMakeVisible (freeze);
        freezeAtt = std::make_unique<APVTS::ButtonAttachment> (p.apvts, "freeze", freeze);
        type.box.setTooltip (U ("Plate = hladký kovový, Room = místnost, Chamber = hustá komora, Hall = sál, Spring = pružina, Prism = vlastní zvuk DeVerbu: krátké basy uprostřed, dlouhé široké plovoucí výšky"));
        early.slider.setTooltip (U ("První odrazy od stěn – počítají se z 3D místnosti na stránce PROSTOR"));
        shimmer.slider.setTooltip (U ("Oktáva nahoru ve zpětné vazbě – zářivé ambientní plochy"));
        typeCaption = addCaption (U ("Plate · Room · Chamber · Hall · Spring · Prism"));
        hintCaption = addCaption (U ("decay = délka noty − pre-delay"));
    }

    void layout() override
    {
        int i = 0;
        preDelay.setTopLeft (knobPos (i, 0).x, knobPos (i, 0).y); ++i;
        size.setTopLeft (knobPos (i, 0).x, knobPos (i, 0).y); ++i;
        decay.setTopLeft (knobPos (i, 0).x, knobPos (i, 0).y); ++i;
        highCut.setTopLeft (knobPos (i, 0).x, knobPos (i, 0).y); ++i;
        lowCut.setTopLeft (knobPos (i, 0).x, knobPos (i, 0).y); ++i;
        shimmer.setTopLeft (knobPos (i, 0).x, knobPos (i, 0).y);
        freeze.setBounds (653, 53, 240, 60);

        type.setBounds ({ 15, 165, 150, 50 });
        typeCaption->setBounds (15, 219, 160, 28);
        int x = 185;
        for (auto* k : { &early, &mod, &depth, &mix }) { k->setTopLeft (x, 159); x += 92; }
        x += 24;
        preSync.button.setBounds (x, 178, 200, 22);
        decaySync.button.setBounds (x, 210, 200, 22);
        hintCaption->setBounds (x + 26, 236, 220, 16);
    }

    void refresh() override
    {
        auto on = [this] (const char* id) { return proc.apvts.getRawParameterValue (id)->load() > 0.5f; };
        preDelay.update (on ("preDelaySync"), "= " + juce::String (proc.uiPreDelayMs.load(), 1) + " ms");
        decay.update (on ("decaySync"), "= " + juce::String (proc.uiDecaySec.load(), 2) + " s");
        const bool off = ! isOn();
        preDelay.setDimmed (off);
        decay.setDimmed (off);
        for (auto* k : { &size, &highCut, &lowCut, &shimmer, &early, &mod, &depth, &mix }) k->setDimmed (off);
        type.setDimmed (off);
        for (auto* t : { &preSync, &decaySync }) t->button.setAlpha (off ? 0.45f : 1.0f);
        freeze.setAlpha (off ? 0.45f : 1.0f);
        hintCaption->setVisible (on ("decaySync"));
        updateTitle();
    }

private:
    SyncCell preDelay, decay;
    Knob size, highCut, lowCut, shimmer, early, mod, depth, mix;
    ComboCell type;
    Toggle preSync, decaySync;
    FreezeButton freeze;
    std::unique_ptr<APVTS::ButtonAttachment> freezeAtt;
    juce::Label* typeCaption = nullptr;
    juce::Label* hintCaption = nullptr;
};

//==============================================================================
class FilterPanel : public ModulePanel
{
public:
    explicit FilterPanel (DusiVerbProcessor& p)
        : ModulePanel (p, U ("FILTR S LFO"), ui::filter, "filterOn",
                       U ("Filtr, který se sám pohybuje: LFO otáčí frekvencí v rytmu skladby. Dobré na pady, plucky a „dýchající“ ozvěny."),
                       (int) dusi::Module::filter),
          type (*this, p.apvts, "filterType", "TYP"),
          shape (*this, p.apvts, "lfoShape", "TVAR LFO"),
          freq (*this, p.apvts, "filterFreq", "FREKVENCE", ui::filter),
          reso (*this, p.apvts, "filterReso", "REZONANCE", ui::filter),
          depth (*this, p.apvts, "lfoDepth", "HLOUBKA LFO", ui::filter),
          stereo (*this, p.apvts, "lfoStereo", "STEREO", ui::filter),
          mix (*this, p.apvts, "filterMix", "MIX", ui::filter),
          rate (*this, p.apvts, "lfoRate", "lfoDiv", "RYCHLOST", ui::filter),
          sync (*this, p.apvts, "lfoSync", U ("Sync na tempo"), ui::filter)
    {
        typeCaption = addCaption (U ("dolní · pásmová · horní propust"));
        shapeCaption = addCaption (U ("Sinus · Trojúhelník · Pila · Obdélník · S&H"));
        rateCaption = addCaption ({});
        rateCaption->setColour (juce::Label::textColourId, ui::filter);
        rateCaption->setFont (ui::font (11.0f, 500));
    }

    void layout() override
    {
        type.setBounds ({ 15, 47, 150, 50 });
        typeCaption->setBounds (15, 101, 170, 16);
        int x = 185;
        for (auto* k : { &freq, &reso, &mix }) { k->setTopLeft (x, 41); x += 92; }

        rate.setTopLeft (13, 159);
        sync.button.setBounds (113, 186, 170, 22);
        rateCaption->setBounds (139, 212, 170, 16);
        shape.setBounds ({ 300, 165, 150, 50 });
        shapeCaption->setBounds (300, 219, 230, 16);
        x = 540;
        for (auto* k : { &depth, &stereo }) { k->setTopLeft (x, 159); x += 92; }
    }

    void refresh() override
    {
        const bool synced = proc.apvts.getRawParameterValue ("lfoSync")->load() > 0.5f;
        rate.update (synced, {});
        const auto text = "= " + juce::String (proc.uiLfoHz.load(), 2) + U (" Hz při ") + juce::String (proc.uiBpm.load(), 0) + " BPM";
        if (rateCaption->getText() != text) rateCaption->setText (text, juce::dontSendNotification);
        rateCaption->setVisible (synced);
        const bool off = ! isOn();
        type.setDimmed (off);
        shape.setDimmed (off);
        rate.setDimmed (off);
        for (auto* k : { &freq, &reso, &depth, &stereo, &mix }) k->setDimmed (off);
        sync.button.setAlpha (off ? 0.45f : 1.0f);
        updateTitle();
    }

private:
    ComboCell type, shape;
    Knob freq, reso, depth, stereo, mix;
    SyncCell rate;
    Toggle sync;
    juce::Label* typeCaption = nullptr;
    juce::Label* shapeCaption = nullptr;
    juce::Label* rateCaption = nullptr;
};

//==============================================================================
class ChorusPanel : public ModulePanel
{
public:
    explicit ChorusPanel (DusiVerbProcessor& p)
        : ModulePanel (p, "CHORUS / ENSEMBLE", ui::chorus, "chorusOn",
                       U ("Rozvlní a rozšíří zvuk – pady, el. piano, synthy. Před reverbem dělá širší a měkčí prostor."),
                       (int) dusi::Module::chorus),
          rate (*this, p.apvts, "chorusRate", "RYCHLOST", ui::chorus),
          depth (*this, p.apvts, "chorusDepth", "HLOUBKA", ui::chorus),
          mix (*this, p.apvts, "chorusMix", "MIX", ui::chorus)
    {
        setVizRows (1);
    }

    void layout() override
    {
        int i = 0;
        for (auto* k : { &rate, &depth, &mix }) { k->setTopLeft (knobPos (i, 0).x, 41); ++i; }
    }
    void refresh() override
    {
        const bool off = ! isOn();
        for (auto* k : { &rate, &depth, &mix }) k->setDimmed (off);
        updateTitle();
    }

private:
    Knob rate, depth, mix;
};

//==============================================================================
class LofiPanel : public ModulePanel
{
public:
    explicit LofiPanel (DusiVerbProcessor& p)
        : ModulePanel (p, "LO-FI", ui::lofi, "lofiOn",
                       U ("Retro zvuk: méně bitů = zrnitější, nižší vzorkování = „digitální“ výšky, TÓN tlumí ostrost. Za reverbem = starý sampler."),
                       (int) dusi::Module::lofi),
          bits (*this, p.apvts, "lofiBits", "BITY", ui::lofi),
          rate (*this, p.apvts, "lofiRate", U ("VZORKOVÁNÍ"), ui::lofi),
          tone (*this, p.apvts, "lofiTone", U ("TÓN"), ui::lofi),
          mix (*this, p.apvts, "lofiMix", "MIX", ui::lofi)
    {
        setVizRows (1);
    }

    void layout() override
    {
        int i = 0;
        for (auto* k : { &bits, &rate, &tone, &mix }) { k->setTopLeft (knobPos (i, 0).x, 41); ++i; }
    }
    void refresh() override
    {
        const bool off = ! isOn();
        for (auto* k : { &bits, &rate, &tone, &mix }) k->setDimmed (off);
        updateTitle();
    }

private:
    Knob bits, rate, tone, mix;
};

//==============================================================================
class DynamicsPanel : public ModulePanel
{
public:
    explicit DynamicsPanel (DusiVerbProcessor& p)
        : ModulePanel (p, {}, ui::dynamics, nullptr, {}),
          duck (*this, p.apvts, "duckAmount", "DUCKING", ui::dynamics, Knob::Size::md),
          release (*this, p.apvts, "duckRelease", "RELEASE", ui::dynamics, Knob::Size::md),
          comp (*this, p.apvts, "compAmount", "KOMPRESE", ui::dynamics, Knob::Size::md)
    {
        duck.slider.setTooltip (U ("Ozvěna a dozvuk ustoupí, když hraje zdroj, a vrátí se v pauzách – čitelný vokál i s velkým prostorem"));
        comp.slider.setTooltip (U ("Srovná dozvuk a ozvěnu, aby seděly v mixu"));
        addAndMakeVisible (meter);
    }

    void paint (juce::Graphics& g) override
    {
        ui::paintPanel (g, getLocalBounds(), U ("DYNAMIKA EFEKTŮ"), ui::text, ui::smallTitle, "delay + reverb");
        auto r = juce::Rectangle<int> (meter.getRight() + 28, 40, getWidth() - meter.getRight() - 40, 80);
        auto para = [&] (const juce::String& head, const juce::String& body)
        {
            juce::AttributedString s;
            s.append (head + " ", ui::font (12.0f, 600), ui::text2);
            s.append (body, ui::font (12.0f, 500), ui::textDim);
            s.setLineSpacing (4.0f);
            s.setWordWrap (juce::AttributedString::byWord);
            juce::TextLayout tl;
            tl.createLayout (s, (float) r.getWidth());
            tl.draw (g, r.removeFromTop ((int) tl.getHeight() + 6).toFloat());
        };
        para ("Ducking:", U ("ozvěna a dozvuk ustoupí, když zdroj hraje, a vrátí se v pauzách (RELEASE = jak rychle)."));
        para ("Komprese:", U ("srovná dozvuk, aby nevyskakoval."));
    }

    void layout() override
    {
        int x = 12;
        for (auto* k : { &duck, &release, &comp }) { k->setTopLeft (x, ui::smallTitle); x += 76 + 12; }
        meter.setBounds (x + 24, ui::smallTitle + 2, 236, 92);
    }

    void refresh() override
    {
        const float g = proc.uiDuckGain.load();
        const bool active = proc.apvts.getRawParameterValue ("duckAmount")->load() > 0.5f;
        meter.setGainDb (20.0f * std::log10 (std::max (g, 1.0e-4f)), active);
    }

private:
    Knob duck, release, comp;
    DuckMeter meter;
};
}

//==============================================================================
EffectsPage::EffectsPage (DusiVerbProcessor& p) : proc (p), chain (p)
{
    setOpaque (true);
    addAndMakeVisible (chain);
    panels[(size_t) dusi::Module::delay]  = std::make_unique<DelayPanel> (p);
    panels[(size_t) dusi::Module::reverb] = std::make_unique<ReverbPanel> (p);
    panels[(size_t) dusi::Module::filter] = std::make_unique<FilterPanel> (p);
    panels[(size_t) dusi::Module::chorus] = std::make_unique<ChorusPanel> (p);
    panels[(size_t) dusi::Module::lofi]   = std::make_unique<LofiPanel> (p);
    for (auto& panel : panels) addChildComponent (*panel);
    dynamics = std::make_unique<DynamicsPanel> (p);
    addAndMakeVisible (*dynamics);

    chain.onSelect = [this] (dusi::Module m) { select (m); };
    select (dusi::Module::reverb);
    startTimerHz (10);
}

EffectsPage::~EffectsPage() = default;

void EffectsPage::select (dusi::Module m)
{
    current = m;
    for (int i = 0; i < dusi::kNumModules; ++i)
        panels[(size_t) i]->setVisible (i == (int) m);
    chain.setSelected (m);
    timerCallback();
}

void EffectsPage::timerCallback()
{
    const int hz = proc.eco.load() ? 4 : 10;
    if (getTimerInterval() != 1000 / hz) startTimerHz (hz);
    if (! isShowing()) return;
    panels[(size_t) current]->refresh();
    static_cast<ModulePanel*> (dynamics.get())->refresh();
}

void EffectsPage::paint (juce::Graphics& g)
{
    g.fillAll (ui::background);
}

void EffectsPage::resized()
{
    const int W = getWidth() - 2 * ui::pad;
    chain.setBounds (ui::pad, 8, W, kChainH);
    const auto panelArea = juce::Rectangle<int> (ui::pad, 8 + kChainH + ui::gap, W, kPanelH);
    for (auto& panel : panels) panel->setBounds (panelArea);
    dynamics->setBounds (ui::pad, panelArea.getBottom() + ui::gap, W, kDynH);
}
