#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <atomic>
#include <mutex>

#ifndef JucePlugin_VersionString
 #define JucePlugin_VersionString "0.6.1"
#endif

// Aktualizace z GitHubu (vydani obsahuje latest.json + zip s pluginem).
// Jedna instance pro vsechny otevrene DeVerby (SharedResourcePointer), prace bezi na vlastnim vlakne.
//
//   check()    -> stahne latest.json, porovna verzi
//   download() -> stahne zip, overi velikost a SHA-256
//   install()  -> spusti odpojeny skript: pocka, az se Logic zavre, zazalohuje stary plugin,
//                 nainstaluje novy (AU + VST3), odstrani karantenu, obnovi seznam pluginu
class Updater : private juce::Thread
{
public:
    static constexpr const char* repo = "DusiXxx/DeVerb";

    enum class State
    {
        idle,          // jeste nekontrolovano
        checking,
        upToDate,
        offline,       // nepodarilo se spojit
        available,     // je nova verze
        downloading,
        ready,         // stazeno a overeno, ceka na klik Instalovat
        scheduled,     // instalator ceka na zavreni Logicu
        installed,     // nova verze nainstalovana, staci znovu otevrit Logic
        failed
    };

    struct Info
    {
        juce::String version, notes, url, sha256;
        juce::int64 size = 0;
    };

    Updater();
    ~Updater() override;

    void check (bool force);          // force = i kdyz se kontrolovalo nedavno
    void download();
    void install();
    void cancel();

    State getState() const            { return state.load(); }
    float getProgress() const         { return progress.load(); }
    Info getInfo() const;
    juce::String getError() const;

    static juce::String currentVersion() { return JucePlugin_VersionString; }
    static int compareVersions (const juce::String& a, const juce::String& b);   // <0 a starsi, 0 stejne, >0 a novejsi
    static bool parseLatestJson (const juce::String& json, Info& out, juce::String& error);
    static juce::String sha256OfFile (const juce::File&);
    static juce::File folder();       // ~/Music/DusiX DeVerb/Aktualizace

private:
    enum class Job { none, check, download };
    void run() override;
    void doCheck();
    void doDownload();
    void fail (const juce::String& message);
    void readInstallStatus();
    void saveLastCheck();
    juce::File zipFile() const;

    std::atomic<State> state { State::idle };
    std::atomic<float> progress { 0.0f };
    std::atomic<Job> job { Job::none };
    mutable std::mutex lock;
    Info info;
    juce::String error;
};
