// DusiX DeVerb - zakladni stavebni kameny DSP (bez zavislosti na JUCE).
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <vector>

namespace dusi
{
constexpr float kPi = 3.14159265358979f;

inline float dbToGain (float db) { return std::pow (10.0f, db / 20.0f); }

//------------------------------------------------------------------------------
// Geometrie prostoru - sdili ji DSP (early reflections, bezpecna sirka) i 3D pohled.
namespace space
{
    constexpr float kPanReach   = 0.8f;   // zvuk muze dojit do 80 % k bocni stene
    constexpr float kWidthReach = 0.55f;  // sirka 100 % = 55 % polosirky mistnosti

    // Nejvetsi bezpecna sirka (1.0 = 100 %), aby se efekt "nevylil" za steny mistnosti.
    inline float safeWidthLimit (float pan)
    {
        const float room = (1.0f - kPanReach * std::fabs (pan)) / kWidthReach;
        return std::clamp (room, 0.2f, 1.0f);
    }

    // Mistnost ve "svetovych" jednotkach 3D pohledu. size = 0.3 .. 1.5
    struct Room
    {
        float R, D, zNear = 1.0f;     // polosirka, hloubka, predni stena
        float metersPerUnit;          // prevod na metry pro vypocet odrazu
        float heightM;                // vyska stropu v metrech

        static Room fromSize (float size)
        {
            const float n = std::clamp ((size - 0.3f) / 1.2f, 0.0f, 1.0f);
            Room r;
            r.R = 0.8f + 0.3f * n;
            r.D = 1.6f + 2.2f * n;
            const float widthM = 5.0f + 25.0f * n;     // 5 m .. 30 m
            r.metersPerUnit = widthM / (2.0f * r.R);
            r.heightM = 3.0f + 9.0f * n;
            return r;
        }
        float zFar() const                    { return zNear + D; }
        float sourceX (float pan) const       { return std::clamp (pan, -1.0f, 1.0f) * kPanReach * R; }
        float sourceZ (float depth) const     { return zNear + D * (0.1f + 0.8f * std::clamp (depth, 0.0f, 1.0f)); }
    };
}

//------------------------------------------------------------------------------
// One-pole vyhlazeni (parametry bez lupani).
struct Smooth
{
    void prepare (double sampleRate, float timeMs)
    {
        coef = std::exp (-1.0f / (0.001f * timeMs * (float) sampleRate));
    }
    void snap (float v)            { z = target = v; }
    void set (float v)             { target = v; }
    float next()                   { z = target + coef * (z - target); return z; }
    bool settled() const           { return std::fabs (z - target) < 1.0e-5f * (1.0f + std::fabs (target)); }

    float z = 0.0f, target = 0.0f, coef = 0.0f;
};

//------------------------------------------------------------------------------
// Kruhovy buffer s linearni interpolaci. d = 1 vraci naposledy zapsany vzorek.
class DelayLine
{
public:
    void prepare (int maxSamples)
    {
        int size = 16;
        while (size < maxSamples + 4) size <<= 1;
        buf.assign ((size_t) size, 0.0f);
        mask = size - 1;
        w = 0;
    }
    void clear()                   { std::fill (buf.begin(), buf.end(), 0.0f); }
    void write (float x)           { buf[(size_t) w] = x; w = (w + 1) & mask; }
    int capacity() const           { return mask - 2; }

    // Cela cisla cte presne (bez ztraty vysek), jinak linearni interpolace.
    float read (float d) const
    {
        d = std::clamp (d, 1.0f, (float) (mask - 2));
        const int   di   = (int) d;
        const float frac = d - (float) di;
        const float a = buf[(size_t) ((w - di) & mask)];
        const float b = buf[(size_t) ((w - di - 1) & mask)];
        return a + frac * (b - a);
    }

    // Cteni o cely pocet vzorku (nejrychlejsi)
    float readInt (int d) const { return buf[(size_t) ((w - std::clamp (d, 1, mask - 2)) & mask)]; }

    // Kubicka (Hermite) interpolace - pro modulovane linky ve smycce, mene tlumi vysky
    float readCubic (float d) const
    {
        d = std::clamp (d, 2.0f, (float) (mask - 3));
        const int   i = (int) d;
        const float f = d - (float) i;
        const float xm1 = at (i - 1), x0 = at (i), x1 = at (i + 1), x2 = at (i + 2);
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

private:
    float at (int delay) const { return buf[(size_t) ((w - delay) & mask)]; }
    std::vector<float> buf;
    int mask = 0, w = 0;
};

// Schroederuv allpass - zahustuje odrazy
struct Allpass
{
    DelayLine line;
    float process (float x, float delay, float g)
    {
        const float d = line.read (delay);
        const float v = x - g * d;
        line.write (v);
        return d + g * v;
    }
    float processCubic (float x, float delay, float g)
    {
        const float d = line.readCubic (delay);
        const float v = x - g * d;
        line.write (v);
        return d + g * v;
    }
};

//------------------------------------------------------------------------------
// Biquad (RBJ cookbook) - koeficienty zvlast od stavu, aby sly sdilet mezi kanaly
// a pouzit i pro kresleni krivky v GUI.
struct BiquadCoefs
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;

