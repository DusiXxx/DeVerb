#include "EffectViews.h"
#include "ChainStrip.h"

using ui::U;

namespace
{
juce::String fmtMs (float ms)  { return ms >= 1000.0f ? juce::String (ms / 1000.0f, 2) + " s" : juce::String (ms, ms < 100.0f ? 1 : 0) + " ms"; }
juce::String fmtHz (float hz)  { return hz >= 1000.0f ? juce::String (hz / 1000.0f, hz >= 10000.0f ? 1 : 2) + " kHz" : juce::String (juce::roundToInt (hz)) + " Hz"; }

float lfoShape (dusi::LfoShape s, float ph)
{
    ph -= std::floor (ph);
    switch (s)
    {
        case dusi::LfoShape::triangle: return 1.0f - 4.0f * std::abs (ph - 0.5f);
        case dusi::LfoShape::saw:      return 2.0f * ph - 1.0f;
        case dusi::LfoShape::square:   return std::tanh (12.0f * std::sin (juce::MathConstants<float>::twoPi * ph)) / std::tanh (12.0f);
        case dusi::LfoShape::random:   { const float v[] = { 0.55f, -0.7f, 0.2f, -0.15f, 0.85f, -0.45f, 0.4f, -0.9f };
                                         return v[(int) (ph * 8.0f) % 8]; }
        case dusi::LfoShape::sine:
        default:                       return std::sin (juce::MathConstants<float>::twoPi * ph);
    }
}

void gridLines (juce::Graphics& g, juce::Rectangle<float> r, int nx, int ny)
{
    g.setColour (juce::Colour (0xff141d30));
    for (int i = 1; i < nx; ++i) g.drawVerticalLine ((int) (r.getX() + r.getWidth() * (float) i / (float) nx), r.getY(), r.getBottom());
    for (int j = 1; j < ny; ++j) g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * (float) j / (float) ny), r.getX(), r.getRight());
}
}

//==============================================================================
EffectViz::EffectViz (DusiVerbProcessor& p, dusi::Module m)
    : proc (p), module (m), accent (ChainStrip::moduleColour (m))
{
    setOpaque (false);
    setInterceptsMouseClicks (false, false);
}

void EffectViz::visibilityChanged()
{
    if (isVisible()) { startTimerHz (10); timerCallback(); }
    else stopTimer();
}

bool EffectViz::animated() const
{
    return module == dusi::Module::filter && ! proc.eco.load()
        && proc.apvts.getRawParameterValue ("filterOn")->load() > 0.5f;
}

std::vector<float> EffectViz::key() const
{
    const auto p = proc.currentParams();
    std::vector<float> k { (float) getWidth(), (float) getHeight(), (float) p.bpm };
    switch (module)
    {
        case dusi::Module::delay:  k.insert (k.end(), { p.delayOn ? 1.f : 0.f, p.delayMs, p.feedback, p.delayTone, p.pingPong ? 1.f : 0.f, p.delayMix }); break;
        case dusi::Module::reverb: k.insert (k.end(), { p.reverbOn ? 1.f : 0.f, p.preDelayMs, p.decaySec, p.early, p.reverbMix, p.freeze ? 1.f : 0.f,
                                                        p.shimmer, (float) p.reverbType, p.highCut }); break;
        case dusi::Module::filter: k.insert (k.end(), { p.filterOn ? 1.f : 0.f, (float) p.filterType, p.filterFreq, p.filterReso, p.lfoHz,
                                                        p.lfoDepth, (float) p.lfoShape, p.lfoStereo }); break;
        case dusi::Module::chorus: k.insert (k.end(), { p.chorusOn ? 1.f : 0.f, p.chorusRate, p.chorusDepth, p.chorusMix }); break;
        case dusi::Module::lofi:   k.insert (k.end(), { p.lofiOn ? 1.f : 0.f, p.lofiBits, p.lofiRate, p.lofiTone, p.lofiMix }); break;
    }
    return k;
}

