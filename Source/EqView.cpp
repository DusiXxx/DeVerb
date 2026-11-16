#include "EqView.h"

namespace
{
const juce::Colour kBandColours[dusi::kNumEqBands] = {
    juce::Colour (0xffff6b6b), juce::Colour (0xffff9f43), juce::Colour (0xfff5c451), juce::Colour (0xff9be15d),
    juce::Colour (0xff2fd6b5), juce::Colour (0xff4da8ff), juce::Colour (0xff9d8cff), juce::Colour (0xfff06bd0) };

constexpr float kDbRange = 20.0f;        // +-20 dB na vysku grafu (body jdou do +-18)
constexpr float kMinF = 20.0f, kMaxF = 20000.0f;

juce::String hz (float f)
{
    return f >= 1000.0f ? juce::String (f / 1000.0f, f >= 10000.0f ? 1 : 2) + " kHz" : juce::String (juce::roundToInt (f)) + " Hz";
}
}

EqView::EqView (DusiVerbProcessor& p) : processor (p)
{
    setOpaque (true);
    // parametry, ktere ovlivnuji krivky (ukazatele jednou, ne hledani podle jmena kazdy snimek)
    for (auto* prm : p.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            if (r->paramID.startsWith ("eq") || r->paramID.startsWith ("dq") || r->paramID == "decayEqOn"
                || r->paramID == "reverbType" || r->paramID == "highCut" || r->paramID == "freeze" || r->paramID == "size")
                watched.push_back (p.apvts.getRawParameterValue (r->paramID));
    setupFft (11);
    startTimerHz (20);
}

EqView::~EqView()
{
    endGestures();
}

void EqView::setupFft (int order)
{
    fftOrder = order;
    const int n = 1 << order;
    fft = std::make_unique<juce::dsp::FFT> (order);
    window = std::make_unique<juce::dsp::WindowingFunction<float>> ((size_t) n, juce::dsp::WindowingFunction<float>::hann, false);
    history.assign ((size_t) n, 0.0f);
    fftData.assign ((size_t) n * 2, 0.0f);
    readBuf.assign ((size_t) n, 0.0f);
    spectrum.assign ((size_t) n / 2 + 1, -120.0f);
    writePos = 0;
}

//------------------------------------------------------------------------------
float EqView::value (const juce::String& id) const
{
    return processor.apvts.getRawParameterValue (id)->load();
}

void EqView::setValue (const juce::String& id, float v)
{
    if (auto* p = processor.apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (v));
}

void EqView::beginGesture (std::initializer_list<juce::String> ids)
{
    endGestures();
    for (auto& id : ids)
        if (auto* p = processor.apvts.getParameter (id))
        {
            p->beginChangeGesture();
            gestureIds.add (id);
        }
}

void EqView::endGestures()
{
    for (auto& id : gestureIds)
        if (auto* p = processor.apvts.getParameter (id))
            p->endChangeGesture();
    gestureIds.clear();
}

//------------------------------------------------------------------------------
void EqView::resized()
{
    graph = getLocalBounds().toFloat().withTrimmedLeft (41.0f).withTrimmedBottom (30.0f).withTrimmedRight (15.0f).withTrimmedTop (15.0f);
}

float EqView::freqToX (float f) const
{
    return graph.getX() + graph.getWidth() * std::log (juce::jlimit (kMinF, kMaxF, f) / kMinF) / std::log (kMaxF / kMinF);
}

float EqView::xToFreq (float x) const
{
    return kMinF * std::pow (kMaxF / kMinF, juce::jlimit (0.0f, 1.0f, (x - graph.getX()) / graph.getWidth()));
}

float EqView::dbToY (float db) const
{
    return graph.getCentreY() - db / kDbRange * graph.getHeight() * 0.5f;
}

float EqView::yToDb (float y) const
{
    return (graph.getCentreY() - y) / (graph.getHeight() * 0.5f) * kDbRange;
}

namespace
{
constexpr float kSecMin = 0.1f, kSecMax = 30.0f;
}

float EqView::secToY (float sec) const
{
    const float t = std::log (juce::jlimit (kSecMin, kSecMax, sec) / kSecMin) / std::log (kSecMax / kSecMin);
    return graph.getBottom() - t * graph.getHeight();
}

float EqView::yToSec (float y) const
{
    const float t = juce::jlimit (0.0f, 1.0f, (graph.getBottom() - y) / graph.getHeight());
    return kSecMin * std::pow (kSecMax / kSecMin, t);
}

float EqView::rtBase() const
{
    return juce::jlimit (0.2f, 20.0f, processor.uiDecaySec.load() > 0.0f ? processor.uiDecaySec.load() : value ("decay"));
}

float EqView::tiltOf (int band) const
{
    static const float tilt[dusi::kNumDecayBands] = { 0.5f, 0.85f, 1.15f, 1.45f };
    return (int) value ("reverbType") == (int) dusi::ReverbType::prism ? tilt[juce::jlimit (0, dusi::kNumDecayBands - 1, band)] : 1.0f;
}

// Skutecna delka dozvuku na frekvenci: decay EQ (vc. PRISM) + tlumeni vysek (HIGH CUT) v kazdem obehu smycky
float EqView::effectiveRt (float f, const dusi::Params& p, const dusi::DecayEqDesign& d, float rt) const
{
    const float rtEq = rt * d.multiplierAt (f, rt);
    if (p.freeze) return rtEq;
    const float sr = (float) processor.getSampleRateForUi();
    const float tau = 0.05f;                                              // prumerna delka obehu smycky
    const float a = std::exp (-2.0f * juce::MathConstants<float>::pi * juce::jlimit (500.0f, 0.45f * sr, p.highCut) / sr)
                  * juce::jmin (1.0f, tau / 0.33f);
    const float w = 2.0f * juce::MathConstants<float>::pi * f / sr;
    const std::complex<float> z = std::polar (1.0f, -w);
    const float mag = std::abs ((1.0f - a) / (1.0f - a * z));
    const float lossDbPerSec = -20.0f * std::log10 (juce::jmax (1.0e-6f, mag)) / tau;
    return 1.0f / (1.0f / rtEq + lossDbPerSec / 60.0f);
}

void EqView::setMode (Mode m)
{
    if (m == mode) return;
    mode = m;
    selected = -1;
    resetSpectrum();
    bgCache = {};
    repaint();
}

void EqView::resetSpectrum()
{
    std::fill (spectrum.begin(), spectrum.end(), -120.0f);
    std::fill (history.begin(), history.end(), 0.0f);
}

bool EqView::isGainType (int band) const
{
    const int t = (int) value (DusiVerbProcessor::eqId (band, "Type"));
    return t == (int) dusi::EqType::bell || t == (int) dusi::EqType::lowShelf || t == (int) dusi::EqType::highShelf;
}

bool EqView::nodeVisible (int index) const
{
    return mode == Mode::decay || value (DusiVerbProcessor::eqId (index, "On")) > 0.5f;
}

juce::Point<float> EqView::nodePos (int index) const
{
    if (mode == Mode::output)
    {
        const float f = value (DusiVerbProcessor::eqId (index, "Freq"));
        const float db = isGainType (index) ? value (DusiVerbProcessor::eqId (index, "Gain")) : 0.0f;
        return { freqToX (f), dbToY (db) };
    }
    const float f = value (DusiVerbProcessor::dqId (index, "Freq"));
    return { freqToX (f), secToY (rtBase() * value (DusiVerbProcessor::dqId (index, "Mult")) * tiltOf (index)) };
}

int EqView::hitNode (juce::Point<float> p) const
{
    int best = -1;
    float bestD = 11.0f;
    for (int i = 0; i < numNodes(); ++i)
    {
        if (! nodeVisible (i)) continue;
        const float d = p.getDistanceFrom (nodePos (i));
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

juce::String EqView::nodeText (int i) const
{
    using namespace ui;
    if (mode == Mode::output)
    {
        const int t = (int) value (DusiVerbProcessor::eqId (i, "Type"));
        juce::String s = DusiVerbProcessor::eqTypeNames()[t] + "  " + hz (value (DusiVerbProcessor::eqId (i, "Freq")));
        if (isGainType (i))
        {
            const float g = value (DusiVerbProcessor::eqId (i, "Gain"));
            s << "  " << (g >= 0 ? "+" : "") << juce::String (g, 1) << " dB";
        }
        if (t == (int) dusi::EqType::lowCut || t == (int) dusi::EqType::highCut)
            s << "  " << (12 << (int) value (DusiVerbProcessor::eqId (i, "Slope"))) << " dB/okt";
        else
            s << "  Q " << juce::String (value (DusiVerbProcessor::eqId (i, "Q")), 2);
        return s;
    }
    const char* names[] = { "Basy", "Středy 1", "Středy 2", "Výšky" };
    return U (names[i]) + "  " + hz (value (DusiVerbProcessor::dqId (i, "Freq"))) + U ("  dozvuk ×")
           + juce::String (value (DusiVerbProcessor::dqId (i, "Mult")), 2);
}

//------------------------------------------------------------------------------
// Kresli se ve trech vrstvach: mrizka (cache), analyzator (zivy), krivky + body (cache, prekresli se
// jen kdyz se zmeni nastaveni EQ). Diky tomu kazdy snimek stoji jen par operaci.
std::vector<float> EqView::overlayKey() const
{
    std::vector<float> k;
    k.reserve (watched.size() + 8);
    for (auto* a : watched) k.push_back (a->load());
    k.push_back ((float) mode);
    k.push_back ((float) selected);
    k.push_back ((float) hover);
    k.push_back ((float) getWidth());
    k.push_back ((float) getHeight());
    k.push_back (processor.uiDecaySec.load());
    return k;
}

void EqView::paint (juce::Graphics& g)
{
    const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    auto makeImage = [this, scale] (juce::Image::PixelFormat fmt)
    {
        return juce::Image (fmt, juce::jmax (1, juce::roundToInt ((float) getWidth() * scale)),
                            juce::jmax (1, juce::roundToInt ((float) getHeight() * scale)), true);
    };

    const int bgKey = getWidth() * 7919 + getHeight() * 31 + (int) mode;
    if (bgCache.isNull() || bgKey != bgCacheKey || ! juce::approximatelyEqual (scale, cacheScale))
    {
        bgCache = makeImage (juce::Image::RGB);
        juce::Graphics ig (bgCache);
        ig.addTransform (juce::AffineTransform::scale (scale));
        ig.fillAll (ui::panel);
        ig.setColour (ui::inset);
        ig.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
        ig.setColour (ui::line);
        ig.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 6.0f, 1.0f);
        drawGrid (ig);
        bgCacheKey = bgKey;
        overlayCache = {};
    }
    g.drawImage (bgCache, getLocalBounds().toFloat());

    if (analyzerOn)
    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (graph.toNearestInt());
        drawAnalyzer (g);
    }

    const auto key = overlayKey();
    if (overlayCache.isNull() || key != overlayCacheKey || ! juce::approximatelyEqual (scale, cacheScale))
    {
        overlayCache = makeImage (juce::Image::ARGB);
        juce::Graphics ig (overlayCache);
        ig.addTransform (juce::AffineTransform::scale (scale));
        {
            juce::Graphics::ScopedSaveState s (ig);
            ig.reduceClipRegion (graph.toNearestInt());
            if (mode == Mode::output) { drawDecayCurve (ig, false); drawOutputCurve (ig, true); }
            else                      { drawOutputCurve (ig, false); drawDecayCurve (ig, true); }
        }
        drawNodes (ig);
        overlayCacheKey = key;
    }
    g.drawImage (overlayCache, getLocalBounds().toFloat());
    cacheScale = scale;
    paintedKey = key;
}

void EqView::drawGrid (juce::Graphics& g)
{
    g.setFont (ui::font (10.0f, 500));
    const float freqs[] = { 30, 50, 100, 200, 500, 1000, 2000, 5000, 10000 };
    const char* labels[] = { "30", "50", "100", "200", "500", "1k", "2k", "5k", "10k" };
    for (int i = 0; i < 9; ++i)
    {
        const float x = freqToX (freqs[i]);
        const bool major = freqs[i] == 100 || freqs[i] == 1000 || freqs[i] == 10000;
        g.setColour (major ? juce::Colour (0xff1e2a42) : juce::Colour (0xff141d30));
        g.drawVerticalLine ((int) x, graph.getY(), graph.getBottom());
        g.setColour (ui::textDim);
        g.drawText (labels[i], juce::Rectangle<float> (40.0f, 16.0f).withCentre ({ x, graph.getBottom() + 12.0f }), juce::Justification::centred);
    }

    if (mode == Mode::output)
    {
        for (int k = -3; k <= 3; ++k)
        {
            const float db = (float) k * 6.0f;
            const float y = dbToY (db);
            g.setColour (k == 0 ? juce::Colour (0xff2c3a56) : juce::Colour (0xff1a2438));
            g.drawHorizontalLine ((int) y, graph.getX(), graph.getRight());
            g.setColour (k == 0 ? ui::text2 : ui::textDim);
            g.drawText ((k > 0 ? "+" : (k < 0 ? ui::U ("−") : juce::String())) + juce::String (std::abs ((int) db)),
                        juce::Rectangle<float> (34.0f, 14.0f).withCentre ({ 20.0f, y }), juce::Justification::centred);
        }
        return;
    }

    // DOZVUK: sekundy (logaritmicky)
    const float secs[] = { 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f };
    const char* sl[] = { "0.25 s", "0.5 s", "1 s", "2 s", "4 s", "8 s", "16 s" };
    for (int i = 0; i < 7; ++i)
    {
        const float y = secToY (secs[i]);
        g.setColour (secs[i] == 1.0f ? juce::Colour (0xff24304a) : juce::Colour (0xff1a2438));
        g.drawHorizontalLine ((int) y, graph.getX(), graph.getRight());
        g.setColour (ui::verb.withAlpha (0.85f));
        g.drawText (ui::U (sl[i]), juce::Rectangle<float> (38.0f, 14.0f).withCentre ({ 21.0f, y }), juce::Justification::centred);
    }
}

void EqView::drawAnalyzer (juce::Graphics& g)
{
    if (spectrum.empty()) return;
    const float sr = (float) processor.getSampleRateForUi();
    const int n = 1 << fftOrder;
    const float binHz = sr / (float) n;
    const bool tail = mode == Mode::decay;

    juce::Path line, fill;
    bool started = false;
    float prev = graph.getBottom();
    for (float x = graph.getX(); x <= graph.getRight() + 2.0f; x += 2.0f)
    {
        const float f = xToFreq (x);
        const float bin = f / binHz;
        const int b0 = juce::jlimit (0, (int) spectrum.size() - 2, (int) bin);
        const float frac = juce::jlimit (0.0f, 1.0f, bin - (float) b0);
        float db = spectrum[(size_t) b0] + frac * (spectrum[(size_t) b0 + 1] - spectrum[(size_t) b0]);
        db += 4.5f * std::log2 (f / 1000.0f);                     // naklon 4.5 dB/okt jako u FabFilteru
        float y = juce::jmap (juce::jlimit (-90.0f, 6.0f, db), -90.0f, 6.0f, graph.getBottom(), graph.getY() + graph.getHeight() * 0.08f);
        y = started ? prev + 0.55f * (y - prev) : y;              // vyhlazeni mezi sousedy
        prev = y;
        if (! started) { line.startNewSubPath (x, y); fill.startNewSubPath (x, graph.getBottom()); started = true; }
        else line.lineTo (x, y);
        fill.lineTo (x, y);
    }
    fill.lineTo (graph.getRight() + 2.0f, graph.getBottom());
    fill.closeSubPath();

    const auto col = tail ? ui::verb : juce::Colour (0xff8fb4e8);
    g.setGradientFill (juce::ColourGradient (col.withAlpha (tail ? 0.26f : 0.20f), 0.0f, graph.getY() + graph.getHeight() * 0.3f,
                                             col.withAlpha (0.02f), 0.0f, graph.getBottom(), false));
    g.fillPath (fill);
    g.setColour (col.withAlpha (tail ? 0.45f : 0.35f));
    g.strokePath (line, juce::PathStrokeType (1.0f));
}

void EqView::drawOutputCurve (juce::Graphics& g, bool active)
{
    const auto params = processor.currentParams();
    const float sr = (float) processor.getSampleRateForUi();
    const bool on = params.eqOn;

    juce::Path p;
    bool started = false;
    for (float x = graph.getX(); x <= graph.getRight() + 1.0f; x += 2.0f)
    {
        const float f = xToFreq (x);
        float db = 0.0f;
        for (const auto& b : params.eq)
            if (b.on) db += dusi::eqBandMagnitudeDb (b, f, sr);
        const float y = dbToY (juce::jlimit (-kDbRange * 1.2f, kDbRange * 1.2f, db));
        if (! started) { p.startNewSubPath (x, y); started = true; } else p.lineTo (x, y);
    }

    if (! active)   // v rezimu DOZVUK jen jemny duch
    {
        g.setColour (ui::eq.withAlpha (0.18f));
        g.strokePath (p, juce::PathStrokeType (1.0f));
        return;
    }

    const float alpha = on ? 1.0f : 0.35f;
    const float zeroY = dbToY (0);

    // vybrane pasmo: barevna plocha mezi jeho krivkou a nulou (jako Pro-Q)
    if (selected >= 0 && params.eq[(size_t) selected].on)
    {
        juce::Path bp;
        bool bpStarted = false;
        for (float x = graph.getX(); x <= graph.getRight() + 1.0f; x += 3.0f)
        {
            const float y = dbToY (juce::jlimit (-kDbRange * 1.2f, kDbRange * 1.2f,
                                                 dusi::eqBandMagnitudeDb (params.eq[(size_t) selected], xToFreq (x), sr)));
            if (! bpStarted) { bp.startNewSubPath (x, zeroY); bpStarted = true; }
            bp.lineTo (x, y);
        }
        bp.lineTo (graph.getRight() + 1.0f, zeroY);
        bp.closeSubPath();
        g.setColour (kBandColours[selected].withAlpha (0.16f * alpha));
        g.fillPath (bp);
    }

    // plocha celkove krivky: zare od krivky k nule
    juce::Path fill (p);
    fill.lineTo (graph.getRight() + 1.0f, zeroY);
    fill.lineTo (graph.getX(), zeroY);
    fill.closeSubPath();
    const auto bounds = p.getBounds();
    g.setGradientFill (juce::ColourGradient (ui::eq.withAlpha (0.20f * alpha), 0.0f, juce::jmin (bounds.getY(), zeroY - 1.0f),
                                             ui::eq.withAlpha (0.03f * alpha), 0.0f, zeroY, false));
    g.fillPath (fill);

    // krivka se zari
    g.setColour (ui::eq.withAlpha (0.10f * alpha));
    g.strokePath (p, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (ui::eq.withAlpha (0.22f * alpha));
    g.strokePath (p, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (ui::eq.withAlpha (alpha));
    g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void EqView::drawDecayCurve (juce::Graphics& g, bool active)
{
    if (! active) return;
    const auto params = processor.currentParams();
    const bool on = params.decayEqOn || params.reverbType == dusi::ReverbType::prism;
    const float sr = (float) processor.getSampleRateForUi();
    const float rt = rtBase();
    dusi::DecayEqDesign d;
    d.compute (dusi::prismDecayParams (params), rt, sr);

    // zaklad (DECAY)
    {
        juce::Path base;
        base.startNewSubPath (graph.getX(), secToY (rt));
        base.lineTo (graph.getRight(), secToY (rt));
        const float dashes[] = { 4.0f, 4.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (base, base, dashes, 2);
        g.setColour (ui::textDim.withAlpha (0.6f));
        g.fillPath (base);
    }

    juce::Path p, real;
    bool started = false;
    for (float x = graph.getX(); x <= graph.getRight() + 1.0f; x += 3.0f)
    {
        const float f = xToFreq (x);
        const float y = secToY (params.freeze ? kSecMax : rt * d.multiplierAt (f, rt));
        const float yr = secToY (params.freeze ? kSecMax : effectiveRt (f, params, d, rt));
        if (! started) { p.startNewSubPath (x, y); real.startNewSubPath (x, yr); started = true; }
        else { p.lineTo (x, y); real.lineTo (x, yr); }
    }

    // plocha skutecneho dozvuku (vc. HIGH CUT)
    juce::Path fill (real);
    fill.lineTo (graph.getRight() + 1.0f, graph.getBottom());
    fill.lineTo (graph.getX(), graph.getBottom());
    fill.closeSubPath();
    g.setGradientFill (juce::ColourGradient (ui::verb.withAlpha (on ? 0.22f : 0.12f), 0.0f, real.getBounds().getY(),
                                             ui::verb.withAlpha (0.02f), 0.0f, graph.getBottom(), false));
    g.fillPath (fill);
    g.setColour (ui::verb.withAlpha (0.45f));
    {
        juce::Path dashed;
        const float dashes[] = { 2.0f, 3.0f };
        juce::PathStrokeType (1.2f).createDashedStroke (dashed, real, dashes, 2);
        g.fillPath (dashed);
    }

    // krivka decay EQ se zari
    const float alpha = on ? 1.0f : 0.5f;
    g.setColour (ui::verb.withAlpha (0.12f * alpha));
    g.strokePath (p, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (ui::verb.withAlpha (alpha));
    g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // popis vpravo nahore
    juce::String info = params.freeze ? ui::U ("FREEZE – nekonečný dozvuk")
                                      : ui::U ("DECAY ") + juce::String (rt, 2) + " s";
    if (params.reverbType == dusi::ReverbType::prism) info << ui::U ("  ·  PRISM: krátké basy, dlouhé výšky");
    if (! params.decayEqOn) info << ui::U ("  ·  decay EQ vypnutý");
    g.setColour (ui::verb);
    g.setFont (ui::font (11.0f, 600));
    g.drawText (info, graph.reduced (10.0f, 6.0f).removeFromTop (14.0f), juce::Justification::centredRight);
    g.setColour (ui::textDim);
    g.setFont (ui::font (10.0f, 500));
    g.drawText (ui::U ("— nastavení   ┄ skutečně (s HIGH CUT)   ▒ spektrum dozvuku"),
                graph.reduced (10.0f, 6.0f).withTrimmedTop (16.0f).removeFromTop (14.0f), juce::Justification::centredRight);
}

void EqView::drawNodes (juce::Graphics& g)
{
    for (int i = 0; i < numNodes(); ++i)
    {
        if (! nodeVisible (i)) continue;
        const auto pos = nodePos (i);
        const auto col = mode == Mode::output ? kBandColours[i] : ui::verb;
        const bool hot = i == selected || i == hover;
        const float r = 9.0f;   // bod 18 px, kruh 28 px
        if (hot)
        {
            g.setColour (col.withAlpha (0.16f));
            g.fillEllipse (juce::Rectangle<float> (28.0f, 28.0f).withCentre (pos));
            g.setColour (col.withAlpha (0.6f));
            g.drawEllipse (juce::Rectangle<float> (28.0f, 28.0f).withCentre (pos), 1.0f);
        }
        // svisla linka k ose (jako Pro-Q) u vybraneho bodu
        if (i == selected)
        {
            g.setColour (col.withAlpha (0.25f));
            g.drawVerticalLine ((int) pos.x, graph.getY(), graph.getBottom());
        }
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillEllipse (juce::Rectangle<float> (r * 2.0f + 2.0f, r * 2.0f + 2.0f).withCentre (pos.translated (0.0f, 1.5f)));
        g.setColour (col);
        g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (pos));
        g.setColour (i == selected ? ui::text : ui::inset);
        g.drawEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (pos), i == selected ? 2.0f : 1.5f);
        g.setColour (juce::Colour (0xff06101e));
        g.setFont (ui::font (10.0f, 700));
        g.drawText (juce::String (i + 1), juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (pos), juce::Justification::centred);
    }

    if (selected >= 0 && nodeVisible (selected))
        drawBandPanel (g);

    if (hover >= 0 && hover != selected && nodeVisible (hover))
    {
        const auto pos = nodePos (hover);
        const auto text = nodeText (hover);
        g.setFont (ui::font (11.0f, 500));
        const float w = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), text) + 16.0f;
        auto box = juce::Rectangle<float> (w, 20.0f).withCentre ({ pos.x, pos.y - 24.0f });
        if (box.getY() < graph.getY()) box.setY (pos.y + 16.0f);
        box = box.constrainedWithin (graph);
        g.setColour (ui::raised.withAlpha (0.95f));
        g.fillRoundedRectangle (box, 5.0f);
        g.setColour (ui::lineStrong);
        g.drawRoundedRectangle (box.reduced (0.5f), 5.0f, 1.0f);
        g.setColour (ui::text);
        g.drawText (text, box, juce::Justification::centred);
    }
}

// Panel vybraneho pasma dole uprostred grafu (jako ve FabFilteru): typ, frekvence, zisk / delka, Q
void EqView::drawBandPanel (juce::Graphics& g)
{
    using namespace ui;
    const int i = selected;
    struct Cell { juce::String label, value; };
    std::vector<Cell> cells;
    juce::String title;
    juce::Colour col;
    if (mode == Mode::output)
    {
        const int t = (int) value (DusiVerbProcessor::eqId (i, "Type"));
        col = kBandColours[i];
        title = upper (DusiVerbProcessor::eqTypeNames()[t]);
        cells.push_back ({ "FREKVENCE", hz (value (DusiVerbProcessor::eqId (i, "Freq"))) });
        if (isGainType (i))
        {
            const float gdb = value (DusiVerbProcessor::eqId (i, "Gain"));
            cells.push_back ({ "ZISK", (gdb >= 0 ? "+" : U ("−")) + juce::String (std::abs (gdb), 1) + " dB" });
        }
        if (t == (int) dusi::EqType::lowCut || t == (int) dusi::EqType::highCut)
            cells.push_back ({ U ("STRMOST"), juce::String (12 << (int) value (DusiVerbProcessor::eqId (i, "Slope"))) + " dB/okt" });
        else
            cells.push_back ({ "Q", juce::String (value (DusiVerbProcessor::eqId (i, "Q")), 2) });
    }
    else
    {
        const char* names[] = { "BASY", "STŘEDY 1", "STŘEDY 2", "VÝŠKY" };
        col = verb;
        title = U (names[i]);
        const float m = value (DusiVerbProcessor::dqId (i, "Mult")) * tiltOf (i);
        cells.push_back ({ "FREKVENCE", hz (value (DusiVerbProcessor::dqId (i, "Freq"))) });
        cells.push_back ({ "DOZVUK", juce::String (rtBase() * m, 2) + " s" });
        cells.push_back ({ U ("NÁSOBEK"), U ("×") + juce::String (m, 2) });
    }

    const float cellW = 86.0f, titleW = 96.0f, h = 40.0f;
    const float w = titleW + cellW * (float) cells.size() + 8.0f;
    auto box = juce::Rectangle<float> (w, h).withCentre ({ graph.getCentreX(), graph.getBottom() - h * 0.5f - 8.0f });
    // kdyz by panel prekryl vybrany bod, presun ho nahoru
    if (box.expanded (14.0f).contains (nodePos (i))) box.setY (graph.getY() + 30.0f);
    g.setColour (juce::Colour (0xf0101827));
    g.fillRoundedRectangle (box, 7.0f);
    g.setColour (col.withAlpha (0.55f));
    g.drawRoundedRectangle (box.reduced (0.5f), 7.0f, 1.0f);

    auto r = box.reduced (10.0f, 0.0f);
    auto t = r.removeFromLeft (titleW - 10.0f);
    g.setColour (col);
    g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ t.getX() + 4.0f, t.getCentreY() }));
    g.setFont (font (11.0f, 700, 0.06f));
    g.drawText (juce::String (i + 1) + "  " + title, t.withTrimmedLeft (14.0f), juce::Justification::centredLeft, true);
    for (auto& c : cells)
    {
        auto cr = r.removeFromLeft (cellW);
        g.setColour (line);
        g.drawVerticalLine ((int) cr.getX(), cr.getY() + 8.0f, cr.getBottom() - 8.0f);
        g.setColour (textDim);
        g.setFont (font (9.5f, 600, 0.06f));
        g.drawText (c.label, cr.withTrimmedLeft (8.0f).withTrimmedBottom (h * 0.5f).withTrimmedTop (6.0f), juce::Justification::bottomLeft);
        g.setColour (text);
        g.setFont (font (12.0f, 600));
        g.drawText (c.value, cr.withTrimmedLeft (8.0f).withTrimmedTop (h * 0.5f).withTrimmedBottom (4.0f), juce::Justification::topLeft);
    }
}

