#include "MainPage.h"

using ui::U;

MainPage::MainPage (DusiVerbProcessor& p)
    : proc (p), space (p), eqView (p),
      pan      (*this, p.apvts, "pan",      "PAN",       ui::space,  Knob::Size::md),
      width    (*this, p.apvts, "width",    U ("ŠÍŘKA"), ui::space,  Knob::Size::md),
      monoBass (*this, p.apvts, "monoBass", "BASS MONO", ui::space,  Knob::Size::md),
      outGain  (*this, p.apvts, "outGain",  "OUTPUT",    ui::output, Knob::Size::md),
      outMode (U ("VÝSTUP")), decayMode ("DECAY"),
      eqPower (ui::eq), decayPower (ui::verb)
{
    setOpaque (true);
    addAndMakeVisible (space);
    addAndMakeVisible (eqView);

    safeButton.setClickingTogglesState (true);
    safeButton.setColour (juce::TextButton::buttonOnColourId, ui::good);
    safeButton.setTooltip (U ("Hlídá stereo: omezí šířku podle prostoru a při protifázi efekt zúží"));
    addAndMakeVisible (safeButton);
    safeAtt = std::make_unique<APVTS::ButtonAttachment> (p.apvts, "safe", safeButton);

    for (auto* b : { &outMode, &decayMode })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (101);
        addAndMakeVisible (b);
    }
    outMode.setColour (juce::TextButton::buttonOnColourId, ui::eq);
    decayMode.setColour (juce::TextButton::buttonOnColourId, ui::verb);
    outMode.setTooltip (U ("Výstupní EQ – barva celého zvuku (8 pásem)"));
    decayMode.setTooltip (U ("Decay EQ – jak dlouho dozvuk zní v basech, středech a výškách (jako Pro-R)"));
    outMode.setToggleState (true, juce::dontSendNotification);
    outMode.onClick   = [this] { setEqMode (EqView::Mode::output); };
    decayMode.onClick = [this] { setEqMode (EqView::Mode::decay); };

    eqPower.setTooltip (U ("Zapnout / vypnout výstupní EQ"));
    decayPower.setTooltip (U ("Zapnout / vypnout decay EQ"));
    addAndMakeVisible (eqPower);
    addAndMakeVisible (decayPower);
    eqPowerAtt    = std::make_unique<APVTS::ButtonAttachment> (p.apvts, "eqOn", eqPower);
    decayPowerAtt = std::make_unique<APVTS::ButtonAttachment> (p.apvts, "decayEqOn", decayPower);

    startTimerHz (15);
}

void MainPage::setEqMode (EqView::Mode m)
{
    eqView.setMode (m);
    repaint (eqPanel);
}

int MainPage::tempoHighlightKey() const
{
    // ktera bunka tabulky odpovida aktualnimu delayi (radek*3 + sloupec), -1 = zadna
    auto v = [this] (const char* id) { return proc.apvts.getRawParameterValue (id)->load(); };
    if (v ("delayOn") < 0.5f || v ("delaySync") < 0.5f) return -1;
    const auto name = DusiVerbProcessor::divisionNames()[(int) v ("delayDiv")];
    const juce::StringArray rows { "1/4", "1/8", "1/16", "1/32" };
    const int col = name.endsWith (".") ? 1 : name.endsWith ("T") ? 2 : 0;
    const int row = rows.indexOf (name.trimCharactersAtEnd (".T"));
    return row < 0 ? -1 : row * 3 + col;
}

void MainPage::timerCallback()
{
    if (! isShowing()) return;
    const float corr = proc.uiCorrelation.load(), ag = proc.uiAutoGain.load();
    if (std::abs (corr - shownCorrelation) > 0.005f || std::abs (ag - shownAutoGain) > 0.005f)
    {
        shownCorrelation = corr;
        shownAutoGain = ag;
        repaint (meterArea);
    }
    const double bpm = proc.uiBpm.load();
    const int tk = tempoHighlightKey();
    if (std::abs (bpm - shownBpm) > 0.01 || tk != shownTempoKey)
    {
        shownBpm = bpm;
        shownTempoKey = tk;
        repaint (tempoPanel);
    }
}

