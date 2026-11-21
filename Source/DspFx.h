// DusiX DeVerb - dalsi efekty: kompresor mokreho signalu, pitch shifter (shimmer), chorus, lo-fi.
// Vsechny se pri vypnuti preskakuji (0 % CPU) a zatezove vypocty bezi jen kazdych 16 vzorku.
#pragma once

#include "DspCore.h"
#include "DspParams.h"

namespace dusi
{
//==============================================================================
// Kompresor na mokry signal (srovna dozvuk/ozvenu, "prilepi" je k mixu).
// Jeden knoflik: vic = nizsi prah, vyssi pomer, automaticky makeup.
class WetComp
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        att = std::exp (-1.0f / (0.010f * sr));
        rel = std::exp (-1.0f / (0.150f * sr));
        env = 0.0f;
        gain = 1.0f;
        countdown = 0;
    }

    void setAmount (float amount)
    {
        amount = std::clamp (amount, 0.0f, 1.0f);
        active = amount > 0.001f;
        threshDb = -12.0f - 24.0f * amount;
        ratio = 1.0f + 5.0f * amount;
        makeup = dbToGain (-threshDb * (1.0f - 1.0f / ratio) * 0.5f);
    }

    void process (float& l, float& r)
    {
        if (! active) return;
        const float x = std::max (std::fabs (l), std::fabs (r));
        env = x > env ? x + att * (env - x) : x + rel * (env - x);
        if (--countdown <= 0)   // zisk jen kazdych 16 vzorku (log/pow jsou drahe)
        {
            countdown = 16;
            const float envDb = 20.0f * std::log10 (env + 1.0e-9f);
            const float over = envDb - threshDb;
            const float grDb = over > 0.0f ? over * (1.0f - 1.0f / ratio) : 0.0f;
            target = dbToGain (-grDb) * makeup;
            step = (target - gain) / 16.0f;
        }
        gain += step;
        l *= gain;
        r *= gain;
    }

private:
    float sr = 48000.0f, att = 0.0f, rel = 0.0f, env = 0.0f, gain = 1.0f, target = 1.0f, step = 0.0f;
    float threshDb = -12.0f, ratio = 1.0f, makeup = 1.0f;
    int countdown = 0;
    bool active = false;
};

//==============================================================================
// Pitch shifter o oktavu nahoru (dve cteci hlavy s prolinanim) - pro shimmer
class OctaveUp
{
public:
    void prepare (double sampleRate)
    {
        window = (float) std::max (1024, (int) (0.045 * sampleRate));   // ~45 ms okno
        line.prepare ((int) window * 2 + 8);
        win.setFreq (1.0f, window);   // 1 perioda na okno
        phase = 0.0f;
        win.reset();
    }
    void reset() { line.clear(); phase = 0.0f; win.reset(); }

    float process (float x)
    {
        line.write (x);
        // cteci hlava se pohybuje dvakrat rychleji nez zapis -> zpozdeni klesa o 1 vzorek za vzorek
        phase += 1.0f;
        if (phase >= window) phase -= window;
        const float p2 = phase + 0.5f * window >= window ? phase - 0.5f * window : phase + 0.5f * window;
        const float d1 = window - phase + 2.0f, d2 = window - p2 + 2.0f;
        // okna sin^2 a cos^2 (hlavy posunute o pul okna) -> staci jeden kosinus (rotaci, bez sin())
        if (phase < 1.0f) win.reset();   // nove okno: presne od nuly
        else win.next();
        const float w1sq = 0.5f - 0.5f * win.c;   // sin^2 (pi * phase / window)
        return line.read (d1) * w1sq + line.read (d2) * (1.0f - w1sq);
    }

private:
    DelayLine line;
    Phasor win;
    float window = 2048.0f, phase = 0.0f;
};

