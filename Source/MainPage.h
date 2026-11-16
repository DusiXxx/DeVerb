#pragma once

#include "PluginProcessor.h"
#include "Widgets.h"
#include "SpaceView.h"
#include "EqView.h"

// SPACE (jako ve FabFilter Pro-R): jeden velky knob = velikost mistnosti i delka dozvuku najednou
class SpaceMacro : public juce::Component
{
public:
    explicit SpaceMacro (DusiVerbProcessor&);
    ~SpaceMacro() override;
    void refresh();                      // z parametru (volano casovacem)
    void paint (juce::Graphics&) override;
    void resized() override;
    static juce::String roomName (float s);

private:
    void apply (float s);
    DusiVerbProcessor& proc;
    juce::Slider knob;
    bool dragging = false;
    juce::String shownText;
    float shownS = -1.0f;
};

// Hlavni stranka: PROSTOR (SPACE + Pro-R knoby + 3D) + STEREO + TEMPO nahore, EQ dole
class MainPage : public juce::Component, private juce::Timer
{
public:
    explicit MainPage (DusiVerbProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void paintCorrelation (juce::Graphics&);
    void paintTempo (juce::Graphics&);
    void setEqMode (EqView::Mode m);

    DusiVerbProcessor& proc;
    SpaceView space;
    EqView eqView;

    juce::Rectangle<int> stereoPanel, tempoPanel, eqPanel, meterArea, tempoArea;
    juce::Rectangle<int> spacePanel;

    SpaceMacro spaceMacro;
    Knob brightness, character, distance, width;
    Knob pan, monoBass, outGain;
    juce::TextButton safeButton { "SAFE" };
    std::unique_ptr<APVTS::ButtonAttachment> safeAtt;

    juce::TextButton outMode, decayMode;
    PowerButton eqPower, decayPower;
    std::unique_ptr<APVTS::ButtonAttachment> eqPowerAtt, decayPowerAtt;

    float shownCorrelation = 1.0f, shownAutoGain = 1.0f;
    double shownBpm = 0.0;
    int shownTempoKey = -1;
    int tempoHighlightKey() const;
};