//------------------------------------------------------------------------------
void EqView::mouseMove (const juce::MouseEvent& e)
{
    const int h = hitNode (e.position);
    if (h != hover) { hover = h; repaint(); }
    setMouseCursor (h >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
}

void EqView::mouseExit (const juce::MouseEvent&)
{
    if (hover != -1) { hover = -1; repaint(); }
}

void EqView::mouseDown (const juce::MouseEvent& e)
{
    const int h = hitNode (e.position);
    selected = h;
    if (h < 0) { repaint(); return; }

    if (e.mods.isPopupMenu())
    {
        if (mode == Mode::output) showBandMenu (h);
        return;
    }

    dragging = true;
    if (mode == Mode::output)
        beginGesture ({ DusiVerbProcessor::eqId (h, "Freq"), DusiVerbProcessor::eqId (h, "Gain") });
    else
        beginGesture ({ DusiVerbProcessor::dqId (h, "Freq"), DusiVerbProcessor::dqId (h, "Mult") });
    repaint();
}

void EqView::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging || selected < 0) return;
    const float f = juce::jlimit (kMinF, kMaxF, xToFreq (e.position.x));
    if (mode == Mode::output)
    {
        setValue (DusiVerbProcessor::eqId (selected, "Freq"), f);
        if (isGainType (selected))
            setValue (DusiVerbProcessor::eqId (selected, "Gain"), juce::jlimit (-18.0f, 18.0f, yToDb (e.position.y)));
    }
    else
    {
        setValue (DusiVerbProcessor::dqId (selected, "Freq"), f);
        setValue (DusiVerbProcessor::dqId (selected, "Mult"),
                  juce::jlimit (0.25f, 4.0f, yToSec (e.position.y) / (rtBase() * tiltOf (selected))));
    }
    repaint();
}