void EffectViz::timerCallback()
{
    if (! isShowing()) return;
    const int hz = animated() ? 15 : 10;
    if (getTimerInterval() != 1000 / hz) startTimerHz (hz);

    if (key() != cacheKey) { repaint(); return; }
    if (animated())
    {
        const float ph = proc.uiLfoPhase.load();
        if (std::abs (ph - shownPhase) > 0.002f) repaint (lfoArea.toNearestInt().expanded (6));
    }
}

void EffectViz::paint (juce::Graphics& g)
{
    const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    auto k = key();
    if (cache.isNull() || k != cacheKey || ! juce::approximatelyEqual (scale, cacheScale))
    {
        cache = juce::Image (juce::Image::ARGB, juce::jmax (1, juce::roundToInt ((float) getWidth() * scale)),
                             juce::jmax (1, juce::roundToInt ((float) getHeight() * scale)), true);
        juce::Graphics ig (cache);
        ig.addTransform (juce::AffineTransform::scale (scale));
        paintStatic (ig, proc.currentParams());
        cacheKey = std::move (k);
        cacheScale = scale;
    }
    g.drawImage (cache, getLocalBounds().toFloat());

    // ziva poloha LFO (jen filtr, ne v ECO)
    if (animated() && ! lfoArea.isEmpty())
    {
        const auto p = proc.currentParams();
        const float ph = proc.uiLfoPhase.load();
        shownPhase = ph;
        const float beats = juce::jmax (0.01f, (float) (p.lfoHz > 0 ? p.bpm / 60.0 / p.lfoHz : 4.0));
        const float cycles = 8.0f / beats;          // 2 takty = 8 dob
        const float y = lfoArea.getCentreY() - lfoShape (p.lfoShape, ph) * lfoArea.getHeight() * 0.42f;
        for (int c = 0; c < 8 && (float) c < cycles; ++c)     // stejna faze v kazdem cyklu
        {
            const float x = lfoArea.getX() + lfoArea.getWidth() * ((float) c + ph) / juce::jmax (0.01f, cycles);
            if (x > lfoArea.getRight()) break;
            g.setColour (accent.withAlpha (0.3f));
            g.drawVerticalLine ((int) x, lfoArea.getY(), lfoArea.getBottom());
            g.setColour (accent);
            g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ x, y }));
        }
    }
}

void EffectViz::header (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& title, const juce::String& info, juce::Colour col)
{
    auto h = area.removeFromTop (18.0f);
    const auto tf = ui::font (11.0f, 500, 0.06f);
    g.setFont (tf);
    g.setColour (ui::textDim);
    g.drawText (title, h, juce::Justification::centredLeft);
    const float tw = (float) juce::GlyphArrangement::getStringWidthInt (tf, title);
    g.setFont (ui::font (11.0f, 500));
    g.setColour (col);
    g.drawText (info, h.withTrimmedLeft (tw + 12.0f), juce::Justification::centredLeft);
}

void EffectViz::paintStatic (juce::Graphics& g, const dusi::Params& p)
{
    const auto b = getLocalBounds().toFloat();
    g.setColour (ui::inset);
    g.fillRoundedRectangle (b, 6.0f);
    g.setColour (ui::line);
    g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, 1.0f);
    const auto area = b.reduced (12.0f, 8.0f);
    lfoArea = {};

    switch (module)
    {
        case dusi::Module::delay:  paintDelay (g, area, p); break;
        case dusi::Module::reverb: paintReverb (g, area, p); break;
        case dusi::Module::filter: paintFilter (g, area, p); break;
        case dusi::Module::chorus: paintChorus (g, area, p); break;
        case dusi::Module::lofi:   paintLofi (g, area, p); break;
    }

    const bool on = module == dusi::Module::delay ? p.delayOn : module == dusi::Module::reverb ? p.reverbOn
                  : module == dusi::Module::filter ? p.filterOn : module == dusi::Module::chorus ? p.chorusOn : p.lofiOn;
    if (! on)
    {
        g.setColour (ui::inset.withAlpha (0.8f));
        g.fillRoundedRectangle (b, 6.0f);
        g.setColour (ui::textDim);
        g.setFont (ui::font (12.0f, 500));
        g.drawText (U ("vypnuto – zapni vypínačem vpravo nahoře nebo tečkou v řetězu"), b, juce::Justification::centred);
    }
}

