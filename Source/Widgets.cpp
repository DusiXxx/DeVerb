#include "Widgets.h"
#include "BinaryData.h"

//==============================================================================
namespace
{
juce::Typeface::Ptr typeface (int weight)
{
    static juce::Typeface::Ptr medium   = juce::Typeface::createSystemTypefaceFor (BinaryData::InterMedium_ttf,   BinaryData::InterMedium_ttfSize);
    static juce::Typeface::Ptr semibold = juce::Typeface::createSystemTypefaceFor (BinaryData::InterSemiBold_ttf, BinaryData::InterSemiBold_ttfSize);
    static juce::Typeface::Ptr bold     = juce::Typeface::createSystemTypefaceFor (BinaryData::InterBold_ttf,     BinaryData::InterBold_ttfSize);
    return weight >= 700 ? bold : weight >= 600 ? semibold : medium;
}
}

juce::Font ui::font (float px, int weight, float tracking)
{
    // CSS "font-size" = vyska em -> v JUCE "point height"
    return juce::Font (juce::FontOptions (typeface (weight)).withPointHeight (px).withKerningFactor (tracking));
}

void ui::paintPanel (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, juce::Colour accent,
                     int titleH, const juce::String& subtitle)
{
    const auto rf = r.toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (rf, 8.0f);
    g.setColour (line);
    g.drawRoundedRectangle (rf.reduced (0.5f), 8.0f, 1.0f);
    if (title.isEmpty()) return;

    auto bar = r.withHeight (titleH).withTrimmedLeft (12);
    const auto f = font (12.0f, 600, 0.08f);
    g.setFont (f);
    g.setColour (accent);
    g.drawText (title, bar, juce::Justification::centredLeft);
    if (subtitle.isNotEmpty())
    {
        const int w = juce::GlyphArrangement::getStringWidthInt (f, title);
        g.setColour (textDim);
        g.setFont (font (11.0f, 500));
        g.drawText (subtitle, bar.withTrimmedLeft (w + 10).withTrimmedRight (12), juce::Justification::centredLeft);
    }
}

void ui::paintChip (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, juce::Colour col)
{
    g.setColour (col.withAlpha (0.14f));
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (col.withAlpha (0.7f));
    g.drawRoundedRectangle (r.reduced (0.5f), r.getHeight() * 0.5f, 1.0f);
    g.setColour (col);
    g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ r.getX() + 12.0f, r.getCentreY() }));
    g.setFont (font (11.0f, 600));
    g.drawText (t, r.withTrimmedLeft (21.0f).withTrimmedRight (10.0f), juce::Justification::centredLeft);
}

