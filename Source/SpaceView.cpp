#include "SpaceView.h"
#include "Widgets.h"

namespace
{
const juce::Colour kPanel   { 0xff0f1522 };
const juce::Colour kText    { 0xffe6ecf7 };
const juce::Colour kTextDim { 0xff8c9ab6 };
const juce::Colour kAccent  { 0xff4da8ff };   // modra - zvuk
const juce::Colour kVerb    { 0xff9d8cff };
const juce::Colour kWarn    { 0xffff5c6c };
const juce::Colour kOrange  { 0xffffb547 };

inline juce::String U (const char* s) { return juce::String::fromUTF8 (s); }
}

SpaceView::SpaceView (DusiVerbProcessor& p) : processor (p)
{
    setRepaintsOnMouseActivity (false);
    setOpaque (true);
    startTimerHz (15);
}

SpaceView::~SpaceView()
{
    endGestures();
}

//------------------------------------------------------------------------------
float SpaceView::value (const char* id) const
{
    return processor.apvts.getRawParameterValue (id)->load();
}

void SpaceView::setValue (const char* id, float realValue)
{
    if (auto* p = processor.apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (realValue));
}

void SpaceView::beginGesture (std::initializer_list<const char*> ids)
{
    endGestures();
    for (auto* id : ids)
        if (auto* p = processor.apvts.getParameter (id))
        {
            p->beginChangeGesture();
            gestureIds.add (id);
        }
}

void SpaceView::endGestures()
{
    for (auto& id : gestureIds)
        if (auto* p = processor.apvts.getParameter (id))
            p->endChangeGesture();
    gestureIds.clear();
}

//------------------------------------------------------------------------------
juce::Rectangle<float> SpaceView::contentArea() const
{
    return getLocalBounds().toFloat();
}

SpaceView::Camera SpaceView::makeCamera() const
{
    const auto a = contentArea();
    const auto room = dusi::space::Room::fromSize (value ("size") * 0.01f);   // stejna mistnost jako v DSP (odrazy)

    Camera c;
    c.R = room.R;          // vetsi Size = sirsi a hlubsi mistnost
    c.D = room.D;
    const float frontY = a.getBottom() - 34.0f;
    c.fx = a.getWidth() * 0.42f;
    c.fy = juce::jmax (40.0f, (frontY - (a.getY() + 40.0f)) * c.zNear / c.camH);
    c.horizon = frontY - c.camH * c.fy / c.zNear;
    c.cx = a.getCentreX();
    return c;
}

float SpaceView::sourceZ (const Camera& c) const
{
    const float depth = value ("depth") * 0.01f;
    return c.zNear + c.D * (0.1f + 0.8f * depth);
}

float SpaceView::sourceX (const Camera& c) const
{
    return value ("pan") * 0.01f * dusi::space::kPanReach * c.R;
}

//------------------------------------------------------------------------------
// Staticka cast (mistnost, odrazy, popisky) se kresli jen pri zmene parametru do obrazku v pameti,
// kazdy snimek se pak jen zkopiruje a dokresli se ziva vrstva. Setri to vetsinu vykonu.
std::array<float, 12> SpaceView::sceneKey() const
{
    const bool safe = value ("safe") > 0.5f;
    return { value ("size"), value ("pan"), value ("depth"), value ("width"), safe ? 1.0f : 0.0f,
             value ("reverbOn"), value ("reverbMix"),
             safe ? (float) juce::roundToInt (processor.uiAutoGain.load() * 50.0f) : 0.0f,
             (float) getWidth(), (float) getHeight(), processor.eco.load() ? 1.0f : 0.0f, 0.0f };
}

void SpaceView::paint (juce::Graphics& g)
{
    const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    const auto key = sceneKey();
    if (cache.isNull() || key != cacheKey || ! juce::approximatelyEqual (scale, cacheScale))
    {
        cache = juce::Image (juce::Image::RGB, juce::jmax (1, juce::roundToInt ((float) getWidth() * scale)),
                             juce::jmax (1, juce::roundToInt ((float) getHeight() * scale)), false);
        juce::Graphics ig (cache);
        ig.addTransform (juce::AffineTransform::scale (scale));
        paintStatic (ig);
        cacheKey = key;
        cacheScale = scale;
    }
    g.drawImage (cache, getLocalBounds().toFloat());
    paintDynamic (g);
    paintedKey = key;
    paintedLevel = level;
}