void MainPage::paint (juce::Graphics& g)
{
    g.fillAll (ui::background);
    ui::paintPanel (g, stereoPanel, "STEREO", ui::space, ui::smallTitle);
    ui::paintPanel (g, tempoPanel, "TEMPO", ui::text, 32);
    ui::paintPanel (g, eqPanel, "EQ", ui::eq, 44);

    g.setColour (ui::textDim);
    g.setFont (ui::font (11.0f, 500));
    const juce::String hint = eqView.getMode() == EqView::Mode::output
        ? U ("dvojklik = nové pásmo · táhni bod · kolečko = Q · pravé tlačítko = typ · dvojklik na bod = smazat")
        : U ("bod nahoru = delší dozvuk v pásmu · kolečko = šířka · dvojklik = ×1");
    g.drawText (hint, eqPanel.withHeight (44).reduced (12, 0).withTrimmedLeft (300), juce::Justification::centredRight);

    paintCorrelation (g);
    paintTempo (g);
}

void MainPage::paintCorrelation (juce::Graphics& g)
{
    const float corr = juce::jlimit (-1.0f, 1.0f, shownCorrelation);
    const auto col = corr >= 0.3f ? ui::good : (corr >= 0.0f ? ui::mid : ui::bad);
    const auto r = meterArea;

    g.setColour (ui::textDim);
    g.setFont (ui::font (11.0f, 500, 0.03f));
    g.drawText ("KORELACE", r.getX(), r.getY(), 72, 18, juce::Justification::centredLeft);

    const auto track = juce::Rectangle<float> ((float) r.getX() + 76.0f, (float) r.getY() + 4.0f, 176.0f, 10.0f);
    const auto in = track.reduced (1.0f);
    const float W = in.getWidth();
    g.setColour (ui::inset);
    g.fillRoundedRectangle (track, 3.0f);
    g.setColour (ui::bad.withAlpha (0.16f));  g.fillRect (in.withWidth (W * 0.5f));
    g.setColour (ui::mid.withAlpha (0.16f));  g.fillRect (in.withX (in.getX() + W * 0.5f).withWidth (W * 0.15f));
    g.setColour (ui::good.withAlpha (0.16f)); g.fillRect (in.withX (in.getX() + W * 0.65f).withWidth (W * 0.35f));
    g.setColour (juce::Colour (0xff1f2a40));
    g.drawRoundedRectangle (track.reduced (0.5f), 3.0f, 1.0f);
    const float zero = in.getX() + W * 0.5f, pos = in.getX() + (corr + 1.0f) * 0.5f * W;
    g.setColour (col);
    g.fillRect (juce::Rectangle<float> (juce::jmin (zero, pos), in.getY() + 2.0f, std::abs (pos - zero), 4.0f));
    g.setColour (juce::Colour (0xff3d4f73));
    g.fillRect (zero - 0.5f, track.getY() - 3.0f, 1.0f, 14.0f);
    g.setColour (ui::text);
    g.fillRoundedRectangle (pos - 1.0f, track.getY() - 3.0f, 2.0f, 16.0f, 1.0f);

    g.setColour (col);
    g.setFont (ui::font (12.0f, 600));
    g.drawText ((corr >= 0 ? "+" : U ("−")) + juce::String (std::abs (corr), 2),
                juce::Rectangle<int> ((int) track.getRight() + 8, r.getY(), 44, 18), juce::Justification::centredRight);

    g.setColour (ui::textDim);
    g.setFont (ui::font (10.0f, 500));
    const auto scale = juce::Rectangle<int> ((int) track.getX(), r.getY() + 20, (int) track.getWidth(), 14);
    g.drawText (U ("−1"), scale, juce::Justification::centredLeft);
    g.drawText ("0", scale, juce::Justification::centred);
    g.drawText ("+1", scale, juce::Justification::centredRight);

    juce::String status;
    juce::Colour statusCol = col;
    const bool safe = proc.apvts.getRawParameterValue ("safe")->load() > 0.5f;
    if (safe && shownAutoGain < 0.97f)
    {
        status = U ("Auto-safe zužuje efekt o ") + juce::String (juce::roundToInt ((1.0f - shownAutoGain) * 100.0f)) + " %";
        statusCol = ui::mid;
    }
    else if (corr < 0.0f)  status = U ("Chyba – fáze se v monu ruší");
    else if (corr < 0.3f)  status = U ("Pozor – v monu slábne");
    else                   status = U ("OK – drží v monu");
    g.setColour (statusCol);
    g.setFont (ui::font (11.0f, 500));
    g.drawText (status, r.getX(), r.getY() + 40, r.getWidth(), 16, juce::Justification::centredLeft);
}

