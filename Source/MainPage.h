#pragma once

#include "PluginProcessor.h"
#include "Widgets.h"
#include "SpaceView.h"
#include "EqView.h"

// Hlavni stranka: PROSTOR (3D) + STEREO + TEMPO nahore, EQ dole
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

    Knob pan, width, monoBass, outGain;
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