//------------------------------------------------------------------------------
void EffectViz::paintDelay (juce::Graphics& g, juce::Rectangle<float> a, const dusi::Params& p)
{
    header (g, a, U ("OPAKOVÁNÍ"), U ("= ") + fmtMs (p.delayMs) + U (" · feedback ") + juce::String (juce::roundToInt (p.feedback * 100.0f))
                                       + " %" + (p.pingPong ? U (" · ping-pong L ↔ P") : juce::String()), accent);
    auto plot = a.withTrimmedTop (24.0f);
    gridLines (g, plot, 8, 2);
    const float total = juce::jmax (p.delayMs * 8.0f, 500.0f);
    const float mid = plot.getCentreY();
    g.setColour (ui::lineStrong);
    g.drawHorizontalLine ((int) mid, plot.getX(), plot.getRight());

    // zdroj (cas 0) + opakovani; vyska = hlasitost v dB (-48 .. 0)
    auto barH = [&] (float gainDb) { return juce::jmax (0.0f, (gainDb + 48.0f) / 48.0f) * plot.getHeight() * 0.48f; };
    g.setColour (ui::text2);
    g.fillRoundedRectangle (plot.getX(), mid - barH (0.0f), 3.0f, barH (0.0f) * 2.0f, 1.5f);
    float gain = p.delayMix > 0.0f ? 20.0f * std::log10 (juce::jmax (0.02f, p.delayMix * 1.6f)) : -60.0f;
    for (int k = 1; ; ++k)
    {
        const float t = p.delayMs * (float) k;
        if (t > total || gain < -48.0f) break;
        const float x = plot.getX() + plot.getWidth() * t / total;
        const float h = barH (gain);
        const bool right = p.pingPong && (k % 2 == 0);
        g.setColour (accent.withAlpha (0.85f));
        if (p.pingPong)
            g.fillRoundedRectangle (x - 2.0f, right ? mid : mid - h, 4.0f, h, 2.0f);
        else
            g.fillRoundedRectangle (x - 2.0f, mid - h, 4.0f, h * 2.0f, 2.0f);
        gain += 20.0f * std::log10 (juce::jmax (0.001f, p.feedback));
        gain -= 0.6f * juce::jmax (0.0f, std::log2 (16000.0f / juce::jmax (500.0f, p.delayTone)));   // tlumeni vysek
    }
    g.setColour (ui::textDim);
    g.setFont (ui::font (10.0f, 500));
    if (p.pingPong)
    {
        g.drawText ("L", plot.withWidth (14.0f).withTrimmedBottom (plot.getHeight() * 0.5f).translated (plot.getWidth() - 14.0f, 0), juce::Justification::centredRight);
        g.drawText ("P", plot.withWidth (14.0f).withTrimmedTop (plot.getHeight() * 0.5f).translated (plot.getWidth() - 14.0f, 0), juce::Justification::centredRight);
    }
    g.drawText (fmtMs (total), plot.withTrimmedLeft (plot.getWidth() - 80.0f).withTrimmedTop (plot.getHeight() - 12.0f).translated (0, 4.0f),
                juce::Justification::bottomRight);
}

