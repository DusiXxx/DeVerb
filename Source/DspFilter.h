// DusiX DeVerb - filtr s LFO (stavova promenna TPT SVF podle Zavalishina) synchronizovany s tempem.
#pragma once

#include "DspCore.h"
#include "DspParams.h"

namespace dusi
{
class LfoFilter
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        freq.prepare (sampleRate, 30.0f);
        reso.prepare (sampleRate, 30.0f);
        depth.prepare (sampleRate, 30.0f);
        dry.prepare (sampleRate, 30.0f);
        wet.prepare (sampleRate, 30.0f);
        enable.prepare (sampleRate, 15.0f);
        randSmooth = std::exp (-16.0f / (0.004f * sr));
        first = true;
        reset();
    }

    void reset()
    {
        for (auto& s : svf) s = {};
        phase = 0.0;
        randHeld[0] = randHeld[1] = randNow[0] = randNow[1] = 0.0f;
        lastStep[0] = lastStep[1] = -1;
    }

    void setParams (const Params& p, int blockSize)
    {
        type = p.filterType;
        shape = p.lfoShape;
        stereoOffset = std::clamp (p.lfoStereo, 0.0f, 180.0f) / 360.0f;
        freq.set (std::log2 (std::clamp (p.filterFreq, 20.0f, 20000.0f)));
        reso.set (std::clamp (p.filterReso, 0.0f, 1.0f));
        depth.set (std::clamp (p.lfoDepth, 0.0f, 1.0f) * 4.0f);   // +-4 oktavy
        float d, w;
        mixGains (p.filterMix, d, w);
        dry.set (d);
        wet.set (w);
        enable.set (p.filterOn ? 1.0f : 0.0f);
        rateHz = std::clamp (p.lfoHz, 0.01f, 40.0f);

        // V synchronu: faze se srovna s pozici v taktu, kdyz hostitel hraje
        if (p.lfoSynced && p.playing && p.lfoBeats > 0.0f)
        {
            const double target = std::fmod (p.ppq / (double) p.lfoBeats, 1.0);
            double diff = target - phase;
            diff -= std::floor (diff + 0.5);
            if (std::fabs (diff) > 0.02) phase = target < 0.0 ? target + 1.0 : target;   // velky skok = srovnat
        }
        (void) blockSize;

        if (first)
        {
            freq.snap (freq.target); reso.snap (reso.target); depth.snap (depth.target);
            dry.snap (d); wet.snap (w); enable.snap (enable.target);
            first = false;
        }
    }

    void process (float& l, float& r)
    {
        const float inL = l, inR = r;
        const float e = enable.next();
        const float baseLog = freq.next(), res = reso.next(), dep = depth.next();
        const float d = dry.next(), w = wet.next();

        phase += rateHz / sr;
        if (phase >= 1.0) phase -= 1.0;

        if (e < 1.0e-4f && enable.target < 0.5f)
            return;   // vypnuto a dozneno - usetri CPU

        // koeficienty se prepocitavaji jen kazdych 16 vzorku (LFO je pomale) - setri pow/tan
        if (--coefCountdown <= 0)
        {
            coefCountdown = kCoefStep;
            k = 2.0f - 1.96f * res;   // tlumeni: 2 = bez rezonance, ~0.04 = silna rezonance
            for (int ch = 0; ch < 2; ++ch)
            {
                double ph = phase + (ch == 1 ? stereoOffset : 0.0);
                if (ph >= 1.0) ph -= 1.0;
                const float lfo = lfoValue ((float) ph, ch);
                const float fHz = std::clamp (std::exp2 (baseLog + dep * lfo), 20.0f, 0.45f * sr);
                const float g = std::tan (kPi * fHz / sr);
                auto& c = coef[(size_t) ch];
                c.a1 = 1.0f / (1.0f + g * (g + k));
                c.a2 = g * c.a1;
                c.a3 = g * c.a2;
            }
        }
        float out[2];
        const float in[2] = { l, r };
        for (int ch = 0; ch < 2; ++ch)
        {
            auto& s = svf[(size_t) ch];
            const auto& c = coef[(size_t) ch];
            const float a1 = c.a1, a2 = c.a2, a3 = c.a3;
            const float v3 = in[ch] - s.ic2;
            const float v1 = a1 * s.ic1 + a2 * v3;
            const float v2 = s.ic2 + a2 * s.ic1 + a3 * v3;
            s.ic1 = 2.0f * v1 - s.ic1;
            s.ic2 = 2.0f * v2 - s.ic2;
            float y = v2;                                         // dolni propust
            if (type == FilterType::bandPass) y = v1 * k;         // pasmova (normovana)
            else if (type == FilterType::highPass) y = in[ch] - k * v1 - v2;
            out[ch] = d * in[ch] + w * y;
        }
        l = inL + e * (out[0] - inL);
        r = inR + e * (out[1] - inR);
    }

    float getPhase() const { return (float) phase; }

private:
    // LFO -1 .. +1
    float lfoValue (float ph, int ch)
    {
        switch (shape)
        {
            case LfoShape::triangle: return 1.0f - 4.0f * std::fabs (ph - 0.5f);
            case LfoShape::saw:      return 2.0f * ph - 1.0f;
            case LfoShape::square:
            {
                // mekky obdelnik (bez cvakani)
                return std::tanh (12.0f * std::sin (2.0f * kPi * ph)) / std::tanh (12.0f);
            }
            case LfoShape::random:
            {
                const int step = ph < 0.5f ? 0 : 1;   // dve nahodne hodnoty na periodu
                if (step != lastStep[ch])
                {
                    lastStep[ch] = step;
                    seed = seed * 1664525u + 1013904223u;
                    randHeld[ch] = (float) (seed >> 8) / 8388608.0f - 1.0f;
                }
                randNow[ch] = randHeld[ch] + randSmooth * (randNow[ch] - randHeld[ch]);
                return randNow[ch];
            }
            case LfoShape::sine:
            default:                 return std::sin (2.0f * kPi * ph);
        }
    }

    struct Svf { float ic1 = 0, ic2 = 0; };
    struct Coef { float a1 = 1, a2 = 0, a3 = 0; };
    static constexpr int kCoefStep = 16;
    std::array<Svf, 2> svf {};
    std::array<Coef, 2> coef {};
    int coefCountdown = 0;
    float k = 2.0f;
    Smooth freq, reso, depth, dry, wet, enable;
    FilterType type = FilterType::lowPass;
    LfoShape shape = LfoShape::sine;
    double phase = 0.0;
    float sr = 48000.0f, rateHz = 1.0f, stereoOffset = 0.0f, randSmooth = 0.0f;
    float randHeld[2] {}, randNow[2] {};
    int lastStep[2] { -1, -1 };
    unsigned seed = 12345u;
    bool first = true;
};
} // namespace dusi
