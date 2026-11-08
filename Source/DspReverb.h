// DusiX DeVerb - reverb: typy Plate / Room / Chamber / Hall / Spring, early reflections z 3D mistnosti
// a "decay EQ" (delka dozvuku po frekvencich, jako ve FabFilter Pro-R).
#pragma once

#include "DspCore.h"
#include "DspParams.h"
#include "DspFx.h"

namespace dusi
{
//==============================================================================
// Decay EQ
// Kazde pasmo nasobi delku dozvuku (RT60) na sve frekvenci. Zisk filtru v ceste
// dlouhe tau sekund je  tau * (60/RT - 60/(RT*m))  dB, takze smycka utlumi danou
// frekvenci presne tak, aby dozvuk trval RT*m.
class DecayEqDesign
{
public:
    static constexpr float kMaxRt = 30.0f;

    static BiquadCoefs designBand (int band, const DecayBand& b, float gainDb, float sr)
    {
        gainDb = std::clamp (gainDb, -60.0f, 24.0f);
        switch (band)
        {
            case 0:  return BiquadCoefs::lowShelf  (b.freq, 0.7f, gainDb, sr);
            case 3:  return BiquadCoefs::highShelf (b.freq, 0.7f, gainDb, sr);
            default: return BiquadCoefs::bell (b.freq, std::clamp (b.q, 0.2f, 6.0f), gainDb, sr);
        }
    }

    // Vraci true, kdyz se navrh zmenil (jinak se nic nepocita - setri CPU)
    bool compute (const Params& p, float rt, float sampleRate)
    {
        bool same = juce_equal (sampleRate, sr) && p.decayEqOn == on && juce_equal (rt, lastRt);
        for (int k = 0; k < kNumDecayBands && same; ++k)
            same = juce_equal (p.decayEq[(size_t) k].freq, bands[(size_t) k].freq) && juce_equal (p.decayEq[(size_t) k].mult, bands[(size_t) k].mult)
                && juce_equal (p.decayEq[(size_t) k].q, bands[(size_t) k].q);
        if (same && valid) return false;
        valid = true;
        lastRt = rt;
        ++version;
        sr = sampleRate;
        on = p.decayEqOn;
        bands = p.decayEq;
        if (! on) return true;

        rt = std::clamp (rt, 0.05f, kMaxRt);
        for (int k = 0; k < kNumDecayBands; ++k)
        {
            const float m = std::clamp (bands[(size_t) k].mult, 0.25f, 4.0f);
            const float rtEff = std::clamp (rt * m, 0.05f, kMaxRt);
            eDb[(size_t) k] = 60.0f / rt - 60.0f / rtEff;   // dB za sekundu navic
        }

        // Pojistka: prekryvajici se zesileni nesmi celkove prodlouzit dozvuk pres 30 s
        const float allowed = std::max (0.0f, (60.0f / rt - 60.0f / kMaxRt) * 0.9f);
        auto worst = [this] (float scalePos)
        {
            float mx = -1.0e9f;
            for (int i = 0; i < 40; ++i)
            {
                const float f = 20.0f * std::pow (1000.0f, (float) i / 39.0f);
                float s = 0.0f;
                for (int k = 0; k < kNumDecayBands; ++k)
                {
                    const float e = eDb[(size_t) k] > 0 ? eDb[(size_t) k] * scalePos : eDb[(size_t) k];
                    s += designBand (k, bands[(size_t) k], e * kTauRef, sr).magnitudeDb (f, sr);
                }
                mx = std::max (mx, s / kTauRef);
            }
            return mx;
        };
        posScale = 1.0f;
        if (worst (1.0f) > allowed)
        {
            float lo = 0.0f, hi = 1.0f;
            for (int it = 0; it < 12; ++it)
            {
                const float mid = 0.5f * (lo + hi);
                (worst (mid) > allowed ? hi : lo) = mid;
            }
            posScale = lo;
        }
        return true;
    }

    int getVersion() const { return version; }

    bool isOn() const { return on; }

    // Koeficienty pro cestu dlouhou tauSec
    void designPath (float tauSec, struct DecayEqPath& path) const;