void ui::drawGlyph (juce::Graphics& g, Glyph glyph, juce::Rectangle<float> box, juce::Colour col, float stroke)
{
    // ikony jsou navrzene v mrizce 16x16
    const float s = juce::jmin (box.getWidth(), box.getHeight()) / 16.0f;
    const auto o = box.getCentre() - juce::Point<float> (8.0f * s, 8.0f * s);
    auto P = [&] (float x, float y) { return juce::Point<float> (o.x + x * s, o.y + y * s); };
    juce::Path p;
    bool fill = false;

    switch (glyph)
    {
        case Glyph::undo:
        case Glyph::redo:
        {
            const bool u = glyph == Glyph::undo;
            auto X = [&] (float x) { return u ? x : 16.0f - x; };
            p.startNewSubPath (P (X (3.5f), 6.5f));
            p.lineTo (P (X (10.0f), 6.5f));
            p.cubicTo (P (X (12.5f), 6.5f), P (X (13.5f), 8.5f), P (X (13.5f), 10.0f));
            p.cubicTo (P (X (13.5f), 11.8f), P (X (12.2f), 13.0f), P (X (10.0f), 13.0f));
            p.lineTo (P (X (7.0f), 13.0f));
            p.startNewSubPath (P (X (6.5f), 3.5f));
            p.lineTo (P (X (3.5f), 6.5f));
            p.lineTo (P (X (6.5f), 9.5f));
            break;
        }
        case Glyph::leaf:
            p.startNewSubPath (P (3.0f, 13.0f));
            p.cubicTo (P (3.0f, 6.0f), P (7.0f, 3.0f), P (13.5f, 2.5f));
            p.cubicTo (P (13.5f, 9.0f), P (10.0f, 13.0f), P (3.0f, 13.0f));
            p.startNewSubPath (P (3.0f, 13.0f));
            p.lineTo (P (9.0f, 7.0f));
            break;
        case Glyph::dice:
        {
            p.addRoundedRectangle (juce::Rectangle<float> (P (2.5f, 2.5f), P (13.5f, 13.5f)), 2.5f * s);
            juce::PathStrokeType (stroke).createStrokedPath (p, p);
            for (auto [x, y] : { std::pair { 5.5f, 5.5f }, { 10.5f, 10.5f }, { 8.0f, 8.0f }, { 10.5f, 5.5f }, { 5.5f, 10.5f } })
                p.addEllipse (juce::Rectangle<float> (2.0f * s, 2.0f * s).withCentre (P (x, y)));
            fill = true;
            break;
        }
        case Glyph::chevronDown:  p.startNewSubPath (P (4.5f, 6.5f)); p.lineTo (P (8.0f, 10.0f)); p.lineTo (P (11.5f, 6.5f)); break;
        case Glyph::chevronLeft:  p.startNewSubPath (P (9.5f, 4.5f)); p.lineTo (P (6.0f, 8.0f));  p.lineTo (P (9.5f, 11.5f)); break;
        case Glyph::chevronRight: p.startNewSubPath (P (6.5f, 4.5f)); p.lineTo (P (10.0f, 8.0f)); p.lineTo (P (6.5f, 11.5f)); break;
        case Glyph::download:
            p.startNewSubPath (P (8.0f, 2.5f)); p.lineTo (P (8.0f, 10.0f));
            p.startNewSubPath (P (4.8f, 7.0f)); p.lineTo (P (8.0f, 10.2f)); p.lineTo (P (11.2f, 7.0f));
            p.startNewSubPath (P (3.0f, 13.0f)); p.lineTo (P (13.0f, 13.0f));
            break;
        case Glyph::check:   p.startNewSubPath (P (3.5f, 8.5f)); p.lineTo (P (6.5f, 11.5f)); p.lineTo (P (12.5f, 4.5f)); break;
        case Glyph::refresh:
            p.addCentredArc (o.x + 8.0f * s, o.y + 8.0f * s, 5.0f * s, 5.0f * s, 0.0f, 0.6f, juce::MathConstants<float>::twoPi - 0.2f, true);
            p.startNewSubPath (P (12.8f, 2.8f)); p.lineTo (P (12.6f, 6.2f)); p.lineTo (P (9.3f, 5.6f));
            break;
        case Glyph::star:
            for (int i = 0; i < 10; ++i)
            {
                const float a = juce::MathConstants<float>::pi * (float) i / 5.0f;
                const float r = (i % 2 == 0) ? 6.0f : 2.6f;
                const auto pt = P (8.0f + r * std::sin (a), 8.5f - r * std::cos (a));
                if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
            }
            p.closeSubPath();
            break;
        case Glyph::folder:
            p.startNewSubPath (P (2.5f, 4.0f)); p.lineTo (P (6.5f, 4.0f)); p.lineTo (P (8.0f, 5.5f)); p.lineTo (P (13.5f, 5.5f));
            p.lineTo (P (13.5f, 12.5f)); p.lineTo (P (2.5f, 12.5f)); p.closeSubPath();
            break;
        case Glyph::search:
            p.addEllipse (juce::Rectangle<float> (P (2.5f, 2.5f), P (11.0f, 11.0f)));
            p.startNewSubPath (P (10.0f, 10.0f)); p.lineTo (P (13.5f, 13.5f));
            break;
        case Glyph::close:
            p.startNewSubPath (P (4.0f, 4.0f)); p.lineTo (P (12.0f, 12.0f));
            p.startNewSubPath (P (12.0f, 4.0f)); p.lineTo (P (4.0f, 12.0f));
            break;
    }

    g.setColour (col);
    if (fill) g.fillPath (p);
    else      g.strokePath (p, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
DusiLook::DusiLook()
{
    using namespace ui;
    setColour (juce::ResizableWindow::backgroundColourId, background);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, space.withAlpha (0.3f));
    setColour (juce::Label::textColourId, textDim);
    setColour (juce::Label::textWhenEditingColourId, text);
    setColour (juce::Label::outlineWhenEditingColourId, lineStrong);
    setColour (juce::Label::backgroundWhenEditingColourId, inset);
    setColour (juce::ToggleButton::textColourId, text2);
    setColour (juce::ToggleButton::tickColourId, space);
    setColour (juce::ToggleButton::tickDisabledColourId, disabled);
    setColour (juce::ComboBox::backgroundColourId, inset);
    setColour (juce::ComboBox::outlineColourId, lineStrong);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::arrowColourId, textDim);
    setColour (juce::ComboBox::focusedOutlineColourId, lineHover);
    setColour (juce::PopupMenu::backgroundColourId, raised);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::headerTextColourId, textDim);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, space.withAlpha (0.19f));
    setColour (juce::PopupMenu::highlightedTextColourId, text);
    setColour (juce::TextButton::buttonColourId, panel);
    setColour (juce::TextButton::buttonOnColourId, space);
    setColour (juce::TextButton::textColourOffId, text2);
    setColour (juce::TextButton::textColourOnId, text);
    setColour (juce::TextEditor::backgroundColourId, inset);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::outlineColourId, lineStrong);
    setColour (juce::TextEditor::focusedOutlineColourId, space);
    setColour (juce::TextEditor::highlightColourId, space.withAlpha (0.3f));
    setColour (juce::CaretComponent::caretColourId, space);
    setColour (juce::ListBox::backgroundColourId, panel);
    setColour (juce::AlertWindow::backgroundColourId, raised);
    setColour (juce::AlertWindow::textColourId, text);
    setColour (juce::AlertWindow::outlineColourId, lineStrong);
    setColour (juce::ScrollBar::thumbColourId, lineStrong);
    setColour (juce::TooltipWindow::backgroundColourId, raised);
    setColour (juce::TooltipWindow::textColourId, text2);
    setColour (juce::TooltipWindow::outlineColourId, lineStrong);
}

