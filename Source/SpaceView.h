#pragma once

#include "PluginProcessor.h"

// 3D pohled do mistnosti (perspektiva). Zvuk je svitici bod:
//  - tazeni bodu: doleva/doprava = pan, dopredu/dozadu = hloubka
//  - tazeni krajnich uchytu nebo kolecko mysi = sirka dozvuku
//  - dvojklik = zpet na stred
class SpaceView : public juce::Component, private juce::Timer
{
public:
    explicit SpaceView (DusiVerbProcessor&);
    ~SpaceView() override;

    void paint (juce::Graphics&) override;
    void paintStatic (juce::Graphics&);
    void paintDynamic (juce::Graphics&);
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    // Perspektivni kamera: svet (x doprava, z dopredu od kamery, h nahoru) -> obrazovka
    struct Camera
    {
        float cx = 0, horizon = 0, fx = 1, fy = 1, camH = 1.8f;   // kamera vysoko = pohled shora
        float R = 1, D = 2, zNear = 1.0f;
        float zFar() const { return zNear + D; }
        juce::Point<float> project (float x, float z, float h = 0.0f) const
        {
            return { cx + x * fx / z, horizon + (camH - h) * fy / z };
        }
    };

    static constexpr float kDotHeight = 0.35f;   // zvuk se vznasi nad podlahou
    static constexpr float kRingReach = 0.6f;    // jak daleko dojdou vlny (mensi = mene prekreslovani)
    juce::Rectangle<int> dynamicBounds() const;
    bool ringsShown = false;

    Camera makeCamera() const;
    juce::Rectangle<float> contentArea() const;
    float sourceZ (const Camera&) const;
    float sourceX (const Camera&) const;

    float value (const char* id) const;
    void setValue (const char* id, float realValue);
    void beginGesture (std::initializer_list<const char*> ids);
    void endGestures();

    void dragPosition (juce::Point<float> pos);
    void dragWidth (juce::Point<float> pos);
    int hitHandle (juce::Point<float> pos) const;   // -1 levy, 1 pravy, 0 nic

    void timerCallback() override;

    DusiVerbProcessor& processor;
    enum class Drag { none, position, width } drag = Drag::none;
    juce::StringArray gestureIds;
    int hoverHandle = 0;
    float ringPhase = 0.0f, ringEnergy = 0.0f, level = 0.0f;

    // cache staticke vrstvy
    std::array<float, 12> sceneKey() const;
    juce::Image cache;
    std::array<float, 12> cacheKey {}, paintedKey {};
    float cacheScale = 0.0f, paintedLevel = -1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpaceView)
};