    // Pro GUI: kolikrat delsi/kratsi je dozvuk na frekvenci f
    float multiplierAt (float f, float rt) const
    {
        if (! on) return 1.0f;
        rt = std::clamp (rt, 0.05f, kMaxRt);
        float s = 0.0f;
        for (int k = 0; k < kNumDecayBands; ++k)
        {
            const float e = eDb[(size_t) k] > 0 ? eDb[(size_t) k] * posScale : eDb[(size_t) k];
            s += designBand (k, bands[(size_t) k], e * kTauRef, sr).magnitudeDb (f, sr);
        }
        const float perSec = 60.0f / rt - s / kTauRef;
        return perSec > 1.0e-3f ? std::clamp ((60.0f / perSec) / rt, 0.1f, 10.0f) : 10.0f;
    }

private:
    static bool juce_equal (float a, float b) { return std::fabs (a - b) <= 1.0e-6f * (1.0f + std::fabs (a)); }
    static constexpr float kTauRef = 0.05f;
    bool on = false, valid = false;
    int version = 0;
    float lastRt = -1.0f;
    std::array<DecayBand, kNumDecayBands> bands {};
    std::array<float, kNumDecayBands> eDb {};
    float posScale = 1.0f, sr = 48000.0f;
};

// Sada 4 biquadu decay EQ v jedne ceste
struct DecayEqPath
{
    std::array<BiquadCoefs, kNumDecayBands> c {};
    std::array<BiquadState, kNumDecayBands> s {};
    unsigned activeMask = 0xF;   // pasma s nasobkem x1 se preskakuji (setri CPU)
    void reset() { for (auto& x : s) x.reset(); }
    float process (float x)
    {
        for (int k = 0; k < kNumDecayBands; ++k)
            if (activeMask & (1u << k)) x = s[(size_t) k].process (c[(size_t) k], x);
        return x;
    }
};

inline void DecayEqDesign::designPath (float tauSec, DecayEqPath& path) const
{
    path.activeMask = 0;
    for (int k = 0; k < kNumDecayBands; ++k)
    {
        const float e = eDb[(size_t) k] > 0 ? eDb[(size_t) k] * posScale : eDb[(size_t) k];
        if (std::fabs (e) < 1.0e-4f) continue;   // nasobek x1 = pasmo nic nedela
        path.c[(size_t) k] = designBand (k, bands[(size_t) k], e * tauSec, sr);
        path.activeMask |= 1u << k;
    }
}

// Spolecne nastaveni pro "pozdni" dozvuk
struct LateConfig
{
    float size = 1.0f, rt = 2.0f, dampCoef = 0.0f, mod = 0.3f, highCut = 8000.0f;
    bool freeze = false;   // nekonecny dozvuk: bez ztrat, bez tlumeni
    const DecayEqDesign* eq = nullptr;
};

//==============================================================================
// PLATE - Jon Dattorro, "Effect Design Part 1" (1997)
class PlateLate
{
public:
    static constexpr float kMaxSize = 1.5f;

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        srScale = sr / 29761.0f;
        const float m = srScale * kMaxSize;
        const float inLen[4] = { 142, 107, 379, 277 };
        for (int i = 0; i < 4; ++i) inAp[i].line.prepare ((int) (inLen[i] * srScale) + 2);
        ap1[0].line.prepare ((int) ((672 + 40) * m));
        ap1[1].line.prepare ((int) ((908 + 40) * m));
        d1[0].prepare ((int) (4453 * m));
        d1[1].prepare ((int) (4217 * m));
        ap2[0].line.prepare ((int) (1800 * m));
        ap2[1].line.prepare ((int) (2656 * m));
        d2[0].prepare ((int) (3720 * m));
        d2[1].prepare ((int) (3163 * m));
        size.prepare (sampleRate, 250.0f);
        bwCoef = std::exp (-2.0f * kPi * 0.45f * 0.9f);
        first = true;
        reset();
    }

    void reset()
    {
        for (auto& a : inAp) a.line.clear();
        for (int i = 0; i < 2; ++i)
        {
            ap1[i].line.clear(); ap2[i].line.clear(); d1[i].clear(); d2[i].clear();
            damp[i] = 0.0f;
            eq[i].reset();
        }
        bw = 0.0f;
        lfoPhase = 0.0f;
    }

    void configure (const LateConfig& c)
    {
        size.set (std::clamp (c.size, 0.3f, kMaxSize));
        if (first) { size.snap (size.target); first = false; }
        const float s = size.z;
        const float loopSec = 0.725f * s;
        decay = c.freeze ? 0.99999f : std::min (0.9995f, std::pow (10.0f, -3.0f * loopSec / (4.0f * std::clamp (c.rt, 0.2f, 20.0f))));
        dampCoef = c.freeze ? 0.0f : c.dampCoef;
        modDepth = std::clamp (c.mod, 0.0f, 1.0f) * 16.0f * srScale;
        eqOn = c.eq != nullptr && c.eq->isOn();
        if (eqOn && (c.eq->getVersion() != eqVersion || ! size.settled() || std::fabs (s - eqSize) > 1.0e-6f))
        {
            eqVersion = c.eq->getVersion();
            eqSize = s;
            // kazda polovina tanku je jedna cesta smyckou
            c.eq->designPath ((672 + 4453 + 1800 + 3720) * srScale * s / sr, eq[0]);
            c.eq->designPath ((908 + 4217 + 2656 + 3163) * srScale * s / sr, eq[1]);
        }
    }

