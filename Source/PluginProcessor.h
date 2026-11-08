#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include <unordered_map>
#include <string>
#include "Dsp.h"
#include "PresetManager.h"

class DusiVerbProcessor : public juce::AudioProcessor
{
public:
    DusiVerbProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    void reset() override { engine.reset(); }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                        { return true; }

    const juce::String getName() const override            { return "DusiX DeVerb"; }
    bool acceptsMidi() const override                      { return false; }
    bool producesMidi() const override                     { return false; }
    double getTailLengthSeconds() const override           { return 20.0; }

    // Tovarni presety (Logic je ukazuje i ve svem menu presetu)
    int getNumPrograms() override;
    int getCurrentProgram() override                       { return juce::jmax (0, currentPreset); }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static const juce::StringArray& divisionNames();
    static const juce::StringArray& preDelayDivisionNames();
    static const juce::StringArray& decayDivisionNames();
    static const juce::StringArray& lfoDivisionNames();
    static const juce::StringArray& reverbTypeNames();
    static const juce::StringArray& eqTypeNames();
    static juce::String eqId (int band, const char* what);   // napr. "eq3Freq"
    static juce::String dqId (int band, const char* what);   // napr. "dq2Mult"

    // --- presety, A/B, zpet ---
    juce::String getPresetName() const                     { return presetName; }
    void loadUserPreset (const juce::File& file);
    bool saveUserPreset (const juce::String& name, juce::String& error);
    void randomize();
    PresetManager& getPresetManager()                      { return presets; }
    juce::UndoManager& getUndoManager()                    { return undoManager; }

    int  getAbSlot() const                                 { return abSlot; }
    void selectAb (int slot);
    void copyAbToOther();

    // --- pro GUI ---
    dusi::Params currentParams() const;                    // aktualni hodnoty bez vedlejsich efektu
    int  readAnalyzer (float* dest, int maxSamples);       // posledni vzorky vystupu pro analyzator
    std::atomic<bool> eco { false };

    // Hodnoty pro GUI (zapisuje audio vlakno, cte editor)
    std::atomic<float> uiCorrelation { 1.0f }, uiAutoGain { 1.0f }, uiLevel { 0.0f }, uiLfoPhase { 0.0f }, uiDuckGain { 1.0f };
    std::atomic<float> uiDelayMs { 0.0f }, uiPreDelayMs { 0.0f }, uiDecaySec { 0.0f }, uiLfoHz { 0.0f };
    std::atomic<double> uiBpm { 120.0 };
    double getSampleRateForUi() const                      { return lastSampleRate; }

private:
    juce::UndoManager undoManager;   // musi byt pred apvts

public:
    juce::AudioProcessorValueTreeState apvts;

private:
    dusi::Params readParams();
    void applyValues (const juce::ValueTree& state);       // nastavi parametry jako uzivatel (jde vratit Zpet)

    std::unordered_map<std::string, std::atomic<float>*> paramPtr;
    std::atomic<float>* eqPtr[dusi::kNumEqBands][6] {};
    std::atomic<float>* dqPtr[dusi::kNumDecayBands][3] {};

    dusi::Engine engine;
    juce::AudioBuffer<float> scratch;
    PresetManager presets;
    double lastBpm = 120.0, lastPpq = 0.0, lastSampleRate = 48000.0;
    bool lastPlaying = false;
    int currentPreset = 0;
    juce::String presetName { juce::String::fromUTF8 ("Výchozí") };

    juce::ValueTree abStates[2];
    int abSlot = 0;

    // analyzator: kruhovy buffer mono vystupu
    static constexpr int kFifoSize = 16384;
    juce::AbstractFifo fifo { kFifoSize };
    std::vector<float> fifoData = std::vector<float> (kFifoSize, 0.0f);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DusiVerbProcessor)
};