juce::Typeface::Ptr DusiLook::getTypefaceForFont (const juce::Font& f)
{
    // vsechno pismo bez vlastniho typefacu = Inter (tucne -> Bold)
    if (f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName())
        return typeface (f.isBold() ? 700 : 500);
    return LookAndFeel_V4::getTypefaceForFont (f);
}

void DusiLook::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                 float sliderPos, float startAngle, float endAngle, juce::Slider& slider)
{
    const float S = (float) juce::jmin (width, height);
    const float r = S >= 70.0f ? 22.0f : S >= 60.0f ? 18.0f : S >= 50.0f ? 13.0f : S * 0.25f;
    const float ti = S >= 60.0f ? 8.0f : 7.0f;
    const juce::Point<float> c { (float) x + (float) width * 0.5f, (float) y + (float) height * 0.5f };
    const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
    const bool off   = (bool) slider.getProperties().getWithDefault ("off", false);
    const bool drag  = slider.isMouseButtonDown();
    const bool hover = slider.isMouseOverOrDragging() && ! drag;
    const bool bip   = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const float ang  = startAngle + sliderPos * (endAngle - startAngle);
    auto pt = [&] (float a, float rad) { return juce::Point<float> (c.x + rad * std::sin (a), c.y - rad * std::cos (a)); };

    // rysky (11, krajni a stredova u bipolarnich delsi)
    juce::Path ticks;
    for (int i = 0; i < 11; ++i)
    {
        const float a = startAngle + (endAngle - startAngle) * (float) i / 10.0f;
        const bool major = i == 0 || i == 10 || (bip && i == 5);
        ticks.startNewSubPath (pt (a, r + ti));
        ticks.lineTo (pt (a, r + ti + (major ? 4.0f : 2.5f)));
    }
    g.setColour (off ? juce::Colour (0xff222c42) : (hover || drag) ? juce::Colour (0xff56688f) : juce::Colour (0xff34425f));
    g.strokePath (ticks, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // drazka a hodnota
    const float ra = r + 4.0f;
    const juce::PathStrokeType arcStroke (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    juce::Path track;
    track.addCentredArc (c.x, c.y, ra, ra, 0.0f, startAngle, endAngle, true);
    g.setColour (ui::knobTrack);
    g.strokePath (track, arcStroke);
    const float from = bip ? (startAngle + endAngle) * 0.5f : startAngle;
    if (std::abs (ang - from) > 0.01f)
    {
        juce::Path value;
        value.addCentredArc (c.x, c.y, ra, ra, 0.0f, juce::jmin (from, ang), juce::jmax (from, ang), true);
        g.setColour (off ? juce::Colour (0xff3a4661) : accent);
        g.strokePath (value, arcStroke);
    }

    // telo
    if (drag)
    {
        g.setColour (accent.withAlpha (0.6f));
        g.drawEllipse (juce::Rectangle<float> (2.0f * (r + 1.5f), 2.0f * (r + 1.5f)).withCentre (c), 1.0f);
    }
    const auto body = juce::Rectangle<float> (2.0f * r, 2.0f * r).withCentre (c);
    const float op = off ? 0.6f : 1.0f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff27324b).withMultipliedAlpha (op), c.x, body.getY(),
                                             juce::Colour (0xff121928).withMultipliedAlpha (op), c.x, body.getBottom(), false));
    g.fillEllipse (body);
    g.setColour ((off ? juce::Colour (0xff222c42) : drag ? accent : hover ? ui::lineHover : ui::lineStrong).withMultipliedAlpha (op));
    g.drawEllipse (body.reduced (0.5f), 1.0f);
    g.setColour (juce::Colour (0xff2a3550).withMultipliedAlpha (op));
    g.drawEllipse (juce::Rectangle<float> (1.48f * r, 1.48f * r).withCentre (c), 0.75f);

    // ukazatel
    g.setColour (off ? ui::disabled : drag ? accent : ui::text);
    g.drawLine ({ pt (ang, r * 0.3f), pt (ang, r * 0.86f) }, 2.5f);
}