    void process (float x, float& yl, float& yr)
    {
        const float k = srScale * size.next();
        const bool exact = size.settled();
        auto L = [exact] (float len) { return exact ? std::round (len) : len; };   // cele vzorky = bez ztraty vysek
        bw = x + bwCoef * (bw - x);
        x = bw;
        x = inAp[0].process (x, 142 * srScale, 0.75f);
        x = inAp[1].process (x, 107 * srScale, 0.75f);
        x = inAp[2].process (x, 379 * srScale, 0.625f);
        x = inAp[3].process (x, 277 * srScale, 0.625f);

        lfoPhase += 2.0f * kPi * 0.8f / sr;
        if (lfoPhase > 2.0f * kPi) lfoPhase -= 2.0f * kPi;
        const float lfoA = modDepth * std::sin (lfoPhase);
        const float lfoB = modDepth * std::cos (lfoPhase);

        const float fbL = decay * d2[0].read (L (3720 * k));
        const float fbR = decay * d2[1].read (L (3163 * k));
        tankHalf (0, x + fbR, 672 * k + lfoA + modDepth + 2.0f, L (4453 * k), L (1800 * k));
        tankHalf (1, x + fbL, 908 * k + lfoB + modDepth + 2.0f, L (4217 * k), L (2656 * k));

        yl = d1[1].read (266 * k) + d1[1].read (2974 * k) - ap2[1].line.read (1913 * k)
           + d2[1].read (1996 * k) - d1[0].read (1990 * k) - ap2[0].line.read (187 * k)
           - d2[0].read (1066 * k);
        yr = d1[0].read (353 * k) + d1[0].read (3627 * k) - ap2[0].line.read (1228 * k)
           + d2[0].read (2673 * k) - d1[1].read (2111 * k) - ap2[1].line.read (335 * k)
           - d2[1].read (121 * k);
        yl *= 0.6f;
        yr *= 0.6f;
    }

private:
    void tankHalf (int i, float x, float ap1Len, float d1Len, float ap2Len)
    {
        x = ap1[i].processCubic (x, ap1Len, -0.7f);
        const float t = d1[i].read (d1Len);
        d1[i].write (x);
        damp[i] = t + dampCoef * (damp[i] - t);
        float v = damp[i];
        if (eqOn) v = eq[i].process (v);
        x = ap2[i].process (decay * v, ap2Len, 0.5f);
        d2[i].write (x);
    }

    DelayLine d1[2], d2[2];
    Allpass inAp[4], ap1[2], ap2[2];
    DecayEqPath eq[2];
    Smooth size;
    float sr = 48000.0f, srScale = 1.0f, decay = 0.5f, dampCoef = 0.0f, bwCoef = 0.0f, modDepth = 0.0f;
    float damp[2] {}, bw = 0.0f, lfoPhase = 0.0f, eqSize = 0.0f;
    int eqVersion = -1;
    bool eqOn = false, first = true;
};

