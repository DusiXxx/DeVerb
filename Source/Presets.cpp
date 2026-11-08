#include "Presets.h"

// Rychly prehled hodnot vyberu:
//  reverbType: 0 Plate, 1 Room, 2 Chamber, 3 Hall, 4 Spring
//  delayDiv:   0 1/32, 1 1/16T, 2 1/16, 3 1/8T, 4 1/16., 5 1/8, 6 1/4T, 7 1/8., 8 1/4, 9 1/2T, 10 1/4., 11 1/2
//  preDelayDiv:0 1/128, 1 1/64, 2 1/32T, 3 1/32, 4 1/16T, 5 1/16, 6 1/8T, 7 1/8
//  decayDiv:   0 1/4, 1 1/2, 2 1/2., 3 1 takt, 4 2 takty, 5 4 takty
//  lfoDiv:     0 1/32, 1 1/16, 2 1/8T, 3 1/8, 4 1/4T, 5 1/4, 6 1/2, 7 1 takt, 8 2 takty, 9 4 takty
//  filterType: 0 LP, 1 BP, 2 HP     lfoShape: 0 sinus, 1 trojuhelnik, 2 pila, 3 obdelnik, 4 nahodny
//  chainOrder: 0 D>R>F>C>L, 1 D>F>R>C>L, 2 R>D>F>C>L, 3 R>F>D>C>L, 4 F>D>R>C>L, 5 F>R>D>C>L,
//              72 C>D>R>F>L (chorus pred delayem a reverbem)  (D delay, R reverb, F filtr, C chorus, L lo-fi)
//  eqNType:    0 Low Cut, 1 Low Shelf, 2 Bell, 3 High Shelf, 4 High Cut, 5 Notch   eqNSlope: 0/1/2 = 12/24/48 dB

const std::vector<const char*>& presetCategories()
{
    static const std::vector<const char*> c { "Univerzální", "Vokály", "Bicí", "Piano & klávesy", "Synthy",
                                              "Kytara", "Kreativní", "Mix busy" };
    return c;
}