//==============================================================================
// Chorus / ensemble: 2 hlasy na kanal, pomale LFO s ruznou fazi
class Chorus
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        for (auto& l : lines) l.prepare ((int) (0.04f * sr));
        dry.prepare (sampleRate, 30.0f);
        wet.prepare (sampleRate, 30.0f);
        enable.prepare (sampleRate, 15.0f);
        static const float base[4] = { 7.0f, 11.3f, 8.6f, 12.7f };
        for (int v = 0; v < 4; ++v) delay[v] = base[v] * 0.001f * sr;
        first = true;
        reset();
    }
    void reset() { for (auto& l : lines) l.clear(); phase = 0.0; }

    void setParams (const Params& p)
    {
        rate = std::clamp (p.chorusRate, 0.05f, 8.0f);
        depthSamples = std::clamp (p.chorusDepth, 0.0f, 1.0f) * 0.004f * sr;   // az 4 ms
        float d, w;
        mixGains (p.chorusMix, d, w);
        dry.set (d);
        wet.set (w * 0.75f);
        enable.set (p.chorusOn ? 1.0f : 0.0f);
        if (p.chorusOn && asleep) { reset(); asleep = false; }
        if (enable.target < 0.5f && enable.z < 1.0e-4f) asleep = true;
        if (first) { dry.snap (d); wet.snap (w * 0.75f); enable.snap (enable.target); first = false; }
    }

    void process (float& l, float& r)
    {
        if (asleep) return;
        const float inL = l, inR = r;
        lines[0].write (l);
        lines[1].write (r);

        if (--countdown <= 0)   // zpozdeni hlasu se pocitaji kazdych 16 vzorku, mezi tim linearne
        {
            countdown = 16;
            phase += rate * 16.0 / sr;
            if (phase >= 1.0) phase -= 1.0;
            static const float base[4] = { 7.0f, 11.3f, 8.6f, 12.7f };   // ms
            static const float off[4]  = { 0.0f, 0.5f, 0.25f, 0.75f };
            for (int v = 0; v < 4; ++v)
            {
                const float t = std::sin (2.0f * kPi * (float) (phase + off[v]));
                const float target = base[v] * 0.001f * sr + depthSamples * (0.5f + 0.5f * t);
                step[v] = (target - delay[v]) / 16.0f;
            }
        }
        float voice[4];
        for (int v = 0; v < 4; ++v)
        {
            delay[v] += step[v];
            voice[v] = lines[v < 2 ? 0 : 1].readCubic (std::max (2.0f, delay[v]));
        }
        const float wl = 0.5f * (voice[0] + voice[1]), wr = 0.5f * (voice[2] + voice[3]);
        const float d = dry.next(), w = wet.next(), e = enable.next();
        l = inL + e * (d * inL + w * wl - inL);
        r = inR + e * (d * inR + w * wr - inR);
    }

private:
    DelayLine lines[2];
    Smooth dry, wet, enable;
    double phase = 0.0;
    float sr = 48000.0f, rate = 0.6f, depthSamples = 0.0f;
    float delay[4] { 300, 500, 400, 600 }, step[4] {};
    int countdown = 0;
    bool first = true, asleep = false;
};

//==============================================================================
// Lo-fi: snizeni bitu, snizeni vzorkovaci frekvence a tlumeni vysek
class Lofi
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        dry.prepare (sampleRate, 30.0f);
        wet.prepare (sampleRate, 30.0f);
        enable.prepare (sampleRate, 15.0f);
        first = true;
        reset();
    }
    void reset() { held[0] = held[1] = lp[0] = lp[1] = 0.0f; acc = 1.0f; }

    void setParams (const Params& p)
    {
        levels = std::pow (2.0f, std::clamp (p.lofiBits, 2.0f, 16.0f) - 1.0f);
        inc = std::clamp (p.lofiRate, 500.0f, sr) / sr;
        lpCoef = p.lofiTone >= 19500.0f ? 0.0f   // ton naplno = bez filtru
                                          : std::exp (-2.0f * kPi * std::clamp (p.lofiTone, 500.0f, 0.45f * sr) / sr);
        float d, w;
        mixGains (p.lofiMix, d, w);
        dry.set (d);
        wet.set (w);
        enable.set (p.lofiOn ? 1.0f : 0.0f);
        if (p.lofiOn && asleep) { reset(); asleep = false; }
        if (enable.target < 0.5f && enable.z < 1.0e-4f) asleep = true;
        if (first) { dry.snap (d); wet.snap (w); enable.snap (enable.target); first = false; }
    }

    void process (float& l, float& r)
    {
        if (asleep) return;
        const float in[2] = { l, r };
        acc += inc;
        if (acc >= 1.0f)   // novy "vzorek" pri nizsi vzorkovaci frekvenci
        {
            acc -= 1.0f;
            for (int c = 0; c < 2; ++c)
                held[c] = std::round (std::clamp (in[c], -1.0f, 1.0f) * levels) / levels;
        }
        const float d = dry.next(), w = wet.next(), e = enable.next();
        float out[2];
        for (int c = 0; c < 2; ++c)
        {
            lp[c] = held[c] + lpCoef * (lp[c] - held[c]);
            out[c] = in[c] + e * (d * in[c] + w * lp[c] - in[c]);
        }
        l = out[0];
        r = out[1];
    }

private:
    Smooth dry, wet, enable;
    float sr = 48000.0f, levels = 512.0f, inc = 0.25f, acc = 1.0f, lpCoef = 0.0f;
    float held[2] {}, lp[2] {};
    bool first = true, asleep = false;
};
} // namespace dusi