//==============================================================================
// ROOM / CHAMBER / HALL - siť 8 zpozdovacich linek (FDN) s Householderovou maticí
class FdnLate
{
public:
    static constexpr int N = 8;

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        for (auto& l : lines) l.prepare ((int) (0.17f * 1.3f * sr) + 64);
        for (auto& a : diff) a.line.prepare ((int) (0.013f * sr) + 4);
        size.prepare (sampleRate, 250.0f);
        first = true;
        reset();
    }

    void setType (ReverbType t)
    {
        static const float room[N]    = { 11.3f, 13.9f, 16.7f, 19.3f, 22.1f, 24.7f, 27.9f, 31.3f };
        static const float chamber[N] = { 19.7f, 23.9f, 28.3f, 32.9f, 37.3f, 41.9f, 46.7f, 51.1f };
        static const float hall[N]    = { 37.1f, 44.3f, 52.9f, 59.3f, 67.1f, 76.3f, 84.7f, 97.1f };
        static const float dRoom[4] = { 1.1f, 1.7f, 2.9f, 3.7f }, dCham[4] = { 1.5f, 2.3f, 4.1f, 5.3f }, dHall[4] = { 3.1f, 4.7f, 7.9f, 11.3f };
        const float* base = t == ReverbType::room ? room : (t == ReverbType::hall ? hall : chamber);
        const float* dif  = t == ReverbType::room ? dRoom : (t == ReverbType::hall ? dHall : dCham);
        for (int i = 0; i < N; ++i) baseLen[i] = base[i] * 0.001f * sr;
        for (int i = 0; i < 4; ++i) diffLen[i] = dif[i] * 0.001f * sr;
        diffG = t == ReverbType::hall ? 0.72f : 0.62f;
        eqVersion = -1;
        outScale = t == ReverbType::room ? 0.385f : (t == ReverbType::hall ? 0.65f : 0.475f);
        reset();
    }

    void reset()
    {
        for (auto& l : lines) l.clear();
        for (auto& a : diff) a.line.clear();
        for (auto& e : eq) e.reset();
        for (auto& d : damp) d = 0.0f;
        lfoPhase = 0.0f;
    }

    void configure (const LateConfig& c)
    {
        size.set (0.45f + 0.55f * std::clamp (c.size, 0.3f, 1.5f));
        if (first) { size.snap (size.target); first = false; }
        const float sf = size.z;
        const float rt = std::clamp (c.rt, 0.2f, 20.0f);
        modDepth = std::clamp (c.mod, 0.0f, 1.0f) * 10.0f * (sr / 48000.0f);
        eqOn = c.eq != nullptr && c.eq->isOn();
        const bool redesign = eqOn && (c.eq->getVersion() != eqVersion || std::fabs (sf - eqSize) > 1.0e-6f);
        if (redesign) { eqVersion = c.eq->getVersion(); eqSize = sf; }
        for (int i = 0; i < N; ++i)
        {
            const float tau = baseLen[i] * sf / sr;
            gain[i] = c.freeze ? 0.99999f : std::pow (10.0f, -3.0f * tau / rt);
            // tlumeni vysek prepoctene na delku linky (kratka linka = vic obehu za sekundu)
            dampCoef[i] = c.freeze ? 0.0f : c.dampCoef * std::min (1.0f, tau / 0.33f);
            if (redesign) c.eq->designPath (tau, eq[i]);
        }
    }

    void process (float x, float& yl, float& yr)
    {
        const float sf = size.next();
        for (int i = 0; i < 4; ++i) x = diff[i].process (x, diffLen[i], diffG);

        lfoPhase += 2.0f * kPi * 0.7f / sr;
        if (lfoPhase > 2.0f * kPi) lfoPhase -= 2.0f * kPi;

        const bool exact = size.settled();
        float v[N], sum = 0.0f;
        for (int i = 0; i < N; ++i)
        {
            float d = baseLen[i] * sf;
            float o;
            if ((i & 1) == 0 && modDepth > 0.0f)
                o = lines[i].readCubic (d + modDepth * (1.0f + std::sin (lfoPhase + 1.3f * (float) i)));
            else
                o = lines[i].read (exact ? std::round (d) : d);
            damp[i] = o + dampCoef[i] * (damp[i] - o);
            float s = damp[i];
            if (eqOn) s = eq[i].process (s);
            v[i] = s * gain[i];
            sum += v[i];
        }
        const float h = sum * (2.0f / N);
        static const float inSign[N] = { 1, -1, 1, 1, -1, 1, -1, -1 };
        static const float a[N] = { 1, -1, 1, -1, 1, -1, 1, -1 };
        static const float b[N] = { 1, 1, -1, -1, 1, 1, -1, -1 };
        yl = yr = 0.0f;
        for (int i = 0; i < N; ++i)
        {
            lines[i].write (v[i] - h + 0.5f * inSign[i] * x);
            yl += a[i] * v[i];
            yr += b[i] * v[i];
        }
        yl *= outScale;
        yr *= outScale;
    }

private:
    DelayLine lines[N];
    Allpass diff[4];
    DecayEqPath eq[N];
    Smooth size;
    float baseLen[N] {}, gain[N] {}, damp[N] {}, dampCoef[N] {}, diffLen[4] {};
    float sr = 48000.0f, modDepth = 0.0f, diffG = 0.65f, outScale = 0.6f, lfoPhase = 0.0f, eqSize = 0.0f;
    int eqVersion = -1;
    bool eqOn = false, first = true;
};