const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> list
    {
        // ---------------------------------------------------------------- Univerzální
        { "Výchozí", "Univerzální", {} },
        { "Malá místnost", "Univerzální", { { "reverbType", 1 }, { "delayOn", 0 }, { "preDelay", 3 }, { "size", 40 },
            { "decay", 0.6f }, { "highCut", 9000 }, { "lowCut", 200 }, { "mod", 10 }, { "early", 45 }, { "width", 85 },
            { "reverbMix", 22 }, { "depth", 25 } } },
        { "Studio – dřevěná místnost", "Univerzální", { { "reverbType", 2 }, { "delayOn", 0 }, { "preDelay", 6 },
            { "size", 60 }, { "decay", 0.9f }, { "highCut", 6500 }, { "lowCut", 180 }, { "mod", 10 }, { "early", 40 },
            { "width", 90 }, { "reverbMix", 18 }, { "depth", 30 }, { "decayEqOn", 1 }, { "dq4Mult", 0.7f } } },
        { "Klasický plate", "Univerzální", { { "reverbType", 0 }, { "delayOn", 0 }, { "preDelay", 12 }, { "size", 90 },
            { "decay", 1.8f }, { "highCut", 10000 }, { "lowCut", 250 }, { "mod", 20 }, { "early", 0 }, { "reverbMix", 22 } } },
        { "Velký sál", "Univerzální", { { "reverbType", 3 }, { "delayOn", 0 }, { "preDelay", 28 }, { "size", 140 },
            { "decay", 3.2f }, { "highCut", 7000 }, { "lowCut", 150 }, { "mod", 35 }, { "early", 30 }, { "reverbMix", 28 },
            { "depth", 55 }, { "decayEqOn", 1 }, { "dq1Mult", 0.7f }, { "dq4Mult", 0.6f } } },
        { "Katedrála", "Univerzální", { { "reverbType", 3 }, { "delayOn", 0 }, { "preDelay", 45 }, { "size", 150 },
            { "decay", 7.5f }, { "highCut", 5000 }, { "lowCut", 200 }, { "mod", 45 }, { "early", 20 }, { "reverbMix", 35 },
            { "depth", 75 }, { "decayEqOn", 1 }, { "dq1Mult", 0.8f }, { "dq4Mult", 0.5f } } },
        { "Daleko v pozadí", "Univerzální", { { "reverbType", 3 }, { "delayOn", 0 }, { "preDelay", 20 }, { "size", 120 },
            { "decay", 2.6f }, { "highCut", 5500 }, { "lowCut", 200 }, { "mod", 30 }, { "early", 35 }, { "reverbMix", 40 },
            { "depth", 90 } } },

        // ---------------------------------------------------------------- Vokály
        { "Vokál – plate v tempu", "Vokály", { { "reverbType", 0 }, { "delaySync", 1 }, { "delayDiv", 7 },
            { "delayFeedback", 20 }, { "delayTone", 4500 }, { "delayMix", 12 }, { "preDelaySync", 1 }, { "preDelayDiv", 1 },
            { "decaySync", 1 }, { "decayDiv", 1 }, { "size", 85 }, { "highCut", 8500 }, { "lowCut", 300 }, { "mod", 25 },
            { "early", 0 }, { "reverbMix", 20 }, { "depth", 30 }, { "monoBass", 200 }, { "eq1On", 1 }, { "eq1Freq", 90 } } },
        { "Rap – suchý a blízko", "Vokály", { { "reverbType", 1 }, { "delayOn", 0 }, { "preDelay", 8 }, { "size", 35 },
            { "decay", 0.45f }, { "highCut", 8000 }, { "lowCut", 300 }, { "early", 35 }, { "reverbMix", 12 }, { "depth", 15 },
            { "monoBass", 250 }, { "eq1On", 1 }, { "eq1Freq", 80 } } },
        { "Zpěv – široký hall", "Vokály", { { "reverbType", 3 }, { "delaySync", 1 }, { "delayDiv", 8 },
            { "delayFeedback", 25 }, { "delayTone", 4000 }, { "delayMix", 10 }, { "pingPong", 1 }, { "preDelaySync", 1 },
            { "preDelayDiv", 3 }, { "size", 120 }, { "decay", 2.4f }, { "highCut", 7000 }, { "lowCut", 350 }, { "mod", 40 },
            { "early", 20 }, { "reverbMix", 25 }, { "depth", 35 }, { "decayEqOn", 1 }, { "dq1Mult", 0.6f },
            { "monoBass", 250 } } },
        { "Back vokály – vzadu", "Vokály", { { "reverbType", 2 }, { "delayOn", 0 }, { "preDelay", 15 }, { "size", 100 },
            { "decay", 1.6f }, { "highCut", 6000 }, { "lowCut", 300 }, { "early", 40 }, { "width", 110 },
            { "reverbMix", 35 }, { "depth", 80 } } },
        { "Vocal throw – ping-pong 1/4", "Vokály", { { "delaySync", 1 }, { "delayDiv", 8 }, { "delayFeedback", 45 },
            { "delayTone", 3500 }, { "pingPong", 1 }, { "delayMix", 30 }, { "reverbType", 0 }, { "size", 90 },
            { "decay", 2.5f }, { "highCut", 6000 }, { "lowCut", 400 }, { "early", 0 }, { "reverbMix", 18 } } },

        // ---------------------------------------------------------------- Bicí
        { "Snare – krátký plate v tempu", "Bicí", { { "reverbType", 0 }, { "delayOn", 0 }, { "preDelaySync", 1 },
            { "preDelayDiv", 0 }, { "decaySync", 1 }, { "decayDiv", 0 }, { "size", 70 }, { "highCut", 11000 },
            { "lowCut", 350 }, { "mod", 10 }, { "early", 0 }, { "reverbMix", 30 }, { "monoBass", 250 } } },
        { "Snare – room", "Bicí", { { "reverbType", 1 }, { "delayOn", 0 }, { "preDelay", 2 }, { "size", 55 },
            { "decay", 0.8f }, { "highCut", 9000 }, { "lowCut", 250 }, { "early", 60 }, { "reverbMix", 30 }, { "depth", 35 } } },
        { "Clap – hall v tempu", "Bicí", { { "reverbType", 3 }, { "delayOn", 0 }, { "preDelaySync", 1 }, { "preDelayDiv", 1 },
            { "decaySync", 1 }, { "decayDiv", 1 }, { "size", 110 }, { "highCut", 8000 }, { "lowCut", 400 }, { "mod", 25 },
            { "early", 25 }, { "reverbMix", 30 }, { "decayEqOn", 1 }, { "dq1Mult", 0.5f } } },
        { "Hi-hat – jemný vzduch", "Bicí", { { "reverbType", 2 }, { "delayOn", 0 }, { "preDelay", 5 }, { "size", 45 },
            { "decay", 0.7f }, { "highCut", 14000 }, { "lowCut", 1500 }, { "early", 30 }, { "reverbMix", 18 } } },
        { "Bicí sada – místnost", "Bicí", { { "reverbType", 1 }, { "delayOn", 0 }, { "preDelay", 0 }, { "size", 65 },
            { "decay", 0.9f }, { "highCut", 8000 }, { "lowCut", 200 }, { "early", 70 }, { "reverbMix", 15 },
            { "monoBass", 300 }, { "depth", 30 } } },

        // ---------------------------------------------------------------- Synth & pady
        { "Ambient pad", "Synthy", { { "delaySync", 1 }, { "delayDiv", 10 }, { "delayFeedback", 55 },
            { "delayTone", 3500 }, { "pingPong", 1 }, { "delayMix", 25 }, { "reverbType", 3 }, { "preDelay", 40 },
            { "size", 150 }, { "decay", 9.0f }, { "highCut", 6000 }, { "lowCut", 300 }, { "mod", 70 }, { "early", 10 },
            { "reverbMix", 50 }, { "depth", 60 } } },
        { "Lead – echo 1/8.", "Synthy", { { "delaySync", 1 }, { "delayDiv", 7 }, { "delayFeedback", 40 },
            { "delayTone", 5000 }, { "delayMix", 22 }, { "reverbType", 0 }, { "preDelay", 15 }, { "size", 85 },
            { "decay", 1.6f }, { "early", 0 }, { "reverbMix", 15 }, { "lowCut", 300 } } },
        { "Pluck – ping-pong + plate", "Synthy", { { "delaySync", 1 }, { "delayDiv", 5 }, { "delayFeedback", 35 },
            { "delayTone", 6000 }, { "pingPong", 1 }, { "delayMix", 25 }, { "reverbType", 0 }, { "size", 75 },
            { "decay", 1.4f }, { "early", 0 }, { "reverbMix", 22 }, { "lowCut", 250 } } },
        { "Pad – pomalý filtr", "Synthy", { { "delayOn", 0 }, { "filterOn", 1 }, { "filterType", 0 },
            { "filterFreq", 1500 }, { "filterReso", 30 }, { "lfoSync", 1 }, { "lfoDiv", 8 }, { "lfoDepth", 45 },
            { "lfoShape", 1 }, { "lfoStereo", 60 }, { "filterMix", 100 }, { "chainOrder", 0 }, { "reverbType", 3 },
            { "size", 130 }, { "decay", 4.0f }, { "reverbMix", 35 } } },

        // ---------------------------------------------------------------- Kytara & klavír
        { "Kytara – pokoj", "Kytara", { { "reverbType", 1 }, { "delayOn", 0 }, { "preDelay", 10 }, { "size", 55 },
            { "decay", 1.1f }, { "highCut", 7500 }, { "lowCut", 150 }, { "mod", 30 }, { "early", 45 }, { "reverbMix", 20 },
            { "depth", 40 } } },
        { "Kytara – spring", "Kytara", { { "reverbType", 4 }, { "delayOn", 0 }, { "preDelay", 0 }, { "size", 80 },
            { "decay", 2.2f }, { "highCut", 5000 }, { "lowCut", 200 }, { "early", 0 }, { "reverbMix", 28 }, { "depth", 35 } } },
        { "Klavír – sál", "Piano & klávesy", { { "reverbType", 3 }, { "delayOn", 0 }, { "preDelay", 20 }, { "size", 120 },
            { "decay", 2.4f }, { "highCut", 7500 }, { "lowCut", 100 }, { "mod", 20 }, { "early", 30 }, { "reverbMix", 22 },
            { "depth", 45 }, { "decayEqOn", 1 }, { "dq4Mult", 0.7f } } },
        { "Klavír – komorní", "Piano & klávesy", { { "reverbType", 2 }, { "delayOn", 0 }, { "preDelay", 10 }, { "size", 75 },
            { "decay", 1.3f }, { "highCut", 9000 }, { "lowCut", 120 }, { "early", 40 }, { "reverbMix", 18 }, { "depth", 35 } } },

        // ---------------------------------------------------------------- Kreativní
        { "Wobble 1/4", "Kreativní", { { "delayOn", 0 }, { "filterOn", 1 }, { "filterType", 0 }, { "filterFreq", 700 },
            { "filterReso", 65 }, { "lfoSync", 1 }, { "lfoDiv", 5 }, { "lfoDepth", 70 }, { "lfoShape", 0 },
            { "filterMix", 100 }, { "chainOrder", 4 }, { "reverbType", 0 }, { "early", 0 }, { "reverbMix", 15 } } },
        { "Filtrovaný delay", "Kreativní", { { "delaySync", 1 }, { "delayDiv", 7 }, { "delayFeedback", 55 },
            { "delayMix", 35 }, { "pingPong", 1 }, { "filterOn", 1 }, { "filterType", 1 }, { "filterFreq", 1200 },
            { "filterReso", 50 }, { "lfoSync", 1 }, { "lfoDiv", 7 }, { "lfoDepth", 55 }, { "lfoShape", 2 },
            { "lfoStereo", 90 }, { "filterMix", 45 }, { "chainOrder", 1 }, { "reverbMix", 15 } } },
        { "Náhodné kapky", "Kreativní", { { "filterOn", 1 }, { "filterType", 0 }, { "filterFreq", 2000 },
            { "filterReso", 55 }, { "lfoSync", 1 }, { "lfoDiv", 3 }, { "lfoDepth", 60 }, { "lfoShape", 4 },
            { "lfoStereo", 120 }, { "filterMix", 100 }, { "chainOrder", 2 }, { "delaySync", 1 }, { "delayDiv", 5 },
            { "delayFeedback", 30 }, { "delayMix", 25 }, { "reverbType", 2 }, { "reverbMix", 25 } } },
        { "Rozmazaný prostor", "Kreativní", { { "reverbType", 3 }, { "delayOn", 0 }, { "preDelay", 0 }, { "size", 150 },
            { "decay", 12.0f }, { "highCut", 4500 }, { "lowCut", 250 }, { "mod", 100 }, { "early", 0 }, { "reverbMix", 60 },
            { "depth", 85 }, { "decayEqOn", 1 }, { "dq1Mult", 0.6f }, { "dq2Mult", 1.3f } } },

        // ---------------------------------------------------------------- Piano & klávesy (v0.4)
        { "Piano – intimní pokoj", "Piano & klávesy", { { "reverbType", 1 }, { "delayOn", 0 }, { "preDelay", 6 }, { "size", 45 },
            { "decay", 0.7f }, { "highCut", 9000 }, { "lowCut", 120 }, { "early", 45 }, { "reverbMix", 18 }, { "depth", 25 } } },
        { "Piano – koncertní sál", "Piano & klávesy", { { "reverbType", 3 }, { "delayOn", 0 }, { "preDelay", 24 }, { "size", 135 },
            { "decay", 2.8f }, { "highCut", 7500 }, { "lowCut", 90 }, { "mod", 20 }, { "early", 35 }, { "reverbMix", 26 },
            { "depth", 50 }, { "decayEqOn", 1 }, { "dq1Mult", 0.75f }, { "dq4Mult", 0.7f } } },
        { "Piano – balada (plate)", "Piano & klávesy", { { "reverbType", 0 }, { "preDelay", 18 }, { "size", 95 }, { "decay", 2.2f },
            { "highCut", 8000 }, { "lowCut", 150 }, { "mod", 25 }, { "early", 0 }, { "reverbMix", 24 }, { "delaySync", 1 },
            { "delayDiv", 10 }, { "delayFeedback", 20 }, { "delayTone", 4000 }, { "delayMix", 8 }, { "duckAmount", 40 } } },
        { "Piano – felt (tlumené)", "Piano & klávesy", { { "reverbType", 2 }, { "delayOn", 0 }, { "size", 70 }, { "decay", 1.5f },
            { "highCut", 5000 }, { "lowCut", 150 }, { "early", 30 }, { "reverbMix", 22 }, { "depth", 45 },
            { "eq7On", 1 }, { "eq7Type", 3 }, { "eq7Freq", 9000 }, { "eq7Gain", -3 }, { "eq7Q", 0.7f } } },
        { "Piano – lo-fi beat", "Piano & klávesy", { { "reverbType", 0 }, { "delayOn", 0 }, { "size", 70 }, { "decay", 1.4f },
            { "early", 0 }, { "reverbMix", 20 }, { "lofiOn", 1 }, { "lofiBits", 11 }, { "lofiRate", 16000 }, { "lofiTone", 6500 },
            { "chorusOn", 1 }, { "chorusRate", 0.4f }, { "chorusDepth", 35 }, { "chorusMix", 35 }, { "eq1On", 1 }, { "eq1Freq", 80 } } },
        { "Piano – ambient shimmer", "Piano & klávesy", { { "reverbType", 3 }, { "preDelay", 40 }, { "size", 150 }, { "decay", 7.0f },
            { "shimmer", 40 }, { "mod", 50 }, { "highCut", 7000 }, { "lowCut", 250 }, { "early", 15 }, { "reverbMix", 45 },
            { "depth", 65 }, { "delaySync", 1 }, { "delayDiv", 10 }, { "delayFeedback", 35 }, { "delayMix", 15 }, { "duckAmount", 50 } } },
        { "Piano – nekonečná plocha (zkus FREEZE)", "Piano & klávesy", { { "reverbType", 3 }, { "delayOn", 0 }, { "size", 140 },
            { "decay", 12.0f }, { "shimmer", 20 }, { "mod", 60 }, { "highCut", 6000 }, { "lowCut", 200 }, { "early", 10 },
            { "reverbMix", 55 }, { "depth", 70 } } },
        { "Rhodes – chorus + plate", "Piano & klávesy", { { "chorusOn", 1 }, { "chorusRate", 0.8f }, { "chorusDepth", 50 },
            { "chorusMix", 40 }, { "chainOrder", 72 }, { "reverbType", 0 }, { "size", 80 }, { "decay", 1.6f }, { "early", 0 },
            { "reverbMix", 18 }, { "delaySync", 1 }, { "delayDiv", 7 }, { "delayFeedback", 18 }, { "delayMix", 10 } } },
        { "Varhany – kostel", "Piano & klávesy", { { "reverbType", 3 }, { "delayOn", 0 }, { "preDelay", 30 }, { "size", 150 },
            { "decay", 5.0f }, { "highCut", 6000 }, { "lowCut", 100 }, { "early", 30 }, { "reverbMix", 35 }, { "depth", 70 },
            { "decayEqOn", 1 }, { "dq1Mult", 1.3f }, { "dq4Mult", 0.6f } } },
        { "Klávesy – široké stereo", "Piano & klávesy", { { "reverbType", 2 }, { "chorusOn", 1 }, { "chorusDepth", 60 },
            { "chorusMix", 50 }, { "chainOrder", 72 }, { "pingPong", 1 }, { "delaySync", 1 }, { "delayDiv", 8 },
            { "delayFeedback", 25 }, { "delayMix", 12 }, { "reverbMix", 20 } } },

        // ---------------------------------------------------------------- Synthy (v0.4)
        { "Supersaw lead – hall + 1/8.", "Synthy", { { "reverbType", 3 }, { "size", 120 }, { "decay", 2.2f }, { "reverbMix", 22 },
            { "delaySync", 1 }, { "delayDiv", 7 }, { "delayFeedback", 35 }, { "delayMix", 22 }, { "duckAmount", 60 },
            { "compAmount", 20 }, { "lowCut", 250 } } },
        { "Arp 1/16 – ping-pong", "Synthy", { { "delaySync", 1 }, { "delayDiv", 2 }, { "delayFeedback", 40 }, { "pingPong", 1 },
            { "delayMix", 25 }, { "reverbType", 0 }, { "size", 70 }, { "decay", 1.2f }, { "early", 0 }, { "reverbMix", 15 },
            { "duckAmount", 30 } } },
        { "Pad – shimmer nebe", "Synthy", { { "delayOn", 0 }, { "reverbType", 3 }, { "size", 150 }, { "decay", 8.0f },
            { "shimmer", 55 }, { "mod", 60 }, { "reverbMix", 50 }, { "lowCut", 250 }, { "chorusOn", 1 }, { "chorusRate", 0.3f },
            { "chorusDepth", 40 }, { "chorusMix", 30 }, { "chainOrder", 72 } } },
        { "Pad – chorus + sál", "Synthy", { { "delayOn", 0 }, { "chorusOn", 1 }, { "chorusRate", 0.5f }, { "chorusDepth", 45 },
            { "chorusMix", 50 }, { "chainOrder", 72 }, { "reverbType", 3 }, { "size", 130 }, { "decay", 4.0f },
            { "reverbMix", 35 } } },
        { "Bass – jen trocha prostoru", "Synthy", { { "delayOn", 0 }, { "reverbType", 1 }, { "size", 35 }, { "decay", 0.4f },
            { "reverbMix", 8 }, { "lowCut", 250 }, { "monoBass", 300 }, { "width", 70 }, { "early", 30 } } },
        { "Stab – krátký plate", "Synthy", { { "reverbType", 0 }, { "preDelay", 10 }, { "size", 60 }, { "decay", 0.8f },
            { "early", 0 }, { "reverbMix", 25 }, { "lowCut", 300 }, { "delaySync", 1 }, { "delayDiv", 8 },
            { "delayFeedback", 15 }, { "delayMix", 10 }, { "duckAmount", 30 } } },
        { "Synthwave – 80s hall", "Synthy", { { "chorusOn", 1 }, { "chorusRate", 0.7f }, { "chorusDepth", 55 }, { "chorusMix", 45 },
            { "chainOrder", 72 }, { "reverbType", 3 }, { "size", 140 }, { "decay", 3.5f }, { "highCut", 7000 },
            { "reverbMix", 30 }, { "delaySync", 1 }, { "delayDiv", 10 }, { "delayFeedback", 30 }, { "delayMix", 18 } } },
        { "Pluck – dub delay", "Synthy", { { "delaySync", 1 }, { "delayDiv", 7 }, { "delayFeedback", 60 }, { "delayTone", 2500 },
            { "delayMix", 30 }, { "pingPong", 1 }, { "filterOn", 1 }, { "filterType", 1 }, { "filterFreq", 1500 },
            { "filterReso", 30 }, { "lfoDepth", 30 }, { "lfoDiv", 8 }, { "filterMix", 40 }, { "chainOrder", 1 },
            { "reverbType", 0 }, { "reverbMix", 15 } } },
        { "Lead – široký s duckingem", "Synthy", { { "delaySync", 1 }, { "delayDiv", 8 }, { "delayFeedback", 35 }, { "delayMix", 30 },
            { "pingPong", 1 }, { "reverbType", 2 }, { "size", 90 }, { "decay", 1.8f }, { "reverbMix", 30 }, { "duckAmount", 80 },
            { "duckRelease", 300 }, { "compAmount", 30 } } },
        { "Chords – lo-fi tape", "Synthy", { { "delayOn", 0 }, { "chorusOn", 1 }, { "chorusRate", 0.25f }, { "chorusDepth", 70 },
            { "chorusMix", 45 }, { "lofiOn", 1 }, { "lofiBits", 12 }, { "lofiRate", 22000 }, { "lofiTone", 6000 },
            { "reverbType", 1 }, { "size", 60 }, { "decay", 1.0f }, { "reverbMix", 20 } } },
        { "Pad – čisté basy", "Synthy", { { "delayOn", 0 }, { "reverbType", 3 }, { "size", 130 }, { "decay", 5.0f },
            { "reverbMix", 40 }, { "lowCut", 300 }, { "monoBass", 300 }, { "decayEqOn", 1 }, { "dq1Mult", 0.4f } } },
        { "Bell – spring", "Synthy", { { "reverbType", 4 }, { "size", 80 }, { "decay", 2.0f }, { "early", 0 }, { "reverbMix", 25 },
            { "delaySync", 1 }, { "delayDiv", 5 }, { "delayFeedback", 25 }, { "delayMix", 15 } } },

        // ---------------------------------------------------------------- Vokály (v0.4)
        { "Vokál – ducked plate", "Vokály", { { "reverbType", 0 }, { "size", 90 }, { "decay", 2.4f }, { "preDelay", 25 },
            { "highCut", 8000 }, { "lowCut", 300 }, { "early", 0 }, { "reverbMix", 30 }, { "delaySync", 1 }, { "delayDiv", 7 },
            { "delayFeedback", 30 }, { "delayMix", 20 }, { "duckAmount", 70 }, { "duckRelease", 250 }, { "monoBass", 250 } } },
        { "Vokál – lo-fi rádio", "Vokály", { { "delayOn", 0 }, { "reverbType", 1 }, { "size", 40 }, { "decay", 0.6f },
            { "reverbMix", 15 }, { "lofiOn", 1 }, { "lofiBits", 9 }, { "lofiRate", 11000 }, { "lofiTone", 3500 }, { "lofiMix", 100 },
            { "eq1On", 1 }, { "eq1Freq", 350 }, { "eq1Slope", 2 } } },

        // ---------------------------------------------------------------- Kreativní (v0.4)
        { "Shimmer zvon", "Kreativní", { { "delayOn", 0 }, { "reverbType", 0 }, { "size", 120 }, { "decay", 6.0f }, { "shimmer", 70 },
            { "mod", 40 }, { "highCut", 9000 }, { "lowCut", 400 }, { "early", 0 }, { "reverbMix", 50 } } },
        { "Freeze drone (zapni FREEZE)", "Kreativní", { { "delayOn", 0 }, { "reverbType", 3 }, { "size", 150 }, { "decay", 10.0f },
            { "mod", 80 }, { "early", 0 }, { "reverbMix", 70 }, { "chorusOn", 1 }, { "chorusRate", 0.2f }, { "chorusDepth", 50 },
            { "chorusMix", 40 }, { "lowCut", 200 } } },
        { "Rozbitý sampler", "Kreativní", { { "delayOn", 0 }, { "lofiOn", 1 }, { "lofiBits", 6 }, { "lofiRate", 6000 },
            { "lofiTone", 5000 }, { "lofiMix", 100 }, { "reverbType", 2 }, { "size", 60 }, { "decay", 1.2f }, { "reverbMix", 25 } } },
        { "Echo s duckingem 1/4", "Kreativní", { { "delaySync", 1 }, { "delayDiv", 8 }, { "delayFeedback", 55 }, { "delayMix", 40 },
            { "pingPong", 1 }, { "duckAmount", 90 }, { "duckRelease", 400 }, { "reverbMix", 15 } } },

        // ---------------------------------------------------------------- Mix busy
        { "Bus – lepidlo", "Mix busy", { { "reverbType", 2 }, { "delayOn", 0 }, { "preDelay", 0 }, { "size", 50 },
            { "decay", 0.8f }, { "highCut", 8000 }, { "lowCut", 300 }, { "mod", 10 }, { "early", 30 }, { "width", 85 },
            { "reverbMix", 8 }, { "depth", 30 }, { "monoBass", 250 } } },
        { "Drum bus – room", "Mix busy", { { "reverbType", 1 }, { "delayOn", 0 }, { "preDelay", 0 }, { "size", 60 },
            { "decay", 0.7f }, { "highCut", 7000 }, { "lowCut", 250 }, { "early", 60 }, { "reverbMix", 10 },
            { "monoBass", 300 } } },
        { "Master – jemný vzduch", "Mix busy", { { "reverbType", 2 }, { "delayOn", 0 }, { "size", 50 }, { "decay", 0.6f },
            { "highCut", 12000 }, { "lowCut", 400 }, { "early", 20 }, { "reverbMix", 4 }, { "monoBass", 300 },
            { "eq1On", 1 }, { "eq1Type", 0 }, { "eq1Freq", 25 }, { "eq1Slope", 1 },
            { "eq7On", 1 }, { "eq7Type", 3 }, { "eq7Freq", 12000 }, { "eq7Gain", 1.5f }, { "eq7Q", 0.7f } } },
    };
    return list;
}
