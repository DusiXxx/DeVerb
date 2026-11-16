// DusiX DeVerb - cely zvukovy engine:
// pozice (pan) -> [delay, reverb, filtr v nastavitelnem poradi] -> vystupni EQ -> hlasitost
// + mereni korelace a auto-safe.
#pragma once

#include <limits>
#include "DspCore.h"
#include "DspParams.h"
#include "DspEq.h"
#include "DspDelay.h"
#include "DspReverb.h"
#include "DspFilter.h"
#include "DspFx.h"

namespace dusi
{
class Engine
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        tail.assign (8192, 0.0f);
        delay.prepare (sampleRate);
        reverb.prepare (sampleRate);
        filter.prepare (sampleRate);
        chorus.prepare (sampleRate);
        lofi.prepare (sampleRate);
        eq.prepare (sampleRate);
        duckAttack = std::exp (-1.0f / (0.005f * sr));
        gain.prepare (sampleRate, 30.0f);
        panL.prepare (sampleRate, 30.0f);
        panR.prepare (sampleRate, 30.0f);
        meterCoef = std::exp (-1.0f / (0.3f * sr));    // korelace prumerovana ~300 ms
        levelCoef = std::exp (-1.0f / (0.08f * sr));
        orderStep = 1.0f / (0.01f * sr);
        first = true;
        reset();
    }

    void reset()
    {
        delay.reset();
        reverb.reset();
        filter.reset();
        chorus.reset();
        lofi.reset();
        eq.reset();
        duckEnv = 0.0f; duckGain = duckTarget = 1.0f;
        sLR = sLL = sRR = iLR = iLL = iRR = 0.0f;
        level = 0.0f;
        correlation = 1.0f;
        autoGain = 1.0f;
        silentInSamples = quietOutSamples = 0;
    }

    void setParams (const Params& p, int blockSize = 512)
    {
        safe = p.autoSafe;
        delay.setParams (p, autoGain);
        reverb.setParams (p, autoGain);
        filter.setParams (p, blockSize);
        chorus.setParams (p);
        lofi.setParams (p);
        duckAmount = std::clamp (p.duckAmount, 0.0f, 1.0f);
        duckRelease = std::exp (-3.0f / (std::clamp (p.duckRelease, 10.0f, 2000.0f) * 0.001f * sr));   // release = cas navratu
        eq.setParams (p);
        gain.set (dbToGain (p.outGainDb));
        const float pn = std::clamp (p.pan, -1.0f, 1.0f);
        panL.set (std::min (1.0f, 1.0f - pn));        // balance: stred = beze zmeny
        panR.set (std::min (1.0f, 1.0f + pn));
        pendingOrder = std::clamp (p.chainOrder, 0, (int) chainOrders().size() - 1);
        delay.isAsleep();
        reverb.isAsleep();
        // jak dlouho musi byt ticho, nez engine "usne" (aby se neusekl dozvuk ani ozvena)
        sleepAfter = (p.freeze && p.reverbOn) ? std::numeric_limits<int>::max()   // zmrazeny dozvuk hraje dal
                   : (int) (sr * (0.3f + (p.delayOn ? p.delayMs * 0.001f : 0.0f) + (p.reverbOn ? p.preDelayMs * 0.0013f : 0.0f)));
        if (first)
        {
            gain.snap (gain.target); panL.snap (panL.target); panR.snap (panR.target);
            order = pendingOrder;
            first = false;
        }
    }

    void process (float* left, float* right, int numSamples)
    {
        // --- spanek v tichu: kdyz nic nehraje a dozvuky dozneli, nepocita se vubec nic
        float inPeak = 0.0f;
        for (int k = 0; k < numSamples; ++k)
            inPeak = std::max (inPeak, std::max (std::fabs (left[k]), std::fabs (right[k])));
        const bool silentIn = inPeak < kSilence;
        silentInSamples = silentIn ? std::min (silentInSamples + numSamples, 1 << 30) : 0;
        if (silentIn && silentInSamples > sleepAfter && quietOutSamples > sleepAfter)
        {
            std::fill (left, left + numSamples, 0.0f);
            std::fill (right, right + numSamples, 0.0f);
            sleeping = true;
            tailCount = 0;
            level = 0.0f;
            correlation = 1.0f;
            sLR = sLL = sRR = iLR = iLL = iRR = 0.0f;
            autoGain = std::min (1.0f, autoGain + 0.01f);
            return;
        }
        sleeping = false;
        tailCount = std::min (numSamples, (int) tail.size());

        for (int start = 0; start < numSamples; start += OutputEq::kStep)
        {
            const int n = std::min (OutputEq::kStep, numSamples - start);
            float* l = left + start;
            float* r = right + start;
            for (int k = 0; k < n; ++k)
            {
                float a = l[k] * panL.next(), b = r[k] * panR.next();
                level = std::max (std::max (std::fabs (a), std::fabs (b)), level * levelCoef);
                iLR = a * b + meterCoef * (iLR - a * b);   // korelace vstupu (pro auto-safe)
                iLL = a * a + meterCoef * (iLL - a * a);
                iRR = b * b + meterCoef * (iRR - b * b);

                // zmena poradi: kratce ztlumit, prehodit, zesilit
                if (pendingOrder != order)
                {
                    orderFade -= orderStep;
                    if (orderFade <= 0.0f) { orderFade = 0.0f; order = pendingOrder; }
                }
                else if (orderFade < 1.0f)
                    orderFade = std::min (1.0f, orderFade + orderStep);

                // ducking: obalka vstupu -> zisk mokreho signalu (prepocet kazdych 16 vzorku)
                if (duckAmount > 0.0f)
                {
                    const float x = std::max (std::fabs (a), std::fabs (b));
                    duckEnv = x > duckEnv ? x + duckAttack * (duckEnv - x) : x + duckRelease * (duckEnv - x);
                    if (--duckCountdown <= 0)
                    {
                        duckCountdown = 16;
                        const float envDb = 20.0f * std::log10 (duckEnv + 1.0e-9f);
                        const float reduction = duckAmount * 24.0f * std::clamp ((envDb + 40.0f) / 20.0f, 0.0f, 1.0f);
                        duckTarget = dbToGain (-reduction);
                        duckStep = (duckTarget - duckGain) / 16.0f;
                    }
                    duckGain += duckStep;
                }
                else
                    duckGain = 1.0f;
                delay.duckGain = duckGain;
                reverb.duckGain = duckGain;

                for (auto m : chainOrders()[(size_t) order])
                {
                    switch (m)
                    {
                        case Module::delay:  delay.process (a, b); break;
                        case Module::reverb: reverb.process (a, b); break;
                        case Module::filter: filter.process (a, b); break;
                        case Module::chorus: chorus.process (a, b); break;
                        case Module::lofi:   lofi.process (a, b); break;
                    }
                }
                l[k] = a * orderFade;
                r[k] = b * orderFade;
                if (start + k < (int) tail.size()) tail[(size_t) (start + k)] = reverb.lastTail;
            }

            eq.processChunk (l, r, n);

            for (int k = 0; k < n; ++k)
            {
                const float g = gain.next();
                const float a = l[k] * g, b = r[k] * g;
                l[k] = a;
                r[k] = b;
                sLR = a * b + meterCoef * (sLR - a * b);
                sLL = a * a + meterCoef * (sLL - a * a);
                sRR = b * b + meterCoef * (sRR - b * b);
            }
        }
        updateCorrelation (numSamples);

        float outPeak = 0.0f;
        for (int k = 0; k < numSamples; ++k)
            outPeak = std::max (outPeak, std::max (std::fabs (left[k]), std::fabs (right[k])));
        quietOutSamples = outPeak < kSilence ? std::min (quietOutSamples + numSamples, 1 << 30) : 0;
    }

    bool isSleeping() const { return sleeping; }
    // mokry signal reverbu z posledniho bloku (pro analyzator dozvuku); pocet = min (blok, 8192), pri spanku 0
    const float* getTail() const  { return tail.data(); }
    int getTailCount() const      { return tailCount; }

    float getCorrelation() const  { return correlation; }  // -1 .. +1
    float getAutoGain() const     { return autoGain; }     // 1 = bez zasahu
    float getLevel() const        { return level; }
    float getLfoPhase() const     { return filter.getPhase(); }
    float getDuckGain() const     { return duckGain; }
    const DecayEqDesign& getDecayEq() const { return reverb.getDecayEq(); }