//==============================================================================
// SPRING - dve "pruziny": smycka s disperznimi allpassy (typicke "boing" cvrlikani)
class SpringLate
{
public:
    static constexpr int kLoopAp = 12, kInAp = 24;

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        for (auto& l : loops) l.prepare ((int) (0.06f * sr) + 16);
        size.prepare (sampleRate, 250.0f);
        hpCoef = std::exp (-2.0f * kPi * 120.0f / sr);
        first = true;
        reset();
    }

    void reset()
    {
        for (auto& l : loops) l.clear();
        for (auto& z : apIn) z.fill (0.0f);
        for (auto& z : apLoop) z.fill (0.0f);
        fb[0] = fb[1] = lp[0] = lp[1] = hp[0] = hp[1] = 0.0f;
        for (auto& e : eq) e.reset();
    }

    void configure (const LateConfig& c)
    {
        size.set (0.6f + 0.4f * std::clamp (c.size, 0.3f, 1.5f));
        if (first) { size.snap (size.target); first = false; }
        const float sf = size.z;
        const float rt = std::clamp (c.rt, 0.2f, 20.0f);
        lpCoef = std::exp (-2.0f * kPi * std::min (c.highCut, 5500.0f) / sr);
        frozen = c.freeze;
        eqOn = c.eq != nullptr && c.eq->isOn();
        // ztrata LP+HP filtru na 1 kHz za jeden obeh - vyrovnat, aby decay sedel
        const float w1 = 2.0f * kPi * 1000.0f / sr;
        auto onePoleLp = [w1] (float a) { return (1.0f - a) / std::sqrt (1.0f - 2.0f * a * std::cos (w1) + a * a); };
        const float lpMag = onePoleLp (lpCoef);
        const float hpMag = hpCoef * 2.0f * std::sin (0.5f * w1) / std::sqrt (1.0f - 2.0f * hpCoef * std::cos (w1) + hpCoef * hpCoef);
        const float comp = 1.0f / std::max (0.5f, lpMag * hpMag);
        const bool redesign = eqOn && (c.eq->getVersion() != eqVersion || std::fabs (sf - eqSize) > 1.0e-6f);
        if (redesign) { eqVersion = c.eq->getVersion(); eqSize = sf; }
        for (int i = 0; i < 2; ++i)
        {
            const float tau = kLoopMs[i] * 0.001f * sf;
            g[i] = c.freeze ? 0.9995f : std::min (0.999f, std::pow (10.0f, -3.0f * tau / rt) * comp);
            if (redesign) c.eq->designPath (tau, eq[i]);
        }
    }

    void process (float x, float& yl, float& yr)
    {
        const float sf = size.next();
        float y[2];
        for (int i = 0; i < 2; ++i)
        {
            // disperze na vstupu: vysky dorazi drive nez basy
            float s = x;
            for (auto& z : apIn[(size_t) i]) s = allpass1 (s, z, 0.62f);
            s = s * 0.6f + g[i] * fb[i];
            for (auto& z : apLoop[(size_t) i]) s = allpass1 (s, z, 0.68f);
            const float len = kLoopMs[i] * 0.001f * sr * sf;
            const float o = loops[i].read (size.settled() ? std::round (len) : len);
            loops[i].write (s);
            float v = o;
            if (! frozen)   // pri freeze bez filtru ve smycce = dozvuk nezanikne
            {
                lp[i] = o + lpCoef * (lp[i] - o);
                hp[i] = lp[i] + hpCoef * (hp[i] - lp[i]);
                v = lp[i] - hp[i];
            }
            if (eqOn) v = eq[i].process (v);
            fb[i] = v;
            y[i] = v;
        }
        yl = (y[0] + 0.25f * y[1]) * 1.3f;
        yr = (y[1] + 0.25f * y[0]) * 1.3f;
    }

private:
    static float allpass1 (float x, float& z, float a)
    {
        const float yv = a * x + z;
        z = x - a * yv;
        return yv;
    }

    static constexpr float kLoopMs[2] = { 29.0f, 35.0f };
    DelayLine loops[2];
    std::array<std::array<float, kInAp>, 2> apIn {};
    std::array<std::array<float, kLoopAp>, 2> apLoop {};
    DecayEqPath eq[2];
    Smooth size;
    float g[2] {}, fb[2] {}, lp[2] {}, hp[2] {};
    float sr = 48000.0f, lpCoef = 0.0f, hpCoef = 0.0f, eqSize = 0.0f;
    int eqVersion = -1;
    bool eqOn = false, first = true, frozen = false;
};