juce::Label* DusiLook::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (ui::font (13.0f, 500));
    l->setJustificationType (juce::Justification::centred);
    l->setColour (juce::Label::textColourId, ui::text);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    return l;
}

juce::Font DusiLook::getLabelFont (juce::Label& l)
{
    return l.getFont();
}

void DusiLook::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (box.isMouseOver (true) || box.hasKeyboardFocus (true) ? ui::lineHover : box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
    ui::drawGlyph (g, ui::Glyph::chevronDown, juce::Rectangle<float> ((float) width - 24.0f, 0.0f, 16.0f, (float) height),
                   box.isEnabled() ? ui::textDim : ui::disabled);
}

juce::Font DusiLook::getComboBoxFont (juce::ComboBox&)
{
    return ui::font (13.0f, 500);
}

void DusiLook::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (4, 1, box.getWidth() - 26, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
}

void DusiLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    const auto accent = b.findColour (juce::TextButton::buttonOnColourId);
    if (on)
    {
        g.setColour (accent.withAlpha (down ? 0.26f : 0.16f));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (accent);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
        return;
    }
    g.setColour (down ? ui::raised : b.findColour (juce::TextButton::buttonColourId));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (highlighted && b.isEnabled() ? ui::lineHover : ui::lineStrong);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
}

juce::Font DusiLook::getTextButtonFont (juce::TextButton& b, int)
{
    return ui::font (12.0f, b.getToggleState() ? 600 : 500);
}

void DusiLook::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    const auto accent = b.findColour (juce::ToggleButton::tickColourId);
    const bool on = b.getToggleState();
    const auto box = juce::Rectangle<float> (0.5f, ((float) b.getHeight() - 18.0f) * 0.5f + 0.5f, 17.0f, 17.0f);
    g.setColour (on ? accent : ui::inset);
    g.fillRoundedRectangle (box, 4.0f);
    g.setColour (on ? accent : highlighted ? ui::lineHover : ui::lineStrong);
    g.drawRoundedRectangle (box, 4.0f, 1.0f);
    if (on)
        ui::drawGlyph (g, ui::Glyph::check, box.reduced (2.0f), ui::background, 2.0f);

    g.setColour (b.isEnabled() ? (on ? ui::text : ui::text2) : ui::disabled);
    g.setFont (ui::font (12.0f, 500));
    g.drawText (b.getButtonText(), b.getLocalBounds().withTrimmedLeft (26), juce::Justification::centredLeft);
}

juce::Font DusiLook::getPopupMenuFont()                { return ui::font (13.0f, 500); }
juce::Font DusiLook::getAlertWindowMessageFont()       { return ui::font (13.0f, 500); }
juce::Font DusiLook::getAlertWindowTitleFont()         { return ui::font (15.0f, 700); }

void DusiLook::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (ui::raised);
    g.setColour (ui::lineStrong);
    g.drawRect (0, 0, width, height);
}