//------------------------------------------------------------------------------
void SpaceView::paintStatic (juce::Graphics& g)
{
    g.fillAll (kPanel);   // panel s titulkem kresli MainPage, tady jen 3D okno

    const auto area = contentArea();
    juce::Graphics::ScopedSaveState save (g);
    {
        juce::Path clip;
        clip.addRoundedRectangle (area, 6.0f);
        g.reduceClipRegion (clip);
    }
    g.setColour (ui::inset);
    g.fillRect (area);

    const auto c = makeCamera();
    const float zN = c.zNear, zF = c.zFar(), R = c.R;
    const float wallH = 0.75f;   // nizke steny - divame se do mistnosti shora

    const bool reverbOn = value ("reverbOn") > 0.5f;
    const bool safe     = value ("safe") > 0.5f;
    const float pan     = value ("pan") * 0.01f;
    const float widthReq = value ("width") * 0.01f;
    const float widthEff = safe ? juce::jmin (widthReq, dusi::space::safeWidthLimit (pan)) : widthReq;
    const float mix     = value ("reverbMix") * 0.01f;

    const float xs = sourceX (c), zs = sourceZ (c);
    const float hwReq = widthReq * dusi::space::kWidthReach * R;
    const float hwEff = widthEff * dusi::space::kWidthReach * R;
    const bool overLeft  = xs - hwReq < -R;
    const bool overRight = xs + hwReq >  R;

    auto quad = [&c] (std::array<juce::Point<float>, 4> pts)
    {
        juce::Path p;
        p.startNewSubPath (pts[0]);
        for (int i = 1; i < 4; ++i) p.lineTo (pts[(size_t) i]);
        p.closeSubPath();
        juce::ignoreUnused (c);
        return p;
    };

    // --- mistnost: zadni stena, bocni steny, podlaha
    const auto FL = c.project (-R, zN), FR = c.project (R, zN), BL = c.project (-R, zF), BR = c.project (R, zF);
    const auto FLt = c.project (-R, zN, wallH), FRt = c.project (R, zN, wallH);
    const auto BLt = c.project (-R, zF, wallH), BRt = c.project (R, zF, wallH);

    g.setColour (juce::Colour (0xff121a2b));
    g.fillPath (quad ({ BL, BR, BRt, BLt }));

    auto sideWall = [&] (std::array<juce::Point<float>, 4> pts, bool warn)
    {
        auto path = quad (pts);
        g.setColour (juce::Colour (0xff0e1524));
        g.fillPath (path);
        if (warn && ! safe)
        {
            g.setColour (kWarn.withAlpha (0.22f));
            g.fillPath (path);
        }
    };
    sideWall ({ FL, BL, BLt, FLt }, overLeft);
    sideWall ({ FR, BR, BRt, FRt }, overRight);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff131c2f), 0.0f, BL.y,
                                             juce::Colour (0xff0c1424), 0.0f, FL.y, false));
    g.fillPath (quad ({ FL, FR, BR, BL }));

    // mrizka na podlaze dava pocit hloubky
    g.setColour (juce::Colour (0xff1c2a44));
    for (int i = -4; i <= 4; ++i)
    {
        const float x = R * (float) i / 4.0f;
        g.drawLine ({ c.project (x, zN), c.project (x, zF) }, 1.0f);
    }
    for (int j = 1; j < 6; ++j)
    {
        const float z = zN + c.D * (float) j / 6.0f;
        g.drawLine ({ c.project (-R, z), c.project (R, z) }, 1.0f);
    }

    g.setColour (ui::lineStrong);
    for (auto [a, b] : { std::pair { FL, BL }, { FR, BR }, { BL, BR }, { BL, BLt }, { BR, BRt }, { BLt, BRt },
                         { FL, FLt }, { FR, FRt } })
        g.drawLine ({ a, b }, 1.0f);

    // --- odrazy: zdroj -> stena -> posluchac
    const juce::Point<float> listener { c.cx, FL.y + 22.0f };
    const auto src = c.project (xs, zs, kDotHeight);
    if (reverbOn)
    {
        const float alpha = 0.06f + 0.35f * mix;
        const std::array<juce::Point<float>, 3> reflections {
            c.project (-R, zs, kDotHeight), c.project (R, zs, kDotHeight), c.project (xs, zF, kDotHeight) };
        for (const auto& rp : reflections)
        {
            juce::Path ray;
            ray.startNewSubPath (src);
            ray.lineTo (rp);
            ray.lineTo (listener);
            g.setColour (kVerb.withAlpha (alpha));
            const float dashes[] = { 4.0f, 4.0f };
            juce::PathStrokeType (1.2f).createDashedStroke (ray, ray, dashes, 2);
            g.fillPath (ray);
            g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (rp));
        }
    }

    // --- sirka: stereo "mrak" dozvuku na podlaze
    auto floorEllipse = [&c] (float x0, float z0, float rx, float rz)
    {
        juce::Path p;
        for (int i = 0; i <= 48; ++i)
        {
            const float t = juce::MathConstants<float>::twoPi * (float) i / 48.0f;
            const auto pt = c.project (x0 + rx * std::cos (t), juce::jmax (0.2f, z0 + rz * std::sin (t)));
            if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        p.closeSubPath();
        return p;
    };

    const float rz = 0.12f * c.D;
    const bool danger = ! safe && (overLeft || overRight);
    {
        auto cloud = floorEllipse (xs, zs, juce::jmax (0.03f, hwEff), rz);
        const auto col = danger ? kWarn : kAccent;
        g.setColour (col.withAlpha (reverbOn ? 0.20f : 0.08f));
        g.fillPath (cloud);
        g.setColour (col.withAlpha (0.7f));
        g.strokePath (cloud, juce::PathStrokeType (1.5f));
    }
    if (safe && hwReq > hwEff + 0.01f)   // pozadovana sirka nad bezpecnym limitem
    {
        auto ghost = floorEllipse (xs, zs, hwReq, rz);
        const float dashes[] = { 5.0f, 4.0f };
        juce::PathStrokeType (1.2f).createDashedStroke (ghost, ghost, dashes, 2);
        g.setColour (kOrange.withAlpha (0.7f));
        g.fillPath (ghost);
    }

    // --- posluchac (hlava se sluchatky)
    g.setColour (kText.withAlpha (0.85f));
    g.fillEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre (listener));
    g.setColour (kTextDim);
    {
        juce::Path band;   // oblouk sluchatek
        band.addCentredArc (listener.x, listener.y, 11.0f, 11.0f, 0.0f,
                            -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
        g.strokePath (band, juce::PathStrokeType (2.0f));
    }
    g.fillRoundedRectangle (juce::Rectangle<float> (4.0f, 8.0f).withCentre ({ listener.x - 11.0f, listener.y + 1.0f }), 1.5f);
    g.fillRoundedRectangle (juce::Rectangle<float> (4.0f, 8.0f).withCentre ({ listener.x + 11.0f, listener.y + 1.0f }), 1.5f);

    g.setFont (ui::font (12.0f, 600));
    g.setColour (kTextDim);
    g.drawText ("L", juce::Rectangle<float> (20.0f, 16.0f).withCentre ({ FL.x + 10.0f, FL.y + 12.0f }), juce::Justification::centred);
    g.drawText ("R", juce::Rectangle<float> (20.0f, 16.0f).withCentre ({ FR.x - 10.0f, FR.y + 12.0f }), juce::Justification::centred);

    // --- popisky a stavove stitky
    const auto panText = processor.apvts.getParameter ("pan")->getCurrentValueAsText();
    juce::String info = "Pan " + panText + U ("  ·  Vzdálenost ") + juce::String (juce::roundToInt (value ("depth")))
                      + U (" %  ·  Šířka ") + juce::String (juce::roundToInt (widthEff * 100.0f)) + " %";
    g.setFont (ui::font (11.0f, 500));
    g.setColour (ui::text2);
    g.drawText (info, area.reduced (10.0f, 8.0f).removeFromTop (16.0f), juce::Justification::centredLeft);

    std::vector<std::pair<juce::String, juce::Colour>> chips;
    if (! reverbOn)                                chips.push_back ({ U ("Reverb vypnutý"), kTextDim });
    if (danger)                                    chips.push_back ({ U ("Mimo prostor – vrať zvuk dovnitř"), kWarn });
    if (safe && widthReq > widthEff + 0.005f)      chips.push_back ({ U ("Šířka omezena (SAFE)"), kOrange });
    const float autoGain = processor.uiAutoGain.load();
    if (safe && autoGain < 0.97f)
        chips.push_back ({ U ("Auto-safe zužuje o ") + juce::String (juce::roundToInt ((1.0f - autoGain) * 100.0f)) + " %", kOrange });
    if (processor.eco.load())                      chips.push_back ({ U ("ECO – statický náhled"), ui::good });
    float cx = area.getRight() - 10.0f;
    for (const auto& [t, col] : chips)
    {
        const float w = (float) juce::GlyphArrangement::getStringWidthInt (ui::font (11.0f, 600), t) + 32.0f;
        cx -= w;
        ui::paintChip (g, { cx, area.getY() + 8.0f, w, 22.0f }, t, col);
        cx -= 6.0f;
    }

    if (! reverbOn)
    {
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRect (area);
        g.setColour (kText.withAlpha (0.8f));
        g.setFont (ui::font (13.0f, 500));
        g.drawText (U ("Reverb je vypnutý – vzdálenost a šířka nepůsobí, funguje jen pan"),
                    area.withTrimmedBottom (area.getHeight() * 0.55f), juce::Justification::centred);
    }
}