//==============================================================================
// EARLY REFLECTIONS - zrcadlove zdroje (image-source) v 3D mistnosti z pohledu PROSTOR.
// Kdyz zvuk posunes, zmeni se casy i strany prvnich odrazu.
class EarlyReflections
{
public:
    static constexpr int kTaps = 16;      // 6 odrazu 1. radu + 10 nejsilnejsich 2. radu
    static constexpr int kRamp = 2048;    // zmena polohy se prolne za ~40 ms

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        line.prepare ((int) (0.45f * sr));
        first = true;
        reset();
    }

    void reset()
    {
        line.clear();
        lp1[0] = lp1[1] = lp2[0] = lp2[1] = 0.0f;
    }

    void setGeometry (float pan, float depth, float size, float highCut)
    {
        // geometrie se prepocita jen pri zmene (pohyb bodu v 3D, size, high cut)
        auto same = [] (float a, float b) { return std::fabs (a - b) < 1.0e-7f; };
        if (! first && same (pan, lastPan) && same (depth, lastDepth) && same (size, lastSize) && same (highCut, lastHighCut))
            return;
        lastPan = pan; lastDepth = depth; lastSize = size; lastHighCut = highCut;

        const auto room = space::Room::fromSize (size);
        const float k = room.metersPerUnit;
        const float W = 2.0f * room.R * k, Dm = room.D * k, H = room.heightM;
        const float sx = room.sourceX (pan) * k + 0.5f * W;
        const float sy = (room.sourceZ (depth) - room.zNear) * k;
        const float sz = 1.6f;
        const float lx = 0.5f * W, ly = 0.05f * Dm, lz = 1.5f;
        const float d0 = std::max (0.3f, std::sqrt ((sx - lx) * (sx - lx) + (sy - ly) * (sy - ly) + (sz - lz) * (sz - lz)));

        auto image = [] (int n, float s, float L) { return (n % 2 == 0) ? (float) n * L + s : (float) (n + 1) * L - s; };

        struct Cand { float g, del, pan; bool order2; };
        std::array<Cand, 24> cand {};
        int nc = 0;
        for (int nx = -2; nx <= 2; ++nx)
            for (int ny = -2; ny <= 2; ++ny)
                for (int nz = -2; nz <= 2; ++nz)
                {
                    const int order = std::abs (nx) + std::abs (ny) + std::abs (nz);
                    if (order < 1 || order > 2 || nc >= 24) continue;
                    const float ix = image (nx, sx, W), iy = image (ny, sy, Dm), iz = image (nz, sz, H);
                    const float dx = ix - lx, dy = iy - ly, dz = iz - lz;
                    const float d = std::sqrt (dx * dx + dy * dy + dz * dz);
                    const float absorb = std::pow (0.8f, (float) (std::abs (nx) + std::abs (ny))) * std::pow (0.65f, (float) std::abs (nz));
                    cand[(size_t) nc++] = { d0 / d * absorb, std::clamp ((d - d0) / 343.0f * sr, 1.0f, 0.44f * sr),
                                            std::sin (std::atan2 (dx, dy)), order == 2 };
                }
        // vsechny odrazy 1. radu + nejsilnejsi 2. radu
        std::stable_sort (cand.begin(), cand.begin() + nc, [] (const Cand& x, const Cand& y)
                          { return x.order2 != y.order2 ? ! x.order2 : x.g > y.g; });
        numTaps = std::min (kTaps, nc);

        float energy = 0.0f;
        for (int i = 0; i < numTaps; ++i) energy += cand[(size_t) i].g * cand[(size_t) i].g;
        const float norm = energy > 0.0f ? std::min (1.5f, 0.9f / std::sqrt (energy)) : 0.0f;
        for (int i = 0; i < numTaps; ++i)
        {
            auto& tp = taps[(size_t) i];
            const auto& c = cand[(size_t) i];
            const float p = std::clamp (c.pan, -1.0f, 1.0f);
            tp.order2 = c.order2;
            tp.tDelay = c.del;
            tp.tGl = c.g * norm * std::sqrt (0.5f * (1.0f - p));
            tp.tGr = c.g * norm * std::sqrt (0.5f * (1.0f + p));
            if (first) { tp.delay = tp.tDelay; tp.gl = tp.tGl; tp.gr = tp.tGr; tp.intDelay = (int) std::lround (tp.tDelay); }
            const float inv = 1.0f / (float) kRamp;
            tp.dDelay = (tp.tDelay - tp.delay) * inv;
            tp.dGl = (tp.tGl - tp.gl) * inv;
            tp.dGr = (tp.tGr - tp.gr) * inv;
        }
        rampLeft = first ? 0 : kRamp;
        first = false;
        c1 = std::exp (-2.0f * kPi * std::min (highCut * 1.5f, 16000.0f) / sr);
        c2 = std::exp (-2.0f * kPi * std::min (highCut, 9000.0f) / sr);
    }

    void process (float x, float& el, float& er)
    {
        line.write (x);
        float l1 = 0, r1 = 0, l2 = 0, r2 = 0;
        if (rampLeft > 0)   // plynuly presun odrazu (jen tesne po zmene polohy)
        {
            --rampLeft;
            for (int i = 0; i < numTaps; ++i)
            {
                auto& tp = taps[(size_t) i];
                tp.delay += tp.dDelay; tp.gl += tp.dGl; tp.gr += tp.dGr;
                if (rampLeft == 0) { tp.delay = tp.tDelay; tp.gl = tp.tGl; tp.gr = tp.tGr; }
                const float s = line.read (tp.delay);
                if (tp.order2) { l2 += s * tp.gl; r2 += s * tp.gr; } else { l1 += s * tp.gl; r1 += s * tp.gr; }
            }
            if (rampLeft == 0)
                for (auto& tp : taps) tp.intDelay = (int) std::lround (tp.delay);
        }
        else   // ustaleno: cele vzorky, zadna interpolace
        {
            for (int i = 0; i < numTaps; ++i)
            {
                const auto& tp = taps[(size_t) i];
                const float s = line.readInt (tp.intDelay);
                if (tp.order2) { l2 += s * tp.gl; r2 += s * tp.gr; } else { l1 += s * tp.gl; r1 += s * tp.gr; }
            }
        }
        lp1[0] = l1 + c1 * (lp1[0] - l1);  lp1[1] = r1 + c1 * (lp1[1] - r1);
        lp2[0] = l2 + c2 * (lp2[0] - l2);  lp2[1] = r2 + c2 * (lp2[1] - r2);
        el = lp1[0] + lp2[0];
        er = lp1[1] + lp2[1];
    }

