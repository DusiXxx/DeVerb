#pragma once

#include "PluginProcessor.h"
#include "Widgets.h"

// Retez efektu: POZICE > [5 bloku, ktere jde pretahnout] > EQ
// Klik na blok = otevrit jeho nastaveni, tecka v bloku = zapnout/vypnout, tazeni = zmena poradi.
class ChainStrip : public juce::Component, private juce::Timer
{
public:
    explicit ChainStrip (DusiVerbProcessor&);

    std::function<void (dusi::Module)> onSelect;
    void setSelected (dusi::Module m) { selected = m; repaint(); }

    static juce::String moduleName (dusi::Module m);
    static juce::Colour moduleColour (dusi::Module m);
    static const char* onId (dusi::Module m);
    static const char* mixId (dusi::Module m);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    std::vector<float> stateKey() const;
    std::vector<float> lastKey;

    dusi::ChainOrder currentOrder() const;
    juce::Rectangle<float> slotBounds (int slot) const;   // 0 = POZICE, 1..5 moduly, 6 = EQ
    juce::Rectangle<float> powerDot (juce::Rectangle<float> block) const;
    void setOrder (const dusi::ChainOrder& order);

    DusiVerbProcessor& processor;
    dusi::Module selected = dusi::Module::reverb;
    int dragSlot = -1;
    float dragOffset = 0.0f, dragX = 0.0f;
    bool moved = false;
};
