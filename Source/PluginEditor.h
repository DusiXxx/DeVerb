#pragma once

#include "PluginProcessor.h"
#include "Widgets.h"
#include "MainPage.h"
#include "EffectsPage.h"
#include "PresetBrowser.h"
#include "Updater.h"

// Zalozka v segmentu (PROSTOR & EQ / EFEKTY)
class TabButton : public juce::Button
{
public:
    explicit TabButton (const juce::String& t) : juce::Button (t) {}
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

// Pole s nazvem presetu + kategorie + sipka (klik = prohlizec)
class PresetField : public juce::Button
{
public:
    PresetField() : juce::Button ("preset") {}
    void set (const juce::String& name, const juce::String& category);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    bool open = false;

private:
    juce::String presetName, presetCategory;
};

// Segment A | B
class AbSegment : public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<void (int)> onSelect;
    void setSlot (int s) { if (s != slot) { slot = s; repaint(); } }
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    int slot = 0;
};

// Verze pluginu + aktualizace z GitHubu (vedle loga)
class UpdateButton : public juce::Button
{
public:
    explicit UpdateButton (Updater& u) : juce::Button ("update"), updater (u) {}
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    bool refresh();                         // true = zmenil se stav (prekreslit)
    int preferredWidth() const;

private:
    juce::String label() const;
    Updater& updater;
    Updater::State shown = Updater::State::idle;
    int shownPercent = -1;
};

class DusiVerbEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DusiVerbEditor (DusiVerbProcessor&);
    ~DusiVerbEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void showPage (int index);          // 0 = PROSTOR & EQ, 1 = EFEKTY
    void showBrowser (bool show);

private:
    void timerCallback() override;
    void stepPreset (int delta);
    void savePreset();
    void updateClicked();
    void refreshPresetField();

    DusiVerbProcessor& proc;
    DusiLook look;
    juce::SharedResourcePointer<Updater> updater;
    juce::TooltipWindow tooltips { this, 600 };

    MainPage mainPage;
    EffectsPage effectsPage;
    PresetBrowser browser;

    TabButton tabMain { juce::String::fromUTF8 ("PROSTOR & EQ") }, tabEffects { "EFEKTY" };
    UpdateButton updateButton { *updater };
    IconButton ecoButton { ui::Glyph::leaf, juce::String::fromUTF8 ("Úsporný režim: statický náhled bez animací a analyzátoru (šetří starší Mac)"), "ECO" };
    IconButton undoButton { ui::Glyph::undo, juce::String::fromUTF8 ("Zpět") };
    IconButton redoButton { ui::Glyph::redo, juce::String::fromUTF8 ("Vpřed") };

    IconButton prevPreset { ui::Glyph::chevronLeft, juce::String::fromUTF8 ("Předchozí preset") };
    IconButton nextPreset { ui::Glyph::chevronRight, juce::String::fromUTF8 ("Další preset") };
    PresetField presetField;
    IconButton randomButton { ui::Glyph::dice, juce::String::fromUTF8 ("Náhodné nastavení efektů pro inspiraci (EQ a stereo zůstanou)"),
                              juce::String::fromUTF8 ("Náhodný") };
    juce::TextButton saveButton { juce::String::fromUTF8 ("Uložit…") };
    AbSegment abSegment;
    juce::TextButton abCopy { juce::String::fromUTF8 ("A→B") };

    juce::String shownPresetName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DusiVerbEditor)
};
