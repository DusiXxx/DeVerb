#include "ChainStrip.h"

using ui::U;

ChainStrip::ChainStrip (DusiVerbProcessor& p) : processor (p)
{
    startTimerHz (8);
}

dusi::ChainOrder ChainStrip::currentOrder() const
{
    const auto& orders = dusi::chainOrders();
    const int idx = juce::jlimit (0, (int) orders.size() - 1, (int) processor.apvts.getRawParameterValue ("chainOrder")->load());
    return orders[(size_t) idx];
}

juce::String ChainStrip::moduleName (dusi::Module m)
{
    switch (m)
    {
        case dusi::Module::delay:  return "DELAY";
        case dusi::Module::reverb: return "REVERB";
        case dusi::Module::filter: return "FILTR";
        case dusi::Module::chorus: return "CHORUS";
        case dusi::Module::lofi:   return "LO-FI";
    }
    return {};
}

juce::Colour ChainStrip::moduleColour (dusi::Module m)
{
    switch (m)
    {
        case dusi::Module::delay:  return ui::delay;
        case dusi::Module::reverb: return ui::verb;
        case dusi::Module::filter: return ui::filter;
        case dusi::Module::chorus: return ui::chorus;
        case dusi::Module::lofi:   return ui::lofi;
    }
    return ui::text;
}

const char* ChainStrip::onId (dusi::Module m)
{
    switch (m)
    {
        case dusi::Module::delay:  return "delayOn";
        case dusi::Module::reverb: return "reverbOn";
        case dusi::Module::filter: return "filterOn";
        case dusi::Module::chorus: return "chorusOn";
        case dusi::Module::lofi:   return "lofiOn";
    }
    return "delayOn";
}

const char* ChainStrip::mixId (dusi::Module m)
{
    switch (m)
    {
        case dusi::Module::delay:  return "delayMix";
        case dusi::Module::reverb: return "reverbMix";
        case dusi::Module::filter: return "filterMix";
        case dusi::Module::chorus: return "chorusMix";
        case dusi::Module::lofi:   return "lofiMix";
    }
    return "delayMix";
}

// prekreslit jen kdyz se zmeni poradi, vypinace nebo mix
std::vector<float> ChainStrip::stateKey() const
{
    std::vector<float> k { processor.apvts.getRawParameterValue ("chainOrder")->load(), (float) selected };
    for (auto m : currentOrder())
    {
        k.push_back (processor.apvts.getRawParameterValue (onId (m))->load());
        k.push_back (std::round (processor.apvts.getRawParameterValue (mixId (m))->load()));
    }
    return k;
}

void ChainStrip::timerCallback()
{
    if (! isShowing()) return;
    auto k = stateKey();
    if (k != lastKey) { lastKey = std::move (k); repaint(); }
}

juce::Rectangle<float> ChainStrip::slotBounds (int slot) const
{
    const auto area = juce::Rectangle<float> (11.0f, 8.0f, (float) getWidth() - 22.0f, 44.0f);
    const float small = 72.0f, gap = 22.0f;
    const float big = (area.getWidth() - 2.0f * small - 6.0f * gap) / (float) dusi::kNumModules;
    float x = area.getX();
    for (int i = 0; i < slot; ++i) x += ((i == 0) ? small : big) + gap;
    const float w = (slot == 0 || slot == dusi::kNumModules + 1) ? small : big;
    return { x, area.getY(), w, area.getHeight() };
}

juce::Rectangle<float> ChainStrip::powerDot (juce::Rectangle<float> block) const
{
    return juce::Rectangle<float> (16.0f, 16.0f).withCentre ({ block.getRight() - 16.0f, block.getCentreY() });
}