void EqView::mouseUp (const juce::MouseEvent&)
{
    dragging = false;
    endGestures();
}

void EqView::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int h = hitNode (e.position);
    if (mode == Mode::decay)
    {
        if (h >= 0)
        {
            beginGesture ({ DusiVerbProcessor::dqId (h, "Mult") });
            setValue (DusiVerbProcessor::dqId (h, "Mult"), 1.0f);
            endGestures();
        }
        return;
    }

    if (h >= 0)   // dvojklik na bod = vypnout pasmo
    {
        beginGesture ({ DusiVerbProcessor::eqId (h, "On") });
        setValue (DusiVerbProcessor::eqId (h, "On"), 0.0f);
        endGestures();
        selected = -1;
        repaint();
        return;
    }

    if (! graph.contains (e.position)) return;
    for (int b = 0; b < dusi::kNumEqBands; ++b)   // prvni volne pasmo
    {
        if (value (DusiVerbProcessor::eqId (b, "On")) > 0.5f) continue;
        const float f = xToFreq (e.position.x);
        const int type = f < 45.0f ? (int) dusi::EqType::lowCut : (f > 14000.0f ? (int) dusi::EqType::highCut : (int) dusi::EqType::bell);
        const bool gainType = type == (int) dusi::EqType::bell;
        beginGesture ({ DusiVerbProcessor::eqId (b, "On"), DusiVerbProcessor::eqId (b, "Type"), DusiVerbProcessor::eqId (b, "Freq"),
                        DusiVerbProcessor::eqId (b, "Gain"), DusiVerbProcessor::eqId (b, "Q") });
        setValue (DusiVerbProcessor::eqId (b, "Type"), (float) type);
        setValue (DusiVerbProcessor::eqId (b, "Freq"), f);
        setValue (DusiVerbProcessor::eqId (b, "Gain"), gainType ? juce::jlimit (-18.0f, 18.0f, yToDb (e.position.y)) : 0.0f);
        setValue (DusiVerbProcessor::eqId (b, "Q"), gainType ? 1.0f : 0.71f);
        setValue (DusiVerbProcessor::eqId (b, "On"), 1.0f);
        endGestures();
        selected = b;
        repaint();
        return;
    }
}

