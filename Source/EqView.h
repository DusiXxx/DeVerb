#pragma once

#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"
#include "Widgets.h"

// Graf EQ ve stylu FabFilter:
//  - VYSTUP: 8 pasem, krivka + analyzator spektra
//  - DECAY: 4 body, ktere meni delku dozvuku po frekvencich (jako Pro-R)
// Ovladani: tahni bod (frekvence + zisk/nasobek), kolecko = Q, dvojklik do prazdna = nove pasmo,
// dvojklik na bod = vypnout (u decay = vratit na x1), prave tlacitko = typ pasma.
class EqView : public juce::Component, private juce::Timer
{
public:
    enum class Mode { output, decay };

    explicit EqView (DusiVerbProcessor&);
    ~EqView() override;

    void setMode (Mode m) { mode = m; selected = -1; repaint(); }
    Mode getMode() const  { return mode; }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    // souradnice
    float freqToX (float f) const;
    float xToFreq (float x) const;
    float dbToY (float db) const;
    float yToDb (float y) const;
    float multToY (float m) const { return dbToY (std::log2 (m) * 9.0f); }   // x4 = +18 dB polohy
    float yToMult (float y) const { return std::pow (2.0f, yToDb (y) / 9.0f); }

    // parametry
    float value (const juce::String& id) const;
    void setValue (const juce::String& id, float v);
    void beginGesture (std::initializer_list<juce::String> ids);
    void endGestures();

    juce::Point<float> nodePos (int index) const;     // index pasma v aktualnim rezimu
    bool nodeVisible (int index) const;
    int hitNode (juce::Point<float> p) const;
    int numNodes() const { return mode == Mode::output ? dusi::kNumEqBands : dusi::kNumDecayBands; }
    bool isGainType (int band) const;
    void showBandMenu (int band);
    juce::String nodeText (int index) const;

    void drawGrid (juce::Graphics&);
    void drawAnalyzer (juce::Graphics&);
    void drawOutputCurve (juce::Graphics&, bool active);
    void drawDecayCurve (juce::Graphics&, bool active);
    void drawNodes (juce::Graphics&);

    DusiVerbProcessor& processor;
    Mode mode = Mode::output;
    juce::Rectangle<float> graph;
    int selected = -1, hover = -1;
    bool dragging = false;
    juce::StringArray gestureIds;

    // analyzator
    int fftOrder = 11;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> window;
    std::vector<float> history, fftData, readBuf;
    std::vector<float> spectrum;   // dB pro kazdy bin, vyhlazene
    int writePos = 0;
    void setupFft (int order);

    // cache vrstev
    std::vector<float> overlayKey() const;
    std::vector<std::atomic<float>*> watched;
    juce::Image bgCache, overlayCache;
    std::vector<float> overlayCacheKey, paintedKey;
    int bgCacheKey = -1;
    float cacheScale = 0.0f;
    bool analyzerOn = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EqView)
};