//------------------------------------------------------------------------------
// Ziva vrstva: vlny, zare podle hlasitosti, tecka a uchyty (kresli se kazdy snimek)
void SpaceView::paintDynamic (juce::Graphics& g)
{
    const auto area = contentArea();
    juce::Graphics::ScopedSaveState save (g);
    {
        juce::Path clip;
        clip.addRoundedRectangle (area, 6.0f);
        g.reduceClipRegion (clip);
    }
    const auto c = makeCamera();
    const float zN = c.zNear, R = c.R;
    const bool reverbOn = value ("reverbOn") > 0.5f;
    const float widthReq = value ("width") * 0.01f;
    const float xs = sourceX (c), zs = sourceZ (c);
    const float hwReq = widthReq * dusi::space::kWidthReach * R;
    const auto src = c.project (xs, zs, kDotHeight);
    auto floorEllipse = [&c] (float x0, float z0, float rx, float rz)
    {
        juce::Path p;
        for (int i = 0; i <= 32; ++i)
        {
            const float t = juce::MathConstants<float>::twoPi * (float) i / 32.0f;
            const auto pt = c.project (x0 + rx * std::cos (t), juce::jmax (0.2f, z0 + rz * std::sin (t)));
            if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        p.closeSubPath();
        return p;
    };

    // --- vlny ze zdroje (zivost podle signalu)
    if (ringEnergy > 0.0f && reverbOn)
    {
        for (int k = 0; k < 2; ++k)
        {
            const float frac = std::fmod (ringPhase + (float) k / 2.0f, 1.0f);
            const float r = 0.05f + frac * R * 1.1f;
            g.setColour (kVerb.withAlpha ((1.0f - frac) * 0.35f * ringEnergy));
            g.strokePath (floorEllipse (xs, zs, r, r * 0.7f), juce::PathStrokeType (1.2f));
        }
    }

    // --- zdroj zvuku
    const auto shadow = c.project (xs, zs, 0.0f);
    const float rad = 10.0f * zN / zs + 4.0f;
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (juce::Rectangle<float> (rad * 2.4f, rad * 0.8f).withCentre (shadow));
    g.setColour (kAccent.withAlpha (0.35f));
    g.drawLine ({ shadow, src }, 1.0f);

    const float glow = rad * (2.0f + 3.0f * juce::jlimit (0.0f, 1.0f, level * 2.0f));
    g.setGradientFill (juce::ColourGradient (kAccent.withAlpha (0.55f), src.x, src.y,
                                             kAccent.withAlpha (0.0f), src.x + glow, src.y, true));
    g.fillEllipse (juce::Rectangle<float> (glow * 2.0f, glow * 2.0f).withCentre (src));
    g.setColour (drag == Drag::position ? juce::Colours::white : kAccent.brighter (0.4f));
    g.fillEllipse (juce::Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (src));
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.drawEllipse (juce::Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (src), 1.5f);

    // --- uchyty sirky
    for (int side : { -1, 1 })
    {
        const auto h = c.project (xs + (float) side * hwReq, zs);
        const bool hot = hoverHandle == side || (drag == Drag::width);
        g.setColour (hot ? juce::Colours::white : kAccent.brighter (0.2f));
        g.fillEllipse (juce::Rectangle<float> (hot ? 11.0f : 9.0f, hot ? 11.0f : 9.0f).withCentre (h));
    }

}

//------------------------------------------------------------------------------
int SpaceView::hitHandle (juce::Point<float> pos) const
{
    const auto c = makeCamera();
    const float xs = sourceX (c), zs = sourceZ (c);
    const float hw = value ("width") * 0.01f * dusi::space::kWidthReach * c.R;
    for (int side : { -1, 1 })
        if (pos.getDistanceFrom (c.project (xs + (float) side * hw, zs)) < 10.0f)
            return side;
    return 0;
}

void SpaceView::dragPosition (juce::Point<float> pos)
{
    const auto c = makeCamera();
    const float dy = pos.y - c.horizon;
    float z = dy > 1.0f ? (c.camH - kDotHeight) * c.fy / dy : c.zFar();
    z = juce::jlimit (c.zNear, c.zFar(), z);
    const float x = (pos.x - c.cx) * z / c.fx;

    const float depth = ((z - c.zNear) / c.D - 0.1f) / 0.8f;
    const float pan   = x / (dusi::space::kPanReach * c.R);
    setValue ("pan",   juce::jlimit (-100.0f, 100.0f, pan * 100.0f));
    setValue ("depth", juce::jlimit (0.0f, 100.0f, depth * 100.0f));
}

void SpaceView::dragWidth (juce::Point<float> pos)
{
    const auto c = makeCamera();
    const float zs = sourceZ (c);
    const float x = (pos.x - c.cx) * zs / c.fx;
    const float hw = std::fabs (x - sourceX (c));
    setValue ("width", juce::jlimit (0.0f, 150.0f, hw / (dusi::space::kWidthReach * c.R) * 100.0f));
}

void SpaceView::mouseDown (const juce::MouseEvent& e)
{
    if (! contentArea().contains (e.position))
        return;

    if (hitHandle (e.position) != 0)
    {
        drag = Drag::width;
        beginGesture ({ "width" });
        dragWidth (e.position);
    }
    else
    {
        drag = Drag::position;
        beginGesture ({ "pan", "depth" });
        dragPosition (e.position);
    }
    repaint();
}

void SpaceView::mouseDrag (const juce::MouseEvent& e)
{
    if (drag == Drag::position) dragPosition (e.position);
    else if (drag == Drag::width) dragWidth (e.position);
    repaint();
}

void SpaceView::mouseUp (const juce::MouseEvent&)
{
    drag = Drag::none;
    endGestures();
    repaint();
}

void SpaceView::mouseDoubleClick (const juce::MouseEvent&)
{
    beginGesture ({ "pan", "depth", "width" });
    setValue ("pan", 0.0f);
    setValue ("depth", 35.0f);
    setValue ("width", 100.0f);
    endGestures();
}

void SpaceView::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    beginGesture ({ "width" });
    setValue ("width", juce::jlimit (0.0f, 150.0f, value ("width") + wheel.deltaY * 60.0f));
    endGestures();
}

