#pragma once

#include "PluginProcessor.h"
#include "Widgets.h"

// Spodni vizualizace v panelu efektu (910 x 120):
//   delay = opakovani v case, reverb = obalka dozvuku, filtr = LFO + odezva, chorus = pohyb L/P, lo-fi = schody
// Kresli se do obrazku jen pri zmene parametru; u LFO se kazdy snimek dokresli jen poloha (vypnuto v ECO).
class EffectViz : public juce::Component, private juce::Timer
{
public:
    EffectViz (DusiVerbProcessor&, dusi::Module);
    void paint (juce::Graphics&) override;
    void visibilityChanged() override;

private:
    void timerCallback() override;
    std::vector<float> key() const;
    void paintStatic (juce::Graphics&, const dusi::Params&);
    void paintDelay  (juce::Graphics&, juce::Rectangle<float>, const dusi::Params&);
    void paintReverb (juce::Graphics&, juce::Rectangle<float>, const dusi::Params&);
    void paintFilter (juce::Graphics&, juce::Rectangle<float>, const dusi::Params&);
    void paintChorus (juce::Graphics&, juce::Rectangle<float>, const dusi::Params&);
    void paintLofi   (juce::Graphics&, juce::Rectangle<float>, const dusi::Params&);
    void header (juce::Graphics&, juce::Rectangle<float> area, const juce::String& title, const juce::String& info, juce::Colour);
    bool animated() const;

    DusiVerbProcessor& proc;
    dusi::Module module;
    juce::Colour accent;
    juce::Image cache;
    std::vector<float> cacheKey;
    float cacheScale = 0.0f;
    float shownPhase = -1.0f;
    juce::Rectangle<float> lfoArea;   // kde se kresli ukazatel faze
};

// Velky prepinac FREEZE (240 x 60)
class FreezeButton : public juce::Button
{
public:
    FreezeButton() : juce::Button ("FREEZE") { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

// Box "ZTLUMENO −6.2 dB" s pruhem (236 x 92)
class DuckMeter : public juce::Component
{
public:
    void setGainDb (float db, bool active);
    void paint (juce::Graphics&) override;

private:
    float shownDb = 0.0f;
    bool shownActive = false;
};
