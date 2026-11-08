#pragma once

#include "PluginProcessor.h"
#include "Widgets.h"
#include "ChainStrip.h"
#include "EffectViews.h"

// Panel jednoho efektu (titulek + vypinac + ovladani)
class ModulePanel : public juce::Component
{
public:
    ModulePanel (DusiVerbProcessor&, const juce::String& title, juce::Colour accent, const char* onParam, const juce::String& hint,
                 int module = -1);
    void paint (juce::Graphics&) override;
    void resized() override;
    virtual void layout() = 0;
    virtual void refresh() {}        // volano casovacem (sync hodnoty, ztlumeni pri vypnuti)

protected:
    static juce::Point<int> knobPos (int col, int row) { return { 13 + col * 92, row == 0 ? 41 : 159 }; }
    void setVizRows (int rows) { vizRows = rows; }
    bool isOn() const;
    juce::Label* addCaption (const juce::String& text);
    juce::OwnedArray<juce::Label> captions;
    std::unique_ptr<EffectViz> viz;
    int vizRows = 2;
    bool shownOn = true;
    void updateTitle() { const bool on = isOn(); if (on != shownOn) { shownOn = on; repaint (0, 0, getWidth(), ui::panelTitle); } }

    DusiVerbProcessor& proc;
    juce::String title, hint;
    juce::Colour accent;
    const char* onParam;
    PowerButton power;
    std::unique_ptr<APVTS::ButtonAttachment> powerAtt;
};

// Stranka EFEKTY: retez nahore, pod nim nastaveni vybraneho efektu, dole dynamika (ducking + komprese)
class EffectsPage : public juce::Component, private juce::Timer
{
public:
    explicit EffectsPage (DusiVerbProcessor&);
    ~EffectsPage() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void select (dusi::Module m);

private:
    void timerCallback() override;

    DusiVerbProcessor& proc;
    ChainStrip chain;
    std::array<std::unique_ptr<ModulePanel>, dusi::kNumModules> panels;
    std::unique_ptr<juce::Component> dynamics;
    dusi::Module current = dusi::Module::reverb;
};