private:
    void updateCorrelation (int numSamples)
    {
        const float energy = std::sqrt (sLL * sRR);
        correlation = energy > 1.0e-7f ? std::clamp (sLR / energy, -1.0f, 1.0f) : 1.0f;

        // Kdyz je v protifazi uz zdroj, efekt za to nemuze a nezuzujeme ho.
        const float inEnergy = std::sqrt (iLL * iRR);
        const float inCorr = inEnergy > 1.0e-7f ? std::clamp (iLR / inEnergy, -1.0f, 1.0f) : 1.0f;
        const bool effectIsCause = correlation < inCorr - 0.1f;

        // Auto-safe: pod korelaci 0 zacne zuzovat stereo efektu, pri -0.4 az na 30 %.
        float target = 1.0f;
        if (safe && energy > 1.0e-7f && effectIsCause)
            target = std::clamp (0.3f + 0.7f * (correlation + 0.4f) / 0.4f, 0.3f, 1.0f);

        const float dt = (float) numSamples / sr;
        const float tau = target < autoGain ? 0.05f : 1.5f;   // rychle zuzi, pomalu povoli
        autoGain = target + std::exp (-dt / tau) * (autoGain - target);
    }

    std::vector<float> tail;
    int tailCount = 0;
    StereoDelay delay;
    ReverbUnit reverb;
    LfoFilter filter;
    Chorus chorus;
    Lofi lofi;
    OutputEq eq;
    Smooth gain, panL, panR;
    int order = 0, pendingOrder = 0;
    float sr = 48000.0f, meterCoef = 0.0f, levelCoef = 0.0f, orderFade = 1.0f, orderStep = 0.001f;
    float sLR = 0, sLL = 0, sRR = 0, iLR = 0, iLL = 0, iRR = 0, level = 0, correlation = 1.0f, autoGain = 1.0f;
    float duckAmount = 0.0f, duckEnv = 0.0f, duckGain = 1.0f, duckTarget = 1.0f, duckStep = 0.0f;
    float duckAttack = 0.0f, duckRelease = 0.0f;
    int duckCountdown = 0;
    static constexpr float kSilence = 1.0e-5f;   // -100 dB
    int silentInSamples = 0, quietOutSamples = 0, sleepAfter = 48000;
    bool safe = true, first = true, sleeping = false;
};
} // namespace dusi