void SpaceView::mouseMove (const juce::MouseEvent& e)
{
    const int h = hitHandle (e.position);
    if (h != hoverHandle)
    {
        hoverHandle = h;
        repaint();
    }
    setMouseCursor (h != 0 ? juce::MouseCursor::LeftRightResizeCursor
                           : (contentArea().contains (e.position) ? juce::MouseCursor::DraggingHandCursor
                                                                  : juce::MouseCursor::NormalCursor));
}

void SpaceView::timerCallback()
{
    const bool eco = processor.eco.load();
    const int hz = eco ? 8 : 15;
    if (getTimerInterval() != 1000 / hz) startTimerHz (hz);
    if (! isShowing()) return;

    level = eco ? 0.0f : processor.uiLevel.load();
    ringEnergy = eco ? 0.0f : juce::jmax (juce::jlimit (0.0f, 1.0f, level * 3.0f), ringEnergy * 0.9f);
    if (ringEnergy < 0.01f) ringEnergy = 0.0f;
    const float period = juce::jlimit (0.6f, 3.0f, processor.uiDecaySec.load() * 0.5f);
    ringPhase = std::fmod (ringPhase + (1.0f / (float) hz) / period, 1.0f);

    // prekreslit jen kdyz se neco zmenilo nebo bezi animace
    const bool rings = ringEnergy > 0.0f && value ("reverbOn") > 0.5f;
    if (rings || sceneKey() != paintedKey || std::abs (level - paintedLevel) > 0.03f)
        repaint();
}