    static BiquadCoefs make (double b0, double b1, double b2, double a0, double a1, double a2)
    {
        return { (float) (b0 / a0), (float) (b1 / a0), (float) (b2 / a0), (float) (a1 / a0), (float) (a2 / a0) };
    }

    static double omega (float f, float sr) { return 2.0 * 3.14159265358979 * std::clamp ((double) f, 5.0, 0.49 * sr) / sr; }

    static BiquadCoefs lowPass (float f, float q, float sr)
    {
        const double w = omega (f, sr), c = std::cos (w), al = std::sin (w) / (2.0 * q);
        return make ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
    static BiquadCoefs highPass (float f, float q, float sr)
    {
        const double w = omega (f, sr), c = std::cos (w), al = std::sin (w) / (2.0 * q);
        return make ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
    static BiquadCoefs bell (float f, float q, float gainDb, float sr)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = omega (f, sr), c = std::cos (w), al = std::sin (w) / (2.0 * q);
        return make (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    static BiquadCoefs lowShelf (float f, float q, float gainDb, float sr)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = omega (f, sr), c = std::cos (w);
        const double al = std::sin (w) / (2.0 * q), sA = 2.0 * std::sqrt (A) * al;
        return make (A * ((A + 1) - (A - 1) * c + sA), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sA),
                     (A + 1) + (A - 1) * c + sA, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sA);
    }
    static BiquadCoefs highShelf (float f, float q, float gainDb, float sr)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = omega (f, sr), c = std::cos (w);
        const double al = std::sin (w) / (2.0 * q), sA = 2.0 * std::sqrt (A) * al;
        return make (A * ((A + 1) + (A - 1) * c + sA), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sA),
                     (A + 1) - (A - 1) * c + sA, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sA);
    }
    static BiquadCoefs notch (float f, float q, float sr)
    {
        const double w = omega (f, sr), c = std::cos (w), al = std::sin (w) / (2.0 * q);
        return make (1, -2 * c, 1, 1 + al, -2 * c, 1 - al);
    }

    // Zesileni v dB na dane frekvenci (pro kresleni a kontrolu stability)
    float magnitudeDb (float f, float sr) const
    {
        const double w = 2.0 * 3.14159265358979 * f / sr;
        const std::complex<double> z1 = std::polar (1.0, -w), z2 = z1 * z1;
        const auto num = (double) b0 + (double) b1 * z1 + (double) b2 * z2;
        const auto den = 1.0 + (double) a1 * z1 + (double) a2 * z2;
        return (float) (20.0 * std::log10 (std::max (1.0e-12, std::abs (num / den))));
    }
};

struct BiquadState
{
    float z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    float process (const BiquadCoefs& c, float x)
    {
        const float y = c.b0 * x + z1;
        z1 = c.b1 * x - c.a1 * y + z2;
        z2 = c.b2 * x - c.a2 * y;
        return y;
    }
};

// Jednoduchy biquad horni propust pro "bass mono" (koeficienty + stav dohromady)
struct HighPass
{
    void set (float freq, float sampleRate) { c = BiquadCoefs::highPass (freq, 0.7071f, sampleRate); }
    void reset() { s.reset(); }
    float process (float x) { return s.process (c, x); }
    BiquadCoefs c;
    BiquadState s;
};

//------------------------------------------------------------------------------
// Mix: dry zustava na 100 % do pulky knofliku, wet je na 100 % od pulky.
inline void mixGains (float mix, float& dry, float& wet)
{
    mix = std::clamp (mix, 0.0f, 1.0f);
    dry = std::min (1.0f, 2.0f * (1.0f - mix));
    wet = std::min (1.0f, 2.0f * mix);
}

// Hloubka -> fyzikalni parametry (cim dal, tim tissi, tmavsi, vic dozvuku, kratsi pre-delay)
struct DepthModel
{
    explicit DepthModel (float d)
    {
        d = std::clamp (d, 0.0f, 1.0f);
        dryGain   = std::pow (10.0f, -8.0f * d / 20.0f);          // az -8 dB
        dryCutoff = 20000.0f * std::pow (0.3f, d);                // az ~6 kHz
        wetScale  = std::pow (2.0f, (d - 0.35f) * 1.5f);          // 0.7x .. 2x
        preScale  = 1.3f - 0.9f * d;                              // 1.3x .. 0.4x
    }
    float dryGain, dryCutoff, wetScale, preScale;
};
} // namespace dusi
