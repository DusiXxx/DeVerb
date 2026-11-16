// DusiX DeVerb - vsechny hodnoty, ktere DSP dostava z parametru pluginu (ve fyzikalnich jednotkach).
#pragma once

#include <algorithm>
#include <array>
#include <vector>

namespace dusi
{
enum class ReverbType { plate = 0, room, chamber, hall, spring, prism };
enum class EqType     { lowCut = 0, lowShelf, bell, highShelf, highCut, notch };
enum class FilterType { lowPass = 0, bandPass, highPass };
enum class LfoShape   { sine = 0, triangle, saw, square, random };
enum class Module     { delay = 0, reverb, filter, chorus, lofi };
constexpr int kNumModules = 5;

constexpr int kNumEqBands = 8;
constexpr int kNumDecayBands = 4;   // low shelf, bell, bell, high shelf

// Poradi modulu v retezu: vsech 120 permutaci. Prvnich 6 odpovida starsi verzi (3 moduly)
// s chorusem a lo-fi na konci, aby stare projekty hraly stejne.
using ChainOrder = std::array<Module, kNumModules>;
inline const std::vector<ChainOrder>& chainOrders()
{
    static const std::vector<ChainOrder> list = []
    {
        std::vector<ChainOrder> v;
        const Module D = Module::delay, R = Module::reverb, F = Module::filter, C = Module::chorus, L = Module::lofi;
        for (auto o : { ChainOrder { D, R, F, C, L }, ChainOrder { D, F, R, C, L }, ChainOrder { R, D, F, C, L },
                        ChainOrder { R, F, D, C, L }, ChainOrder { F, D, R, C, L }, ChainOrder { F, R, D, C, L } })
            v.push_back (o);
        ChainOrder perm { D, R, F, C, L };
        std::sort (perm.begin(), perm.end());
        do
        {
            if (std::find (v.begin(), v.end(), perm) == v.end()) v.push_back (perm);
        } while (std::next_permutation (perm.begin(), perm.end()));
        return v;
    }();
    return list;
}

struct EqBand
{
    bool   on    = false;
    EqType type  = EqType::bell;
    float  freq  = 1000.0f;
    float  gain  = 0.0f;     // dB
    float  q     = 0.707f;
    int    slope = 1;        // 0 = 12, 1 = 24, 2 = 48 dB/okt (jen cut)
};

struct DecayBand
{
    float freq = 1000.0f;
    float mult = 1.0f;       // nasobek delky dozvuku (0.25 .. 4)
    float q    = 0.8f;
};

struct Params
{
    // Delay
    bool  delayOn    = true;
    float delayMs    = 375.0f;   // 1 .. 2000
    float feedback   = 0.35f;    // 0 .. 0.95
    float delayTone  = 6000.0f;  // Hz, high-cut ve zpetne vazbe
    bool  pingPong   = false;
    float delayMix   = 0.25f;    // 0 .. 1

    // Reverb
    bool  reverbOn   = true;
    ReverbType reverbType = ReverbType::plate;
    float preDelayMs = 20.0f;    // 0 .. 500
    float size       = 1.0f;     // 0.3 .. 1.5
    float decaySec   = 2.5f;     // 0.2 .. 20
    float highCut    = 8000.0f;  // Hz, tlumeni vysek v dozvuku
    float lowCut     = 150.0f;   // Hz, orez basu z dozvuku
    float mod        = 0.3f;     // 0 .. 1
    float early      = 0.25f;    // hlasitost prvnich odrazu 0 .. 1
    float reverbMix  = 0.3f;     // 0 .. 1
    bool  decayEqOn  = false;
    std::array<DecayBand, kNumDecayBands> decayEq {{ { 200.0f, 1.0f, 0.7f }, { 700.0f, 1.0f, 0.8f },
                                                     { 2500.0f, 1.0f, 0.8f }, { 7000.0f, 1.0f, 0.7f } }};

    // Prostor a stereo
    float pan        = 0.0f;     // -1 (L) .. 1 (R)
    float depth      = 0.35f;    // 0 (u posluchace) .. 1 (vzadu v mistnosti)
    float width      = 1.0f;     // 0 .. 1.5 (uz omezena bezpecnym limitem)
    float monoBassHz = 150.0f;   // pod touto frekvenci je efekt mono
    bool  autoSafe   = true;

    // Filtr s LFO
    bool  filterOn    = false;
    FilterType filterType = FilterType::lowPass;
    float filterFreq  = 1200.0f;
    float filterReso  = 0.3f;    // 0 .. 1
    float lfoHz       = 1.0f;    // rychlost LFO (uz prepoctena z tempa, kdyz je sync)
    bool  lfoSynced   = true;
    float lfoBeats    = 4.0f;    // delka jedne periody v dobach (pro fazovani s taktem)
    float lfoDepth    = 0.5f;    // 0 .. 1 (= az +-4 oktavy)
    LfoShape lfoShape = LfoShape::sine;
    float lfoStereo   = 0.0f;    // fazovy posun L/P ve stupnich 0 .. 180
    float filterMix   = 1.0f;

    // Dynamika efektu (delay + reverb)
    float duckAmount  = 0.0f;    // 0 .. 1 (az -24 dB, kdyz hraje zdroj)
    float duckRelease = 250.0f;  // ms
    float compAmount  = 0.0f;    // 0 .. 1 komprese mokreho signalu

    // Reverb navic
    bool  freeze  = false;       // nekonecny dozvuk
    float shimmer = 0.0f;        // 0 .. 1 oktava nahoru ve zpetne vazbe

    // Chorus / ensemble
    bool  chorusOn    = false;
    float chorusRate  = 0.6f;    // Hz
    float chorusDepth = 0.4f;    // 0 .. 1
    float chorusMix   = 0.5f;

    // Lo-fi
    bool  lofiOn   = false;
    float lofiBits = 10.0f;      // 4 .. 16
    float lofiRate = 12000.0f;   // Hz (vzorkovaci frekvence)
    float lofiTone = 7000.0f;    // Hz
    float lofiMix  = 1.0f;

    // Vystupni EQ
    bool eqOn = true;
    std::array<EqBand, kNumEqBands> eq {};

    // Retez a vystup
    int   chainOrder = 0;
    float outGainDb  = 0.0f;

    // Transport hostitele (pro LFO v taktu)
    double bpm = 120.0;
    double ppq = 0.0;
    bool   playing = false;
};
} // namespace dusi