void EqView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const int h = hitNode (e.position) >= 0 ? hitNode (e.position) : selected;
    if (h < 0) return;
    juce::String id;
    if (mode == Mode::output)
        id = DusiVerbProcessor::eqId (h, "Q");
    else if (h == 1 || h == 2)
        id = DusiVerbProcessor::dqId (h, "Q");
    else
        return;
    beginGesture ({ id });
    setValue (id, value (id) * std::exp (wheel.deltaY * 2.0f));
    endGestures();
    repaint();
}

void EqView::showBandMenu (int band)
{
    using namespace ui;
    juce::PopupMenu m;
    const int type = (int) value (DusiVerbProcessor::eqId (band, "Type"));
    const auto& names = DusiVerbProcessor::eqTypeNames();
    for (int t = 0; t < names.size(); ++t)
        m.addItem (1 + t, names[t], true, t == type);
    if (type == (int) dusi::EqType::lowCut || type == (int) dusi::EqType::highCut)
    {
        m.addSeparator();
        const int slope = (int) value (DusiVerbProcessor::eqId (band, "Slope"));
        for (int s = 0; s < 3; ++s)
            m.addItem (11 + s, juce::String (12 << s) + " dB/okt", true, s == slope);
    }
    m.addSeparator();
    m.addItem (20, U ("Vypnout pásmo"));

    juce::Component::SafePointer<EqView> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                     [safe, band] (int r)
                     {
                         if (safe == nullptr || r == 0) return;
                         auto& self = *safe;
                         juce::String id;
                         float v = 0.0f;
                         if (r >= 1 && r <= 6)        { id = DusiVerbProcessor::eqId (band, "Type");  v = (float) (r - 1); }
                         else if (r >= 11 && r <= 13) { id = DusiVerbProcessor::eqId (band, "Slope"); v = (float) (r - 11); }
                         else if (r == 20)            { id = DusiVerbProcessor::eqId (band, "On");    v = 0.0f; self.selected = -1; }
                         self.beginGesture ({ id });
                         self.setValue (id, v);
                         self.endGestures();
                         self.repaint();
                     });
}

