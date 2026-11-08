// DusiX DeVerb - vystupni EQ (8 pasem) a navrh pasem sdileny s GUI.
#pragma once

#include "DspCore.h"
#include "DspParams.h"

namespace dusi
{
// Navrh jednoho pasma do 1-4 biquadu. Vraci pocet pouzitych sekci.
// Pouziva se v DSP i v GUI pro kresleni krivky, takze obraz presne odpovida zvuku.
inline int designEqBand (const EqBand& b, float sr, std::array<BiquadCoefs, 4>& out)
{
    const float f = std::clamp (b.freq, 10.0f, 0.48f * sr);
    switch (b.type)
    {
        case EqType::lowCut:
        case EqType::highCut:
        {
            const bool low = b.type == EqType::lowCut;
            auto make = [&] (float q) { return low ? BiquadCoefs::highPass (f, q, sr) : BiquadCoefs::lowPass (f, q, sr); };
            if (b.slope <= 0) { out[0] = make (std::clamp (b.q, 0.3f, 10.0f)); return 1; }   // 12 dB/okt, Q = rezonance
            if (b.slope == 1) { out[0] = make (0.5412f); out[1] = make (1.3066f); return 2; } // 24 dB/okt Butterworth
            const float qs[4] = { 0.5098f, 0.6013f, 0.9000f, 2.5629f };                      // 48 dB/okt Butterworth
            for (int i = 0; i < 4; ++i) out[(size_t) i] = make (qs[i]);
            return 4;
        }
        case EqType::lowShelf:  out[0] = BiquadCoefs::lowShelf  (f, std::clamp (b.q, 0.3f, 2.0f), b.gain, sr); return 1;
        case EqType::highShelf: out[0] = BiquadCoefs::highShelf (f, std::clamp (b.q, 0.3f, 2.0f), b.gain, sr); return 1;
        case EqType::notch:     out[0] = BiquadCoefs::notch (f, std::clamp (b.q, 0.3f, 20.0f), sr); return 1;
        case EqType::bell:
        default:                out[0] = BiquadCoefs::bell (f, std::clamp (b.q, 0.1f, 20.0f), b.gain, sr); return 1;
    }
}

inline float eqBandMagnitudeDb (const EqBand& b, float f, float sr)
{
    std::array<BiquadCoefs, 4> c;
    const int n = designEqBand (b, sr, c);
    float db = 0.0f;
    for (int i = 0; i < n; ++i) db += c[(size_t) i].magnitudeDb (f, sr);
    return db;
}

//------------------------------------------------------------------------------
class OutputEq
{
public:
    static constexpr int kStep = 32;   // koeficienty se prepocitavaji kazdych 32 vzorku

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        for (auto& s : smooth)
        {
            s.logF.prepare (sampleRate / kStep, 40.0f);
            s.gain.prepare (sampleRate / kStep, 40.0f);
            s.q.prepare (sampleRate / kStep, 40.0f);
        }
        first = true;
        reset();
    }

    void reset()
    {
        for (auto& ch : state)
            for (auto& band : ch)
                for (auto& st : band) st.reset();
    }

    void setParams (const Params& p)
    {
        enabled = p.eqOn;
        for (int i = 0; i < kNumEqBands; ++i)
        {
            const auto& b = p.eq[(size_t) i];
            auto& s = smooth[(size_t) i];
            // vypnute pasmo se zvukem plynule "vynuluje" (gain -> 0), pak se preskakuje
            const bool gainType = b.type == EqType::bell || b.type == EqType::lowShelf || b.type == EqType::highShelf;
            if (b.type != current[(size_t) i].type || b.slope != current[(size_t) i].slope)
                for (auto& ch : state) for (auto& st : ch[(size_t) i]) st.reset();
            current[(size_t) i].type = b.type;
            current[(size_t) i].slope = b.slope;
            active[(size_t) i] = b.on || (gainType && std::fabs (s.gain.z) > 0.01f);
            s.logF.set (std::log2 (std::clamp (b.freq, 10.0f, 22000.0f)));
            s.gain.set (b.on ? b.gain : 0.0f);
            s.q.set (b.q);
            if (first) { s.logF.snap (s.logF.target); s.gain.snap (s.gain.target); s.q.snap (s.q.target); }
            if (! gainType && ! b.on) active[(size_t) i] = false;
        }
        first = false;
        dirty = true;
    }

    // Zpracuje blok; volat po kouscich max kStep vzorku
    void processChunk (float* l, float* r, int n)
    {
        if (! enabled)
            return;
        updateCoefs();
        for (int i = 0; i < kNumEqBands; ++i)
        {
            if (! active[(size_t) i]) continue;
            const int ns = numStages[(size_t) i];
            for (int st = 0; st < ns; ++st)
            {
                const auto& c = coefs[(size_t) i][(size_t) st];
                auto& sl = state[0][(size_t) i][(size_t) st];
                auto& sr2 = state[1][(size_t) i][(size_t) st];
                for (int k = 0; k < n; ++k)
                {
                    l[k] = sl.process (c, l[k]);
                    r[k] = sr2.process (c, r[k]);
                }
            }
        }
    }

private:
    void updateCoefs()
    {
        for (int i = 0; i < kNumEqBands; ++i)
        {
            if (! active[(size_t) i]) continue;
            auto& s = smooth[(size_t) i];
            const bool moving = ! (s.logF.settled() && s.gain.settled() && s.q.settled());
            if (! moving && ! dirty) continue;
            EqBand b = current[(size_t) i];
            b.freq = std::pow (2.0f, s.logF.next());
            b.gain = s.gain.next();
            b.q    = s.q.next();
            numStages[(size_t) i] = designEqBand (b, sr, coefs[(size_t) i]);
        }
        dirty = false;
    }

    struct BandSmooth { Smooth logF, gain, q; };
    std::array<BandSmooth, kNumEqBands> smooth;
    std::array<EqBand, kNumEqBands> current {};
    std::array<bool, kNumEqBands> active {};
    std::array<int, kNumEqBands> numStages {};
    std::array<std::array<BiquadCoefs, 4>, kNumEqBands> coefs {};
    std::array<std::array<std::array<BiquadState, 4>, kNumEqBands>, 2> state {};
    float sr = 48000.0f;
    bool enabled = true, first = true, dirty = true;
};
} // namespace dusi