void EffectViz::paintReverb (juce::Graphics& g, juce::Rectangle<float> a, const dusi::Params& p)
{
    const float pre = p.preDelayMs, dec = p.decaySec * 1000.0f;
    header (g, a, U ("OBÁLKA DOZVUKU"), "pre-delay " + fmtMs (pre) + U (" · early ") + juce::String (juce::roundToInt (p.early * 100.0f)) + " %",
            ui::text2);
    {
        auto h = a.withHeight (18.0f);
        g.setFont (ui::font (11.0f, 600));
        g.setColour (accent);
        g.drawText (p.freeze ? U ("FREEZE – nekonečný dozvuk") : "DECAY " + juce::String (p.decaySec, 2) + U (" s (−60 dB)"), h, juce::Justification::centredRight);
    }
    auto plot = a.withTrimmedTop (24.0f);
    gridLines (g, plot, 10, 3);
    const float total = pre + (p.freeze ? 4000.0f : dec * (p.reverbType == dusi::ReverbType::prism ? 1.85f : 1.15f)) + 40.0f;
    auto xOf = [&] (float ms) { return plot.getX() + plot.getWidth() * ms / total; };
    auto yOf = [&] (float db) { return plot.getY() + plot.getHeight() * juce::jlimit (0.0f, 1.0f, -db / 66.0f); };

    // suchy zvuk
    g.setColour (ui::text2);
    g.fillRoundedRectangle (xOf (0.0f), yOf (0.0f), 3.0f, plot.getBottom() - yOf (0.0f), 1.5f);

    // early reflections: tecky po pre-delay
    if (p.early > 0.01f)
    {
        const float erDb = 20.0f * std::log10 (juce::jmax (0.01f, p.early)) - 4.0f;
        for (int i = 0; i < 9; ++i)
        {
            const float t = pre + 4.0f + 7.0f * (float) i * (0.8f + 0.5f * p.size) * (1.0f + 0.15f * (float) (i % 3));
            const float db = erDb - 2.2f * (float) i;
            const float x = xOf (t);
            g.setColour (ui::text2.withAlpha (0.7f));
            g.drawLine (x, yOf (db), x, plot.getBottom(), 1.5f);
        }
    }

    // dozvuk: od pre-delay linearne v dB az do -60 (freeze = rovne)
    const float startDb = -6.0f + 6.0f * p.reverbMix;
    const float x0 = xOf (pre + 10.0f);
    juce::Path tail;
    tail.startNewSubPath (x0, plot.getBottom());
    tail.lineTo (x0, yOf (startDb));
    if (p.freeze)
        tail.lineTo (plot.getRight(), yOf (startDb));
    else
    {
        tail.lineTo (xOf (pre + 10.0f + dec), yOf (startDb - 60.0f));
        tail.lineTo (xOf (pre + 10.0f + dec), plot.getBottom());
    }
    tail.lineTo (p.freeze ? plot.getRight() : xOf (pre + 10.0f + dec), plot.getBottom());
    tail.closeSubPath();
    g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.32f), x0, plot.getY(), accent.withAlpha (0.04f), plot.getRight(), plot.getY(), false));
    g.fillPath (tail);
    juce::Path edge;
    edge.startNewSubPath (x0, yOf (startDb));
    edge.lineTo (p.freeze ? plot.getRight() : xOf (pre + 10.0f + dec), p.freeze ? yOf (startDb) : yOf (startDb - 60.0f));
    g.setColour (accent);
    g.strokePath (edge, juce::PathStrokeType (2.0f));

    // PRISM: basy doznivaji kratce, vysky dlouho -> dve tenke cary
    if (p.reverbType == dusi::ReverbType::prism && ! p.freeze)
    {
        const std::pair<float, const char*> bands[] = { { 0.5f, "basy" }, { 1.7f, "výšky" } };
        for (auto [m, name] : bands)
        {
            const float end = pre + 10.0f + dec * m;
            g.setColour ((m < 1.0f ? ui::space : ui::filter).withAlpha (0.8f));
            g.drawLine (x0, yOf (startDb), xOf (juce::jmin (end, total)), yOf (startDb - 60.0f * juce::jmin (1.0f, (total - pre - 10.0f) / (dec * m))), 1.2f);
            g.setFont (ui::font (10.0f, 500));
            const float lx = xOf (juce::jmin (end, total)) - 40.0f;
            g.drawText (U (name), juce::Rectangle<float> (lx, yOf (startDb - 60.0f * juce::jmin (1.0f, (total - pre - 10.0f) / (dec * m))) - 14.0f, 40.0f, 12.0f),
                        juce::Justification::centredRight);
        }
    }

    // shimmer: jiskry nad ocasem
    if (p.shimmer > 0.02f)
    {
        g.setColour (accent.brighter (0.4f).withAlpha (0.25f + 0.5f * p.shimmer));
        for (int i = 0; i < 14; ++i)
        {
            const float t = pre + 30.0f + (p.freeze ? 3600.0f : dec * 0.8f) * (float) i / 14.0f;
            const float db = p.freeze ? startDb + 2.0f : startDb - 60.0f * (t - pre) / juce::jmax (1.0f, dec) + 4.0f;
            const float y = yOf (db) - 5.0f - (float) ((i * 7) % 5) * 2.0f;
            g.fillEllipse (juce::Rectangle<float> (3.0f, 3.0f).withCentre ({ xOf (t), y }));
        }
    }

    // -60 dB
    if (! p.freeze)
    {
        juce::Path dash;
        dash.startNewSubPath (xOf (pre + 10.0f + dec), plot.getY());
        dash.lineTo (xOf (pre + 10.0f + dec), plot.getBottom());
        const float d[] = { 3.0f, 3.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (dash, dash, d, 2);
        g.setColour (ui::textDim.withAlpha (0.7f));
        g.fillPath (dash);
    }
    g.setColour (ui::textDim);
    g.setFont (ui::font (10.0f, 500));
    g.drawText ("0 dB", juce::Rectangle<float> (plot.getRight() - 44.0f, yOf (0.0f), 40.0f, 12.0f), juce::Justification::centredRight);
    g.drawText (U ("−60 dB"), juce::Rectangle<float> (plot.getRight() - 54.0f, yOf (-60.0f) - 12.0f, 50.0f, 12.0f), juce::Justification::centredRight);
}