private:
    struct Tap
    {
        float delay = 1, gl = 0, gr = 0, tDelay = 1, tGl = 0, tGr = 0, dDelay = 0, dGl = 0, dGr = 0;
        int intDelay = 1;
        bool order2 = false;
    };
    DelayLine line;
    std::array<Tap, kTaps> taps {};
    int numTaps = 0, rampLeft = 0;
    float sr = 48000.0f, c1 = 0.0f, c2 = 0.0f;
    float lastPan = -9.0f, lastDepth = -9.0f, lastSize = -9.0f, lastHighCut = -9.0f;
    float lp1[2] {}, lp2[2] {};
    bool first = true;
};

//==============================================================================
// Cely reverb modul: pre-delay -> (early reflections + pozdni dozvuk zvoleneho typu)
// -> low cut, bass mono, sirka, naklon -> mix s primym zvukem (hloubka) -> vypinac
class ReverbUnit
{
public:
    static constexpr float kMaxPreDelaySec = 0.65f;

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        preDelay.prepare ((int) ((kMaxPreDelaySec + 0.01f) * sr));
        plate.prepare (sampleRate);
        fdn.prepare (sampleRate);
        fdn.setType (ReverbType::room);
        spring.prepare (sampleRate);
        er.prepare (sampleRate);
        shifter.prepare (sampleRate);
        comp.prepare (sampleRate);
        freezeIn.prepare (sampleRate, 40.0f);
        shimmerGain.prepare (sampleRate, 50.0f);
        shimmerHpCoef = std::exp (-2.0f * kPi * 300.0f / sr);
        pre.prepare (sampleRate, 120.0f);
        dry.prepare (sampleRate, 30.0f);
        wet.prepare (sampleRate, 30.0f);
        widthS.prepare (sampleRate, 50.0f);
        lean.prepare (sampleRate, 50.0f);
        early.prepare (sampleRate, 50.0f);
        enable.prepare (sampleRate, 15.0f);
        fadeStep = 1.0f / (0.02f * sr);
        first = true;
        reset();
    }

    void reset()
    {
        preDelay.clear();
        plate.reset();
        fdn.reset();
        spring.reset();
        er.reset();
        shifter.reset();
        sideHp.reset();
        hp[0] = hp[1] = dryLp[0] = dryLp[1] = 0.0f;
        shimmerFb = shimmerHp = 0.0f;
    }

    void setParams (const Params& p, float autoGain)
    {
        const DepthModel depth (p.depth);
        const float rt = std::clamp (p.decaySec, 0.2f, 20.0f);

        if (first) { activeType = p.reverbType; if (isFdn (activeType)) fdn.setType (activeType); }
        pendingType = p.reverbType;

        const float preSec = std::clamp (p.preDelayMs * 0.001f * depth.preScale, 0.0f, kMaxPreDelaySec);
        pre.set (std::max (1.0f, preSec * sr));

        eqDesign.compute (p, rt, sr);
        LateConfig c;
        c.size = p.size;
        c.rt = rt;
        c.dampCoef = std::exp (-2.0f * kPi * std::clamp (p.highCut, 500.0f, 0.45f * sr) / sr);
        c.mod = p.mod;
        c.highCut = p.highCut;
        c.freeze = p.freeze;
        c.eq = p.freeze ? nullptr : &eqDesign;
        freezeIn.set (p.freeze ? 0.0f : 1.0f);
        shimmerGain.set (std::clamp (p.shimmer, 0.0f, 1.0f) * 0.6f);
        comp.setAmount (p.compAmount);
        switch (activeType)
        {
            case ReverbType::plate:  plate.configure (c); break;
            case ReverbType::spring: spring.configure (c); break;
            case ReverbType::room:
            case ReverbType::chamber:
            case ReverbType::hall:
            default:                 fdn.configure (c); break;
        }

        er.setGeometry (p.pan, p.depth, p.size, p.highCut);
        early.set (std::clamp (p.early, 0.0f, 1.0f) * 0.8f);

        hpCoef    = std::exp (-2.0f * kPi * std::clamp (p.lowCut, 10.0f, 2000.0f) / sr);
        dryLpCoef = std::exp (-2.0f * kPi * std::min (depth.dryCutoff, 0.45f * sr) / sr);
        widthS.set (std::clamp (p.width, 0.0f, 1.5f) * autoGain);
        lean.set (0.5f * std::clamp (p.pan, -1.0f, 1.0f));
        sideHp.set (p.monoBassHz, sr);
        enable.set (p.reverbOn ? 1.0f : 0.0f);
        if (p.reverbOn && asleep) { reset(); asleep = false; }   // probuzeni: cisty start

        float d, w;
        mixGains (p.reverbMix, d, w);
        d *= depth.dryGain;
        w *= depth.wetScale;
        dry.set (d);
        wet.set (w);
        if (first)
        {
            pre.snap (pre.target); dry.snap (d); wet.snap (w); widthS.snap (widthS.target);
            lean.snap (lean.target); enable.snap (enable.target); early.snap (early.target);
            freezeIn.snap (freezeIn.target); shimmerGain.snap (shimmerGain.target);
            first = false;
        }
        lastConfig = c;
    }

    bool isAsleep() { if (enable.target < 0.5f && enable.z < 1.0e-4f) asleep = true; return asleep; }

    void process (float& l, float& r)
    {
        if (asleep) return;
        const float inL = l, inR = r;
        const float e = enable.next();

        // prepnuti typu: wet ztlumit, prepnout, vycistit, zesilit
        if (pendingType != activeType)
        {
            typeFade -= fadeStep;
            if (typeFade <= 0.0f)
            {
                typeFade = 0.0f;
                activeType = pendingType;
                if (isFdn (activeType)) fdn.setType (activeType);
                plate.reset(); spring.reset(); fdn.reset();
                switch (activeType)
                {
                    case ReverbType::plate:  plate.configure (lastConfig); break;
                    case ReverbType::spring: spring.configure (lastConfig); break;
                    case ReverbType::room:
                    case ReverbType::chamber:
                    case ReverbType::hall:
                    default:                 fdn.configure (lastConfig); break;
                }
            }
        }
        else if (typeFade < 1.0f)
            typeFade = std::min (1.0f, typeFade + fadeStep);

        const float x = 0.5f * (l + r) * freezeIn.next();   // freeze: novy zvuk uz do dozvuku nejde
        preDelay.write (x);
        const float sg = shimmerGain.next();
        const float xp = preDelay.read (pre.next()) + (sg > 1.0e-4f ? shimmerFb : 0.0f);

        float yl = 0.0f, yr = 0.0f;
        switch (activeType)
        {
            case ReverbType::plate:  plate.process (xp, yl, yr); break;
            case ReverbType::spring: spring.process (xp, yl, yr); break;
            case ReverbType::room:
            case ReverbType::chamber:
            case ReverbType::hall:
            default:                 fdn.process (xp, yl, yr); break;
        }
        // shimmer: dozvuk o oktavu vys se vraci zpet do reverbu
        if (sg > 1.0e-4f || std::fabs (shimmerFb) > 0.0f)
        {
            const float up = shifter.process (0.5f * (yl + yr));
            shimmerHp = up + shimmerHpCoef * (shimmerHp - up);
            shimmerFb = (up - shimmerHp) * sg;
            if (sg <= 1.0e-4f) shimmerFb = 0.0f;
        }

        float el = 0.0f, erv = 0.0f;
        const float eg = early.next();
        if (eg > 1.0e-4f || early.target > 0.0f)
            er.process (x, el, erv);   // prvni odrazy jen kdyz je EARLY > 0
        yl = (yl + eg * el) * typeFade;
        yr = (yr + eg * erv) * typeFade;

        // low-cut
        hp[0] = yl + hpCoef * (hp[0] - yl);  yl -= hp[0];
        hp[1] = yr + hpCoef * (hp[1] - yr);  yr -= hp[1];

        // sirka sterea + basy do mona
        const float mid = 0.5f * (yl + yr);
        const float side = sideHp.process (0.5f * (yl - yr)) * widthS.next();
        yl = mid + side;
        yr = mid - side;

        // naklon dozvuku k pozici zdroje
        const float ln = lean.next();
        yl *= std::min (1.0f, 1.0f - ln);
        yr *= std::min (1.0f, 1.0f + ln);

        comp.process (yl, yr);   // komprese dozvuku

        // primy zvuk: cim dal, tim tmavsi
        dryLp[0] = l + dryLpCoef * (dryLp[0] - l);
        dryLp[1] = r + dryLpCoef * (dryLp[1] - r);

        const float d = dry.next(), w = wet.next() * duckGain;   // ducking
        const float outL = d * dryLp[0] + w * yl;
        const float outR = d * dryLp[1] + w * yr;
        l = inL + e * (outL - inL);
        r = inR + e * (outR - inR);
    }

    const DecayEqDesign& getDecayEq() const { return eqDesign; }

private:
    static bool isFdn (ReverbType t) { return t == ReverbType::room || t == ReverbType::chamber || t == ReverbType::hall; }

    DelayLine preDelay;
    PlateLate plate;
    FdnLate fdn;
    SpringLate spring;
    EarlyReflections er;
    DecayEqDesign eqDesign;
    HighPass sideHp;
    LateConfig lastConfig;
    Smooth pre, dry, wet, widthS, lean, early, enable;
    ReverbType activeType = ReverbType::plate, pendingType = ReverbType::plate;
    float sr = 48000.0f, hpCoef = 0.0f, dryLpCoef = 0.0f, typeFade = 1.0f, fadeStep = 0.001f;
    float hp[2] {}, dryLp[2] {};
    OctaveUp shifter;
    WetComp comp;
    Smooth freezeIn, shimmerGain;
    float shimmerFb = 0.0f, shimmerHp = 0.0f, shimmerHpCoef = 0.0f;
    bool first = true, asleep = false;

public:
    float duckGain = 1.0f;   // nastavuje Engine kazdy vzorek
};
} // namespace dusi