juce::Rectangle<int> DusiLook::getTooltipBounds (const juce::String& tipText, juce::Point<int> pos, juce::Rectangle<int> parent)
{
    const auto f = ui::font (12.0f, 500);
    const int w = juce::jmin (360, juce::GlyphArrangement::getStringWidthInt (f, tipText) + 20);
    const int lines = juce::jmax (1, (juce::GlyphArrangement::getStringWidthInt (f, tipText) + 19) / 340 + 1);
    const int h = lines * 17 + 10;
    return juce::Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 24,
                                 pos.y > parent.getCentreY() ? pos.y - (h + 6) : pos.y + 6, w, h).constrainedWithin (parent);
}

void DusiLook::drawTooltip (juce::Graphics& g, const juce::String& t, int width, int height)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
    g.setColour (ui::raised);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (ui::lineStrong);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    g.setColour (ui::text2);
    g.setFont (ui::font (12.0f, 500));
    g.drawFittedText (t, juce::Rectangle<int> (10, 5, width - 20, height - 10), juce::Justification::centredLeft, 4);
}

//==============================================================================
PowerButton::PowerButton (juce::Colour accentColour)
    : juce::Button ("power"), accent (accentColour)
{
    setClickingTogglesState (true);
    setTooltip (ui::U ("Zapnout / vypnout"));
}

void PowerButton::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    const auto b = getLocalBounds().toFloat().reduced (0.5f);
    const bool on = getToggleState();
    g.setColour (on ? accent.withAlpha (0.14f) : ui::inset);
    g.fillRoundedRectangle (b, 5.0f);
    g.setColour (on ? accent.withAlpha (0.8f) : highlighted ? ui::lineHover : ui::lineStrong);
    g.drawRoundedRectangle (b, 5.0f, 1.0f);

    const auto col = on ? accent : (highlighted ? ui::text2 : ui::textDim);
    const auto c = b.getCentre();
    const float r = 5.0f;
    juce::Path icon;
    icon.addCentredArc (c.x, c.y + 0.5f, r, r, 0.0f, 0.7f, juce::MathConstants<float>::twoPi - 0.7f, true);
    icon.startNewSubPath (c.x, c.y - 6.0f);
    icon.lineTo (c.x, c.y - 0.5f);
    g.setColour (col);
    g.strokePath (icon, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
IconButton::IconButton (ui::Glyph g, const juce::String& tooltip, const juce::String& caption)
    : juce::Button (caption), glyph (g)
{
    setTooltip (tooltip);
}

void IconButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto b = getLocalBounds().toFloat().reduced (0.5f);
    const bool on = getToggleState();
    const bool en = isEnabled();
    g.setColour (on ? onColour.withAlpha (0.14f) : down ? ui::raised : en ? ui::panel : juce::Colour (0xff0c111c));
    g.fillRoundedRectangle (b, 6.0f);
    g.setColour (on ? onColour : ! en ? juce::Colour (0xff1f2a40) : highlighted ? ui::lineHover : ui::lineStrong);
    g.drawRoundedRectangle (b, 6.0f, 1.0f);

    const auto col = on ? onColour : ! en ? ui::disabled : highlighted ? ui::text : ui::text2;
    const auto caption = getButtonText();
    if (caption.isEmpty())
    {
        ui::drawGlyph (g, glyph, juce::Rectangle<float> (16.0f, 16.0f).withCentre (b.getCentre()), col);
        return;
    }
    const auto f = ui::font (caption == "ECO" ? 11.0f : 12.0f, caption == "ECO" ? 600 : 500, caption == "ECO" ? 0.08f : 0.0f);
    const float tw = (float) juce::GlyphArrangement::getStringWidthInt (f, caption);
    const float total = 14.0f + 6.0f + tw;
    const float x0 = b.getCentreX() - total * 0.5f;
    ui::drawGlyph (g, glyph, { x0, b.getCentreY() - 7.0f, 14.0f, 14.0f }, col);
    g.setColour (col);
    g.setFont (f);
    g.drawText (caption, juce::Rectangle<float> (x0 + 20.0f, b.getY(), tw + 4.0f, b.getHeight()), juce::Justification::centredLeft);
}

//==============================================================================
Knob::Knob (juce::Component& parent, APVTS& state, const juce::String& paramId, const juce::String& title, juce::Colour accent,
            Size s)
    : size (s)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, cellWidth (s) + 8, 16);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    parent.addAndMakeVisible (slider);

    label.setText (ui::upper (title), juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (ui::font (11.0f, 500, 0.06f));
    label.setColour (juce::Label::textColourId, ui::textDim);
    label.setInterceptsMouseClicks (false, false);
    parent.addAndMakeVisible (label);

    attachment = std::make_unique<APVTS::SliderAttachment> (state, paramId, slider);
}