void EffectViz::paintFilter (juce::Graphics& g, juce::Rectangle<float> a, const dusi::Params& p)
{
    auto left = a.removeFromLeft (a.getWidth() * 0.55f);
    a.removeFromLeft (24.0f);
    const float beatsPerCycle = p.lfoHz > 0.0f ? (float) (p.bpm / 60.0) / p.lfoHz : 4.0f;
    header (g, left, U ("LFO · 2 TAKTY"), juce::String (p.lfoHz, 2) + " Hz" + (p.lfoSynced ? U (" · v taktu s Logicem") : juce::String()), accent);
    {
        auto h = left.withHeight (18.0f);
        g.setFont (ui::font (10.0f, 500));
        g.setColour (ui::textDim);
        g.drawText (U ("— L   - - P (posun ") + juce::String (juce::roundToInt (p.lfoStereo)) + U ("°)"), h, juce::Justification::centredRight);
    }
    auto plot = left.withTrimmedTop (24.0f);
    gridLines (g, plot, 8, 2);
    lfoArea = plot;
    const float cycles = 8.0f / juce::jmax (0.01f, beatsPerCycle);
    for (int ch = 0; ch < 2; ++ch)
    {
        juce::Path wave;
        const float off = ch == 1 ? p.lfoStereo / 360.0f : 0.0f;
        for (int i = 0; i <= 300; ++i)
        {
            const float t = (float) i / 300.0f;
            const float y = plot.getCentreY() - lfoShape (p.lfoShape, t * cycles + off) * plot.getHeight() * 0.42f;
            const float x = plot.getX() + plot.getWidth() * t;
            if (i == 0) wave.startNewSubPath (x, y); else wave.lineTo (x, y);
        }
        if (ch == 0)
        {
            g.setColour (accent);
            g.strokePath (wave, juce::PathStrokeType (2.0f));
        }
        else if (p.lfoStereo > 0.5f)
        {
            juce::Path dashed;
            const float d[] = { 5.0f, 4.0f };
            juce::PathStrokeType (1.5f).createDashedStroke (dashed, wave, d, 2);
            g.setColour (accent.withAlpha (0.6f));
            g.fillPath (dashed);
        }
    }

    // odezva filtru + rozsah LFO
    const float oct = p.lfoDepth * 4.0f;
    const float fLo = juce::jlimit (20.0f, 20000.0f, p.filterFreq * std::exp2 (-oct));
    const float fHi = juce::jlimit (20.0f, 20000.0f, p.filterFreq * std::exp2 (oct));
    header (g, a, U ("ODEZVA FILTRU"), U ("LFO rozsah ") + fmtHz (fLo) + U (" – ") + fmtHz (fHi), ui::text2);
    auto resp = a.withTrimmedTop (24.0f);
    gridLines (g, resp, 3, 2);
    auto xOf = [&] (float f) { return resp.getX() + resp.getWidth() * std::log10 (f / 20.0f) / 3.0f; };
    g.setColour (accent.withAlpha (0.10f));
    g.fillRect (juce::Rectangle<float> (xOf (fLo), resp.getY(), juce::jmax (1.0f, xOf (fHi) - xOf (fLo)), resp.getHeight()));

    const float k = 2.0f - 1.96f * juce::jlimit (0.0f, 1.0f, p.filterReso);
    auto mag = [&] (float f)
    {
        const float x = f / p.filterFreq, den = std::sqrt ((1.0f - x * x) * (1.0f - x * x) + k * k * x * x);
        const float h = p.filterType == dusi::FilterType::lowPass ? 1.0f / den
                      : p.filterType == dusi::FilterType::highPass ? x * x / den : k * x / den;
        return 20.0f * std::log10 (juce::jmax (1.0e-4f, h));
    };
    juce::Path curve, fill;
    for (int i = 0; i <= 200; ++i)
    {
        const float f = 20.0f * std::pow (1000.0f, (float) i / 200.0f);
        const float db = juce::jlimit (-36.0f, 18.0f, mag (f));
        const float y = resp.getY() + resp.getHeight() * (18.0f - db) / 54.0f;
        const float x = xOf (f);
        if (i == 0) { curve.startNewSubPath (x, y); fill.startNewSubPath (x, resp.getBottom()); }
        else curve.lineTo (x, y);
        fill.lineTo (x, y);
    }
    fill.lineTo (resp.getRight(), resp.getBottom());
    fill.closeSubPath();
    g.setColour (accent.withAlpha (0.10f));
    g.fillPath (fill);
    g.setColour (accent);
    g.strokePath (curve, juce::PathStrokeType (2.0f));
    g.setColour (ui::textDim);
    g.setFont (ui::font (10.0f, 500));
    const float labels[] = { 100.0f, 1000.0f, 10000.0f };
    for (auto f : labels)
        g.drawText (f >= 1000.0f ? juce::String ((int) (f / 1000.0f)) + "k" : juce::String ((int) f),
                    juce::Rectangle<float> (30.0f, 12.0f).withCentre ({ xOf (f), resp.getBottom() - 6.0f }), juce::Justification::centred);
}