void MainPage::paintTempo (juce::Graphics& g)
{
    const double bpm = shownBpm > 1.0 ? shownBpm : 120.0;
    const double beatMs = 60000.0 / bpm;

    // titulek vpravo: tempo a delka taktu
    {
        auto bar = tempoPanel.withHeight (32).reduced (12, 0);
        const auto second = U (" · takt ") + juce::String (beatMs * 4.0, 0) + " ms";
        const auto f2 = ui::font (12.0f, 500);
        const int w2 = juce::GlyphArrangement::getStringWidthInt (f2, second) + 2;
        g.setFont (f2);
        g.setColour (ui::textDim);
        g.drawText (second, bar.removeFromRight (w2), juce::Justification::centredRight);
        g.setFont (ui::font (12.0f, 600));
        g.setColour (ui::text);
        g.drawText (juce::String (bpm, 1) + " BPM", bar, juce::Justification::centredRight);
    }

    auto area = tempoArea;
    const int firstW = 44;
    const int colW = (area.getWidth() - firstW) / 3;
    {
        auto h = area.removeFromTop (17);
        g.setColour (ui::textDim);
        g.setFont (ui::font (10.0f, 600, 0.06f));
        g.drawText ("NOTA", h.removeFromLeft (firstW), juce::Justification::centredLeft);
        for (const auto& t : juce::StringArray { U ("ROVNÁ"), U ("TEČKOVANÁ"), U ("TRIOLA") })
            g.drawText (t, h.removeFromLeft (colW), juce::Justification::centredRight);
    }
    const std::pair<const char*, double> notes[] = { { "1/4", 1.0 }, { "1/8", 0.5 }, { "1/16", 0.25 }, { "1/32", 0.125 } };
    int rowIndex = 0;
    for (const auto& [name, beats] : notes)
    {
        area.removeFromTop (2);
        auto r = area.removeFromTop (17);
        g.setColour (ui::textDim);
        g.setFont (ui::font (12.0f, 500));
        g.drawText (name, r.removeFromLeft (firstW), juce::Justification::centredLeft);
        const double ms = beatMs * beats;
        const double vals[] = { ms, ms * 1.5, ms * 2.0 / 3.0 };
        for (int c = 0; c < 3; ++c)
        {
            const bool hot = shownTempoKey == rowIndex * 3 + c;
            g.setColour (hot ? ui::delay : ui::text);
            g.setFont (ui::font (12.0f, hot ? 600 : 500));
            g.drawText ((hot ? U ("● ") : juce::String()) + juce::String (vals[c], 1) + " ms",
                        r.removeFromLeft (colW), juce::Justification::centredRight);
        }
        ++rowIndex;
    }
}

void MainPage::resized()
{
    space.setBounds (12, 8, 600, 344);
    stereoPanel = { 620, 8, 328, 208 };
    tempoPanel  = { 620, 224, 328, 128 };
    eqPanel     = { 12, 360, 936, 300 };
    tempoArea = tempoPanel.withTrimmedTop (32).reduced (12, 0).withTrimmedBottom (8);

    safeButton.setBounds (stereoPanel.getRight() - 10 - 52, stereoPanel.getY() + 6, 52, 22);
    {
        const int x0 = stereoPanel.getX() + 10, w = stereoPanel.getWidth() - 20;
        const int step = (w - Knob::cellWidth (Knob::Size::md)) / 3;
        int i = 0;
        for (auto* k : { &pan, &width, &monoBass, &outGain })
            k->setTopLeft (x0 + step * i++, stereoPanel.getY() + ui::smallTitle);
        meterArea = { x0, stereoPanel.getY() + ui::smallTitle + 98 + 8, 302, 58 };
    }

    const int by = eqPanel.getY() + 8;
    int x = eqPanel.getX() + 12 + 28 + 12;
    outMode.setBounds (x, by, 78, 28);
    eqPower.setBounds (outMode.getRight() - 1, by, 28, 28);
    x = eqPower.getRight() + 8;
    decayMode.setBounds (x, by, 70, 28);
    decayPower.setBounds (decayMode.getRight() - 1, by, 28, 28);
    eqView.setBounds (eqPanel.getX() + 12, eqPanel.getY() + 44, eqPanel.getWidth() - 24, eqPanel.getHeight() - 44 - 12);
}