void ChainStrip::paint (juce::Graphics& g)
{
    ui::paintPanel (g, getLocalBounds(), {}, ui::text);

    // sipky mezi bloky
    for (int i = 0; i <= dusi::kNumModules; ++i)
    {
        const auto a = slotBounds (i), b = slotBounds (i + 1);
        const float y = a.getCentreY(), x0 = a.getRight() + 5.0f, x1 = b.getX() - 5.0f;
        juce::Path p;
        p.startNewSubPath (x0, y); p.lineTo (x1, y);
        p.startNewSubPath (x1 - 4.0f, y - 3.5f); p.lineTo (x1, y); p.lineTo (x1 - 4.0f, y + 3.5f);
        g.setColour (ui::lineHover);
        g.strokePath (p, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    auto fixedBlock = [&g] (juce::Rectangle<float> r, const juce::String& text, juce::Colour c)
    {
        r = r.withSizeKeepingCentre (r.getWidth(), 40.0f);
        g.setColour (ui::inset);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (c.withAlpha (0.5f));
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
        g.setColour (c);
        g.setFont (ui::font (11.0f, 600, 0.06f));
        g.drawText (text, r, juce::Justification::centred);
    };
    fixedBlock (slotBounds (0), "POZICE", ui::space);
    fixedBlock (slotBounds (dusi::kNumModules + 1), "EQ", ui::eq);

    g.setColour (ui::textDim);
    g.setFont (ui::font (10.0f, 500));
    g.drawText (U ("klik na blok = nastavení · táhni = pořadí · tečka = zapnout / vypnout"),
                juce::Rectangle<int> (12, 54, getWidth() - 24, 16), juce::Justification::centred);

    const auto order = currentOrder();
    auto drawModule = [&] (dusi::Module m, juce::Rectangle<float> r, bool lifted)
    {
        const bool on = processor.apvts.getRawParameterValue (onId (m))->load() > 0.5f;
        const bool sel = m == selected;
        const auto col = moduleColour (m);
        if (! sel) r = r.withSizeKeepingCentre (r.getWidth(), 40.0f);
        if (lifted)
        {
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.fillRoundedRectangle (r.translated (0, 3.0f), 6.0f);
        }
        g.setColour (sel ? col.withAlpha (0.19f) : on ? col.withAlpha (0.08f) : juce::Colour (0xff0c1220));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (sel ? col : on ? col.withAlpha (0.5f) : juce::Colour (0xff26314a));
        g.drawRoundedRectangle (r.reduced (sel || lifted ? 1.0f : 0.5f), 6.0f, sel || lifted ? 2.0f : 1.0f);

        // uchyt pro tazeni
        g.setColour (on ? col.withAlpha (0.69f) : juce::Colour (0xff3a4661));
        for (int k = 0; k < 3; ++k)
            for (int c = 0; c < 2; ++c)
                g.fillEllipse (juce::Rectangle<float> (2.5f, 2.5f).withCentre ({ r.getX() + 10.0f + 4.0f * (float) c,
                                                                                  r.getCentreY() - 5.0f + 5.0f * (float) k }));

        const auto textArea = r.withTrimmedLeft (24.0f).withTrimmedRight (28.0f);
        g.setColour (on || sel ? ui::text : ui::textDim);
        g.setFont (ui::font (12.0f, 600, 0.06f));
        g.drawText (moduleName (m), textArea.withTrimmedBottom (r.getHeight() * 0.42f), juce::Justification::bottomLeft);
        g.setColour (sel ? ui::text : ui::textDim);
        g.setFont (ui::font (11.0f, 500));
        const int mix = juce::roundToInt (processor.apvts.getRawParameterValue (mixId (m))->load());
        g.drawText (on ? "Mix " + juce::String (mix) + " %" : U ("vypnuto"),
                    textArea.withTrimmedTop (r.getHeight() * 0.56f), juce::Justification::topLeft);

        const auto dot = powerDot (r);
        g.setColour (on ? col : juce::Colour (0xff3a4661));
        g.drawEllipse (dot.reduced (0.75f), 1.5f);
        if (on) g.fillEllipse (dot.reduced (4.0f));
    };

    for (int s = 0; s < dusi::kNumModules; ++s)
        if (s != dragSlot)
            drawModule (order[(size_t) s], slotBounds (s + 1), false);
    if (dragSlot >= 0)
    {
        auto r = slotBounds (dragSlot + 1);
        r.setX (dragX - dragOffset);
        drawModule (order[(size_t) dragSlot], r, true);
    }
}

void ChainStrip::mouseMove (const juce::MouseEvent& e)
{
    for (int s = 0; s < dusi::kNumModules; ++s)
        if (slotBounds (s + 1).contains (e.position))
        {
            setMouseCursor (powerDot (slotBounds (s + 1)).expanded (4.0f).contains (e.position)
                                ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::DraggingHandCursor);
            return;
        }
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

void ChainStrip::mouseDown (const juce::MouseEvent& e)
{
    dragSlot = -1;
    moved = false;
    const auto order = currentOrder();
    for (int s = 0; s < dusi::kNumModules; ++s)
    {
        const auto r = slotBounds (s + 1);
        if (! r.contains (e.position)) continue;
        if (powerDot (r).expanded (4.0f).contains (e.position))
        {
            if (auto* p = processor.apvts.getParameter (onId (order[(size_t) s])))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);
                p->endChangeGesture();
            }
            repaint();
            return;
        }
        dragSlot = s;
        dragOffset = e.position.x - r.getX();
        dragX = e.position.x;
        return;
    }
}

void ChainStrip::mouseDrag (const juce::MouseEvent& e)
{
    if (dragSlot < 0) return;
    moved = moved || e.getDistanceFromDragStart() > 4;
    dragX = e.position.x;
    if (moved) repaint();
}

void ChainStrip::mouseUp (const juce::MouseEvent&)
{
    if (dragSlot < 0) return;
    auto order = currentOrder();
    if (! moved)   // obycejny klik = otevrit nastaveni efektu
    {
        selected = order[(size_t) dragSlot];
        if (onSelect) onSelect (selected);
    }
    else
    {
        const float centre = dragX - dragOffset + slotBounds (1).getWidth() * 0.5f;
        int target = 0;
        float best = 1.0e9f;
        for (int s = 0; s < dusi::kNumModules; ++s)
        {
            const float d = std::abs (slotBounds (s + 1).getCentreX() - centre);
            if (d < best) { best = d; target = s; }
        }
        const auto m = order[(size_t) dragSlot];
        std::vector<dusi::Module> rest;
        for (int s = 0; s < dusi::kNumModules; ++s) if (s != dragSlot) rest.push_back (order[(size_t) s]);
        rest.insert (rest.begin() + target, m);
        dusi::ChainOrder result {};
        for (int s = 0; s < dusi::kNumModules; ++s) result[(size_t) s] = rest[(size_t) s];
        setOrder (result);
    }
    dragSlot = -1;
    repaint();
}

void ChainStrip::setOrder (const dusi::ChainOrder& order)
{
    const auto& orders = dusi::chainOrders();
    for (int i = 0; i < (int) orders.size(); ++i)
        if (orders[(size_t) i] == order)
            if (auto* p = processor.apvts.getParameter ("chainOrder"))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (p->convertTo0to1 ((float) i));
                p->endChangeGesture();
            }
}