void EffectViz::paintChorus (juce::Graphics& g, juce::Rectangle<float> a, const dusi::Params& p)
{
    header (g, a, U ("POHYB HLASŮ"), juce::String (p.chorusRate, 2) + U (" Hz · hloubka ") + juce::String (juce::roundToInt (p.chorusDepth * 100.0f))
                                         + U (" % · mix ") + juce::String (juce::roundToInt (p.chorusMix * 100.0f)) + " %", accent);
    {
        auto h = a.withHeight (18.0f);
        g.setFont (ui::font (10.0f, 500));
        g.setColour (ui::textDim);
        g.drawText (U ("— L   - - P   (4 s)"), h, juce::Justification::centredRight);
    }
    auto plot = a.withTrimmedTop (24.0f);
    gridLines (g, plot, 8, 2);
    const float seconds = 4.0f;
    for (int voice = 0; voice < 3; ++voice)
        for (int ch = 0; ch < 2; ++ch)
        {
            juce::Path wave;
            const float off = (float) voice / 3.0f + (ch == 1 ? 0.25f : 0.0f);
            for (int i = 0; i <= 240; ++i)
            {
                const float t = (float) i / 240.0f;
                const float v = std::sin (juce::MathConstants<float>::twoPi * (t * seconds * p.chorusRate + off));
                const float y = plot.getCentreY() - v * plot.getHeight() * 0.42f * juce::jmax (0.08f, p.chorusDepth);
                const float x = plot.getX() + plot.getWidth() * t;
                if (i == 0) wave.startNewSubPath (x, y); else wave.lineTo (x, y);
            }
            const float alpha = voice == 0 ? 1.0f : 0.45f;
            if (ch == 0)
            {
                g.setColour (accent.withAlpha (alpha));
                g.strokePath (wave, juce::PathStrokeType (voice == 0 ? 2.0f : 1.2f));
            }
            else
            {
                juce::Path dashed;
                const float d[] = { 5.0f, 4.0f };
                juce::PathStrokeType (1.2f).createDashedStroke (dashed, wave, d, 2);
                g.setColour (accent.withAlpha (alpha * 0.6f));
                g.fillPath (dashed);
            }
        }
}

