// DusiX DeVerb - stereo delay se synchronizaci, ping-pongem a filtry ve zpetne vazbe.
#pragma once

#include "DspCore.h"
#include "DspParams.h"
#include "DspFx.h"

namespace dusi
{
class StereoDelay
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        for (auto& l : lines) l.prepare ((int) (2.1f * sr));
        time.prepare (sampleRate, 120.0f);
        fb.prepare (sampleRate, 30.0f);
        dry.prepare (sampleRate, 30.0f);
        wet.prepare (sampleRate, 30.0f);
        side.prepare (sampleRate, 50.0f);
        enable.prepare (sampleRate, 15.0f);
        comp.prepare (sampleRate);
        hpCoef = std::exp (-2.0f * kPi * 120.0f / sr);   // low-cut opakovani (cistsi basy)
        for (auto& h : inHp) h.set (150.0f, sr);          // do ozveny nejdou hluboke basy (kopak, bas)
        first = true;
        reset();
    }

    void reset()
    {
        for (auto& l : lines) l.clear();
        lp[0] = lp[1] = hp[0] = hp[1] = 0.0f;
        sideHp.reset();
        inHp[0].reset(); inHp[1].reset();
    }

    void setParams (const Params& p, float autoGain)
    {
        time.set (std::clamp (p.delayMs, 1.0f, 2000.0f) * 0.001f * sr);
        fb.set (std::clamp (p.feedback, 0.0f, 0.95f));
        float d, w;
        mixGains (std::clamp (p.delayMix, 0.0f, 1.0f), d, w);
        dry.set (d);
        wet.set (w);
        side.set (autoGain);
        enable.set (p.delayOn ? 1.0f : 0.0f);
        if (p.delayOn && asleep) { reset(); asleep = false; }   // probuzeni: cisty start
        lpCoef = std::exp (-2.0f * kPi * std::clamp (p.delayTone, 200.0f, 0.45f * sr) / sr);
        sideHp.set (p.monoBassHz, sr);
        comp.setAmount (p.compAmount);
        pingPong = p.pingPong;
        if (first)
        {
            time.snap (time.target); fb.snap (fb.target); dry.snap (d); wet.snap (w);
            side.snap (autoGain); enable.snap (enable.target);
            first = false;
        }
    }

    // Vypnuty a dozneny efekt se vubec nepocita (0 % CPU)
    bool isAsleep() { if (enable.target < 0.5f && enable.z < 1.0e-4f) asleep = true; return asleep; }

    void process (float& l, float& r)
    {
        if (asleep) return;
        const float inL = l, inR = r;
        const float t = std::max (1.0f, time.next());
        const float g = fb.next();

        float dl = lines[0].read (t);
        float dr = lines[1].read (t);

        // filtry ve zpetne vazbe: kazde opakovani je o neco temnejsi a tenci
        const float fl = filter (dl, 0), fr = filter (dr, 1);

        const float il = inHp[0].process (l), ir = inHp[1].process (r);
        if (pingPong)
        {
            lines[0].write (0.5f * (il + ir) + fastTanh (g * fr));
            lines[1].write (fastTanh (g * fl));
        }
        else
        {
            lines[0].write (il + fastTanh (g * fl));
            lines[1].write (ir + fastTanh (g * fr));
        }

        // stereo pojistka na mokrem signalu: basy do mona + auto zuzeni
        const float m = 0.5f * (dl + dr);
        const float s = sideHp.process (0.5f * (dl - dr)) * side.next();
        dl = m + s;
        dr = m - s;

        comp.process (dl, dr);                       // komprese ozveny
        const float d = dry.next(), w = wet.next() * duckGain;   // ducking: ozvena ustoupi, kdyz hraje zdroj
        const float outL = d * l + w * dl;
        const float outR = d * r + w * dr;

        const float e = enable.next();               // vypinac s prolnutim
        l = inL + e * (outL - inL);
        r = inR + e * (outR - inR);
    }

private:
    // rychla nahrada tanh (mekke omezeni zpetne vazby)
    static float fastTanh (float x)
    {
        x = std::clamp (x, -3.0f, 3.0f);
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    float filter (float x, int ch)
    {
        lp[ch] = x + lpCoef * (lp[ch] - x);          // high-cut
        hp[ch] = lp[ch] + hpCoef * (hp[ch] - lp[ch]); // low-cut 120 Hz
        return lp[ch] - hp[ch];
    }

    DelayLine lines[2];
    HighPass sideHp, inHp[2];
    Smooth time, fb, dry, wet, side, enable;
    float sr = 48000.0f, lpCoef = 0.0f, hpCoef = 0.0f;
    float lp[2] {}, hp[2] {};
    WetComp comp;
    bool pingPong = false, first = true, asleep = false;

public:
    float duckGain = 1.0f;   // nastavuje Engine kazdy vzorek
};
} // namespace dusi