void Knob::setBounds (juce::Rectangle<int> area)
{
    const int S = size == Size::lg ? 72 : size == Size::md ? 64 : 52;
    auto cellR = juce::Rectangle<int> (cellWidth (size), cellHeight (size)).withCentre (area.getCentre());
    label.setBounds (cellR.removeFromTop (14).expanded (6, 0));
    cellR.removeFromTop (2);
    slider.setBounds (cellR.withSizeKeepingCentre (cellWidth (size) + 8, S + 16).withY (cellR.getY()));
}

void Knob::setDimmed (bool dimmed)
{
    if ((bool) slider.getProperties().getWithDefault ("off", false) == dimmed) return;
    slider.getProperties().set ("off", dimmed);
    slider.setColour (juce::Slider::textBoxTextColourId, dimmed ? ui::disabled : ui::text);
    label.setColour (juce::Label::textColourId, dimmed ? ui::disabled : ui::textDim);
    slider.repaint();
}

//==============================================================================
ComboCell::ComboCell (juce::Component& parent, APVTS& state, const juce::String& paramId, const juce::String& title)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (paramId)))
        box.addItemList (p->choices, 1);
    parent.addAndMakeVisible (box);
    label.setText (ui::upper (title), juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centredLeft);
    label.setFont (ui::font (11.0f, 500, 0.06f));
    label.setColour (juce::Label::textColourId, ui::textDim);
    parent.addAndMakeVisible (label);
    attachment = std::make_unique<APVTS::ComboBoxAttachment> (state, paramId, box);
}

void ComboCell::setBounds (juce::Rectangle<int> area)
{
    label.setBounds (area.removeFromTop (16).withTrimmedLeft (-2));
    area.removeFromTop (4);
    box.setBounds (area.removeFromTop (30).withWidth (juce::jmin (area.getWidth(), 124)));
}

void ComboCell::setDimmed (bool dimmed)
{
    box.setAlpha (dimmed ? 0.45f : 1.0f);
    label.setColour (juce::Label::textColourId, dimmed ? ui::disabled : ui::textDim);
}

//==============================================================================
SyncCell::SyncCell (juce::Component& parent, APVTS& state, const juce::String& knobId, const juce::String& divId,
                    const juce::String& title, juce::Colour accent, Knob::Size s)
    : knob (parent, state, knobId, title, accent, s)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (divId)))
        division.addItemList (p->choices, 1);
    division.setColour (juce::ComboBox::outlineColourId, accent.withAlpha (0.7f));
    parent.addChildComponent (division);
    divAttachment = std::make_unique<APVTS::ComboBoxAttachment> (state, divId, division);

    info.setJustificationType (juce::Justification::centred);
    info.setFont (ui::font (12.0f, 500));
    info.setColour (juce::Label::textColourId, accent);
    parent.addChildComponent (info);
}

void SyncCell::setBounds (juce::Rectangle<int> area)
{
    knob.setBounds (area);
    const auto body = knob.slider.getBounds();
    division.setBounds (juce::Rectangle<int> (juce::jmax (84, body.getWidth()), 30).withCentre ({ body.getCentreX(), body.getY() + 24 }));
    info.setBounds (division.getBounds().translated (0, 34).withHeight (18).expanded (10, 0));
}

void SyncCell::update (bool synced, const juce::String& computed)
{
    knob.slider.setVisible (! synced);
    division.setVisible (synced);
    info.setVisible (synced);
    if (info.getText() != computed)
        info.setText (computed, juce::dontSendNotification);
}

void SyncCell::setDimmed (bool dimmed)
{
    knob.setDimmed (dimmed);
    division.setAlpha (dimmed ? 0.45f : 1.0f);
    info.setAlpha (dimmed ? 0.45f : 1.0f);
}

//==============================================================================
Toggle::Toggle (juce::Component& parent, APVTS& state, const juce::String& paramId, const juce::String& text, juce::Colour accent)
    : button (text)
{
    button.setColour (juce::ToggleButton::tickColourId, accent);
    parent.addAndMakeVisible (button);
    attachment = std::make_unique<APVTS::ButtonAttachment> (state, paramId, button);
}