void EffectViz::paintLofi (juce::Graphics& g, juce::Rectangle<float> a, const dusi::Params& p)
{
    header (g, a, U ("SIGNÁL"), juce::String (juce::roundToInt (p.lofiBits)) + U (" bitů · ") + fmtHz (p.lofiRate)
                                    + U (" · tón ") + fmtHz (p.lofiTone), accent);
    {
        auto h = a.withHeight (18.0f);
        g.setFont (ui::font (10.0f, 500));
        g.setColour (ui::textDim);
        g.drawText (U ("- - originál   — po lo-fi"), h, juce::Justification::centredRight);
    }
    auto plot = a.withTrimmedTop (24.0f);
    gridLines (g, plot, 8, 2);
    const float cycles = 3.0f;
    // originál
    juce::Path orig;
    for (int i = 0; i <= 300; ++i)
    {
        const float t = (float) i / 300.0f;
        const float v = std::sin (juce::MathConstants<float>::twoPi * t * cycles) * 0.8f + 0.15f * std::sin (juce::MathConstants<float>::twoPi * t * cycles * 5.0f);
        const float x = plot.getX() + plot.getWidth() * t, y = plot.getCentreY() - v * plot.getHeight() * 0.45f;
        if (i == 0) orig.startNewSubPath (x, y); else orig.lineTo (x, y);
    }
    juce::Path dashed;
    const float d[] = { 4.0f, 4.0f };
    juce::PathStrokeType (1.2f).createDashedStroke (dashed, orig, d, 2);
    g.setColour (ui::textDim.withAlpha (0.6f));
    g.fillPath (dashed);

    // po lo-fi: vzorkovani (sirka schodu) + bity (vyska schodu), zvetseno at je to videt
    const float steps = juce::jlimit (12.0f, 300.0f, p.lofiRate / 48000.0f * 300.0f);
    const float levels = std::exp2 (juce::jlimit (2.0f, 7.0f, (p.lofiBits - 4.0f) * 0.42f + 2.0f));
    juce::Path stair;
    for (int i = 0; i <= (int) steps; ++i)
    {
        const float t0 = (float) i / steps, t1 = juce::jmin (1.0f, (float) (i + 1) / steps);
        float v = std::sin (juce::MathConstants<float>::twoPi * t0 * cycles) * 0.8f + 0.15f * std::sin (juce::MathConstants<float>::twoPi * t0 * cycles * 5.0f);
        v = std::round (v * levels) / levels;
        const float y = plot.getCentreY() - v * plot.getHeight() * 0.45f;
        const float x0 = plot.getX() + plot.getWidth() * t0, x1 = plot.getX() + plot.getWidth() * t1;
        if (i == 0) stair.startNewSubPath (x0, y); else stair.lineTo (x0, y);
        stair.lineTo (x1, y);
    }
    g.setColour (accent);
    g.strokePath (stair, juce::PathStrokeType (1.8f));
}

