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
            if (r->paramID.startsWith ("eq") || r->paramID.startsWith ("dq") || r->paramID == "decayEqOn")
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
    return { freqToX (f), multToY (value (DusiVerbProcessor::dqId (index, "Mult"))) };
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
    const float freqs[] = { 50, 100, 200, 500, 1000, 2000, 5000, 10000 };
    const char* labels[] = { "50", "100", "200", "500", "1k", "2k", "5k", "10k" };
    for (int i = 0; i < 8; ++i)
    {
        const float x = freqToX (freqs[i]);
        g.setColour (juce::Colour (0xff141d30));
        g.drawVerticalLine ((int) x, graph.getY(), graph.getBottom());
        g.setColour (ui::textDim);
        g.drawText (labels[i], juce::Rectangle<float> (40.0f, 16.0f).withCentre ({ x, graph.getBottom() + 10.0f }), juce::Justification::centred);
    }

    for (int k = -2; k <= 2; ++k)
    {
        const float db = (float) k * 6.0f * (mode == Mode::output ? 1.0f : 1.5f);
        const float y = dbToY (db);
        g.setColour (k == 0 ? juce::Colour (0xff2c3a56) : juce::Colour (0xff1e2a42));
        g.drawHorizontalLine ((int) y, graph.getX(), graph.getRight());
        juce::String label;
        if (mode == Mode::output)
            label = (k > 0 ? "+" : "") + juce::String ((int) db);
        else
        {
            const char* m[] = { "×0.25", "×0.5", "×1", "×2", "×4" };
            label = ui::U (m[k + 2]);
        }
        g.setColour (mode == Mode::output ? ui::textDim : ui::verb.withAlpha (0.9f));
        g.drawText (label, juce::Rectangle<float> (34.0f, 14.0f).withCentre ({ 20.0f, y }), juce::Justification::centred);
    }
}

void EqView::drawAnalyzer (juce::Graphics& g)
{
    if (spectrum.empty()) return;
    const float sr = (float) processor.getSampleRateForUi();
    const int n = 1 << fftOrder;
    const float binHz = sr / (float) n;

    juce::Path p;
    bool started = false;
    for (float x = graph.getX(); x <= graph.getRight(); x += 3.0f)
    {
        const float f = xToFreq (x);
        const float bin = f / binHz;
        const int b0 = juce::jlimit (0, (int) spectrum.size() - 2, (int) bin);
        const float frac = juce::jlimit (0.0f, 1.0f, bin - (float) b0);
        float db = spectrum[(size_t) b0] + frac * (spectrum[(size_t) b0 + 1] - spectrum[(size_t) b0]);
        db += 4.5f * std::log2 (f / 1000.0f);                     // naklon 4.5 dB/okt jako u FabFilteru
        const float y = juce::jmap (juce::jlimit (-90.0f, 6.0f, db), -90.0f, 6.0f, graph.getBottom(), graph.getY());
        if (! started) { p.startNewSubPath (x, y); started = true; } else p.lineTo (x, y);
    }
    g.setColour (ui::textDim.withAlpha (0.35f));
    g.strokePath (p, juce::PathStrokeType (1.2f));
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

    const float alpha = (active ? 1.0f : 0.3f) * (on ? 1.0f : 0.4f);
    if (active)
    {
        juce::Path fill (p);
        fill.lineTo (graph.getRight(), dbToY (0));
        fill.lineTo (graph.getX(), dbToY (0));
        fill.closeSubPath();
        g.setColour (ui::eq.withAlpha (0.10f * alpha));
        g.fillPath (fill);

        // krivka vybraneho pasma
        if (selected >= 0 && params.eq[(size_t) selected].on)
        {
            juce::Path bp;
            bool bpStarted = false;
            for (float x = graph.getX(); x <= graph.getRight() + 1.0f; x += 3.0f)
            {
                const float y = dbToY (juce::jlimit (-kDbRange * 1.2f, kDbRange * 1.2f,
                                                     dusi::eqBandMagnitudeDb (params.eq[(size_t) selected], xToFreq (x), sr)));
                if (! bpStarted) { bp.startNewSubPath (x, y); bpStarted = true; } else bp.lineTo (x, y);
            }
            g.setColour (kBandColours[selected].withAlpha (0.5f));
            g.strokePath (bp, juce::PathStrokeType (1.2f));
        }
    }
    g.setColour (ui::eq.withAlpha (alpha));
    g.strokePath (p, juce::PathStrokeType (active ? 2.0f : 1.2f));
}

