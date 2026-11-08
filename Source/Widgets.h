#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

// Spolecne barvy, pismo, vzhled a ovladaci prvky (design v0.5)
namespace ui
{
// --- pozadi a linky
inline const juce::Colour background  { 0xff080c14 };   // okno
inline const juce::Colour panel       { 0xff0f1522 };
inline const juce::Colour raised      { 0xff161e2f };
inline const juce::Colour inset       { 0xff0a0f19 };
inline const juce::Colour line        { 0xff1c2539 };
inline const juce::Colour lineStrong  { 0xff2c3a56 };
inline const juce::Colour lineHover   { 0xff4a5e86 };
inline const juce::Colour knobTrack   { 0xff1a2336 };
// --- text
inline const juce::Colour text        { 0xffe6ecf7 };
inline const juce::Colour text2       { 0xffc3cce0 };
inline const juce::Colour textDim     { 0xff8c9ab6 };
inline const juce::Colour disabled    { 0xff56637f };
// --- barvy modulu
inline const juce::Colour space       { 0xff4da8ff };
inline const juce::Colour delay       { 0xff2fd6b5 };
inline const juce::Colour verb        { 0xff9d8cff };
inline const juce::Colour filter      { 0xfff06bd0 };
inline const juce::Colour chorus      { 0xfff5c451 };
inline const juce::Colour lofi        { 0xffff8757 };
inline const juce::Colour eq          { 0xff9be15d };
inline const juce::Colour output      { 0xffe6ecf7 };
inline const juce::Colour dynamics    { 0xffc9d3e6 };
inline const juce::Colour out = lofi;                 // starsi nazev
// --- stavy
inline const juce::Colour good        { 0xff34d399 };
inline const juce::Colour mid         { 0xffffb547 };
inline const juce::Colour bad         { 0xffff5c6c };

constexpr int pad = 12, gap = 8, panelTitle = 40, smallTitle = 34;

inline juce::String U (const char* s) { return juce::String::fromUTF8 (s); }

// Velka pismena vcetne cestiny (toUpperCase nezna vsechny znaky s diakritikou na kazdem systemu)
inline juce::String upper (const juce::String& s)
{
    static const juce::String lo = U ("áčďéěíňóřšťúůýž"), up = U ("ÁČĎÉĚÍŇÓŘŠŤÚŮÝŽ");
    juce::String r;
    for (auto c : s)
    {
        const int i = lo.indexOfChar (c);
        r += i >= 0 ? up[i] : juce::CharacterFunctions::toUpperCase (c);
    }
    return r;
}

// Pismo Inter (vlozene v pluginu). weight: 500 / 600 / 700, tracking = mezery mezi pismeny v em
juce::Font font (float px, int weight = 500, float tracking = 0.0f);

// Panel: pozadi, linka, titulek velkymi pismeny v barve modulu
void paintPanel (juce::Graphics&, juce::Rectangle<int> r, const juce::String& title, juce::Colour accent,
                 int titleH = panelTitle, const juce::String& subtitle = {});

// Mala ikona (14-16 px, cara 1.5)
enum class Glyph { undo, redo, leaf, dice, chevronDown, chevronLeft, chevronRight, download, check, refresh, star, folder, search, close };
void drawGlyph (juce::Graphics&, Glyph, juce::Rectangle<float> box, juce::Colour, float stroke = 1.5f);

// Stitek ("chip") se stavem
void paintChip (juce::Graphics&, juce::Rectangle<float> r, const juce::String& text, juce::Colour col);
}

using APVTS = juce::AudioProcessorValueTreeState;

class DusiLook : public juce::LookAndFeel_V4
{
public:
    DusiLook();
    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    juce::Font getLabelFont (juce::Label&) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown, int buttonX, int buttonY,
                       int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& background,
                               bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override;
    juce::Font getAlertWindowMessageFont() override;
    juce::Font getAlertWindowTitleFont() override;
};

// Vypinac efektu (ikona power 28x22)
class PowerButton : public juce::Button
{
public:
    explicit PowerButton (juce::Colour accentColour);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    juce::Colour accent;
};

// Tlacitko s kreslenou ikonou a volitelnym textem (zpet, vpred, kostka, ECO...)
class IconButton : public juce::Button
{
public:
    IconButton (ui::Glyph g, const juce::String& tooltip, const juce::String& caption = {});
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    void setGlyph (ui::Glyph g) { glyph = g; repaint(); }
    juce::Colour onColour { ui::good };   // barva zapnuteho stavu (ECO)

private:
    ui::Glyph glyph;
};

// Knoflik: popisek nahore, telo s ryskami, hodnota dole. Velikost lg (84x106) / md (76x98) / sm (64x86)
struct Knob
{
    enum class Size { lg, md, sm };
    Knob (juce::Component& parent, APVTS& state, const juce::String& paramId, const juce::String& title, juce::Colour accent,
          Size size = Size::lg);
    void setBounds (juce::Rectangle<int> area);   // area = bunka; knoflik se vycentruje
    void setTopLeft (int x, int y) { setBounds ({ x, y, cellWidth (size), cellHeight (size) }); }
    void setDimmed (bool dimmed);
    static int cellWidth (Size s)  { return s == Size::lg ? 84 : s == Size::md ? 76 : 64; }
    static int cellHeight (Size s) { return s == Size::lg ? 106 : s == Size::md ? 98 : 86; }

    juce::Slider slider;
    juce::Label label;
    Size size;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

// Popisek + rozbalovaci vyber
struct ComboCell
{
    ComboCell (juce::Component& parent, APVTS& state, const juce::String& paramId, const juce::String& title);
    void setBounds (juce::Rectangle<int> area);
    void setDimmed (bool dimmed);

    juce::ComboBox box;
    juce::Label label;
    std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
};

// Bunka: knoflik, nebo vyber noty, kdyz je zapnuty sync na tempo
struct SyncCell
{
    SyncCell (juce::Component& parent, APVTS& state, const juce::String& knobId, const juce::String& divId,
              const juce::String& title, juce::Colour accent, Knob::Size size = Knob::Size::lg);
    void setBounds (juce::Rectangle<int> area);
    void setTopLeft (int x, int y) { setBounds ({ x, y, Knob::cellWidth (knob.size), Knob::cellHeight (knob.size) }); }
    void update (bool synced, const juce::String& computed);
    void setDimmed (bool dimmed);

    Knob knob;
    juce::ComboBox division;
    juce::Label info;
    std::unique_ptr<APVTS::ComboBoxAttachment> divAttachment;
};

// Zaskrtavatko 18 px s textem
struct Toggle
{
    Toggle (juce::Component& parent, APVTS& state, const juce::String& paramId, const juce::String& text, juce::Colour accent);
    juce::ToggleButton button;
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
};