//==============================================================================
void FreezeButton::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool on = getToggleState();
    const auto col = ui::space;
    g.setColour (on ? col.withAlpha (0.19f) : ui::inset);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (on ? col : highlighted ? ui::lineHover : ui::lineStrong);
    g.drawRoundedRectangle (r, 6.0f, on ? 1.5f : 1.0f);

    // snehova vlocka
    const auto c = juce::Point<float> (r.getX() + 28.0f, r.getCentreY());
    juce::Path flake;
    for (int i = 0; i < 3; ++i)
    {
        const float a = juce::MathConstants<float>::pi * (float) i / 3.0f;
        flake.startNewSubPath (c.x - 9.0f * std::sin (a), c.y - 9.0f * std::cos (a));
        flake.lineTo (c.x + 9.0f * std::sin (a), c.y + 9.0f * std::cos (a));
    }
    g.setColour (on ? col : ui::textDim);
    g.strokePath (flake, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    auto t = r.withTrimmedLeft (48.0f).withTrimmedRight (4.0f).reduced (0.0f, 10.0f);
    g.setColour (on ? ui::text : ui::text2);
    g.setFont (ui::font (15.0f, 700, 0.14f));
    g.drawText (on ? U ("FREEZE  ●") : "FREEZE", t.removeFromTop (20.0f), juce::Justification::centredLeft);
    g.setColour (ui::textDim);
    g.setFont (ui::font (10.5f, 500));
    g.drawFittedText (U ("podrží aktuální dozvuk donekonečna"), t.toNearestInt(), juce::Justification::centredLeft, 1, 0.85f);
}

//==============================================================================
void DuckMeter::setGainDb (float db, bool active)
{
    db = std::round (db * 10.0f) / 10.0f;
    if (std::abs (db - shownDb) < 0.05f && active == shownActive) return;
    shownDb = db;
    shownActive = active;
    repaint();
}

void DuckMeter::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour (ui::inset);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (ui::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    auto a = r.reduced (12.0f, 10.0f);
    g.setColour (ui::textDim);
    g.setFont (ui::font (11.0f, 500, 0.06f));
    g.drawText ("ZTLUMENO", a.removeFromTop (16.0f), juce::Justification::centredLeft);
    a.removeFromTop (4.0f);
    g.setColour (shownActive ? ui::text : ui::disabled);
    g.setFont (ui::font (20.0f, 600));
    g.drawText (shownActive ? (shownDb < -0.05f ? U ("−") : juce::String()) + juce::String (std::abs (shownDb), 1) + " dB" : U ("vypnuto"),
                a.removeFromTop (26.0f), juce::Justification::centredLeft);
    a.removeFromTop (8.0f);
    const auto bar = a.removeFromTop (6.0f);
    g.setColour (ui::knobTrack);
    g.fillRoundedRectangle (bar, 3.0f);
    const float frac = juce::jlimit (0.0f, 1.0f, -shownDb / 24.0f);
    if (shownActive && frac > 0.0f)
    {
        g.setColour (ui::dynamics);
        g.fillRoundedRectangle (bar.withTrimmedLeft (bar.getWidth() * (1.0f - frac)), 3.0f);
    }
}