void EqView::drawDecayCurve (juce::Graphics& g, bool active)
{
    auto params = processor.currentParams();
    const bool on = params.decayEqOn;
    params.decayEqOn = true;   // ukaz krivku i vypnutou (ztlumene)
    const float sr = (float) processor.getSampleRateForUi();
    const float rt = juce::jlimit (0.2f, 20.0f, params.decaySec);
    dusi::DecayEqDesign d;
    d.compute (params, rt, sr);

    juce::Path p;
    bool started = false;
    for (float x = graph.getX(); x <= graph.getRight() + 1.0f; x += 3.0f)
    {
        const float y = multToY (juce::jlimit (0.2f, 5.0f, d.multiplierAt (xToFreq (x), rt)));
        if (! started) { p.startNewSubPath (x, y); started = true; } else p.lineTo (x, y);
    }
    const float alpha = (active ? 1.0f : 0.25f) * (on ? 1.0f : 0.4f);
    if (active)
    {
        juce::Path fill (p);
        fill.lineTo (graph.getRight(), dbToY (0));
        fill.lineTo (graph.getX(), dbToY (0));
        fill.closeSubPath();
        g.setColour (ui::verb.withAlpha (0.12f * alpha));
        g.fillPath (fill);
    }
    g.setColour (ui::verb.withAlpha (alpha));
    const float dashes[] = { 6.0f, 4.0f };
    if (active)
        g.strokePath (p, juce::PathStrokeType (2.0f));
    else
    {
        juce::Path dashed;
        juce::PathStrokeType (1.2f).createDashedStroke (dashed, p, dashes, 2);
        g.fillPath (dashed);
    }

    if (active)
    {
        g.setColour (ui::verb.withAlpha (0.8f));
        g.setFont (ui::font (11.0f, 500));
        g.drawText (ui::U ("délka dozvuku · základ ") + juce::String (rt, 2) + " s" + (on ? "" : ui::U ("  (vypnuto)")),
                    graph.reduced (8.0f, 4.0f).removeFromTop (14.0f), juce::Justification::centredRight);
    }
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
        g.setColour (col);
        g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (pos));
        g.setColour (i == selected ? ui::text : ui::inset);
        g.drawEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (pos), i == selected ? 2.0f : 1.5f);
        g.setColour (juce::Colour (0xff06101e));
        g.setFont (ui::font (10.0f, 700));
        g.drawText (juce::String (i + 1), juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (pos), juce::Justification::centred);
    }

    const int infoIndex = hover >= 0 ? hover : selected;
    if (infoIndex >= 0 && nodeVisible (infoIndex))
    {
        const auto pos = nodePos (infoIndex);
        const auto text = nodeText (infoIndex);
        g.setFont (ui::font (11.0f, 500));
        const float w = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), text) + 16.0f;
        auto box = juce::Rectangle<float> (w, 20.0f).withCentre ({ pos.x, pos.y - 22.0f });
        if (box.getY() < graph.getY()) box.setY (pos.y + 14.0f);
        box = box.constrainedWithin (graph);
        g.setColour (ui::raised.withAlpha (0.95f));
        g.fillRoundedRectangle (box, 5.0f);
        g.setColour (ui::lineStrong);
        g.drawRoundedRectangle (box.reduced (0.5f), 5.0f, 1.0f);
        g.setColour (ui::text);
        g.drawText (text, box, juce::Justification::centred);
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
        setValue (DusiVerbProcessor::dqId (selected, "Mult"), juce::jlimit (0.25f, 4.0f, yToMult (e.position.y)));
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
        processor.readAnalyzer (readBuf.data(), (int) readBuf.size());   // jen vyprazdnit frontu
        if (overlayKey() != paintedKey) repaint();
        return;
    }
    analyzerOn = true;

    // nove vzorky z procesoru -> kruhova historie
    const int n = 1 << fftOrder;
    const int got = processor.readAnalyzer (readBuf.data(), n);
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