//------------------------------------------------------------------------------
void EqView::timerCallback()
{
    const bool eco = processor.eco.load();
    const int wantHz = eco ? 8 : 15;
    if (getTimerInterval() != 1000 / wantHz) startTimerHz (wantHz);
    if (! isShowing()) return;

    // ECO: analyzator vypnuty uplne (zadna FFT, zadne prekreslovani spektra)
    if (eco)
    {
        if (analyzerOn) { analyzerOn = false; repaint(); }
        processor.readAnalyzer (readBuf.data(), (int) readBuf.size(), false);   // jen vyprazdnit fronty
        processor.readAnalyzer (readBuf.data(), (int) readBuf.size(), true);
        if (overlayKey() != paintedKey) repaint();
        return;
    }
    analyzerOn = true;

    // nove vzorky z procesoru -> kruhova historie
    const int n = 1 << fftOrder;
    // VYSTUP = spektrum vystupu, DOZVUK = spektrum samotneho dozvuku (druhou frontu jen vyprazdnit)
    const bool tail = mode == Mode::decay;
    processor.readAnalyzer (readBuf.data(), n, ! tail);
    const int got = processor.readAnalyzer (readBuf.data(), n, tail);
    for (int i = 0; i < got; ++i)
    {
        history[(size_t) writePos] = readBuf[(size_t) i];
        writePos = (writePos + 1) % n;
    }

    float maxDb = -200.0f;
    for (auto v : spectrum) maxDb = std::max (maxDb, v);
    const bool idle = got == 0 && maxDb < -110.0f;   // ticho a spektrum uz dopadlo
    if (! idle)
    {
        std::fill (fftData.begin(), fftData.end(), 0.0f);
        for (int i = 0; i < n; ++i)
            fftData[(size_t) i] = got > 0 ? history[(size_t) ((writePos + i) % n)] : 0.0f;
        window->multiplyWithWindowingTable (fftData.data(), (size_t) n);
        fft->performFrequencyOnlyForwardTransform (fftData.data());

        for (size_t b = 0; b < spectrum.size(); ++b)
        {
            const float mag = fftData[b] * 4.0f / (float) n;
            const float db = 20.0f * std::log10 (mag + 1.0e-9f);
            spectrum[b] = std::max (db, spectrum[b] - 2.5f);
        }
        if (overlayKey() != paintedKey) repaint(); else repaint (graph.toNearestInt());
    }
    else if (overlayKey() != paintedKey)
        repaint();
}
