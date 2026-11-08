#include "Updater.h"
#include <juce_cryptography/juce_cryptography.h>
#include "BinaryData.h"

using juce::String;

namespace
{
constexpr double kAutoCheckHours = 6.0;
String U (const char* s) { return String::fromUTF8 (s); }

juce::File stateFile()  { return Updater::folder().getChildFile ("stav.json"); }
juce::File statusFile() { return Updater::folder().getChildFile ("install-status.json"); }

juce::var readJson (const juce::File& f)
{
    return f.existsAsFile() ? juce::JSON::parse (f.loadFileAsString()) : juce::var();
}
}

//==============================================================================
juce::File Updater::folder()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory)
               .getChildFile ("DusiX DeVerb").getChildFile ("Aktualizace");
}

int Updater::compareVersions (const String& a, const String& b)
{
    auto parts = [] (const String& v)
    {
        auto t = juce::StringArray::fromTokens (v.trim().trimCharactersAtStart ("vV"), ".", {});
        std::array<int, 3> n { 0, 0, 0 };
        for (int i = 0; i < 3 && i < t.size(); ++i) n[(size_t) i] = t[i].getIntValue();
        return n;
    };
    const auto x = parts (a), y = parts (b);
    for (size_t i = 0; i < 3; ++i)
        if (x[i] != y[i]) return x[i] < y[i] ? -1 : 1;
    return 0;
}

bool Updater::parseLatestJson (const String& json, Info& out, String& err)
{
    const auto v = juce::JSON::parse (json);
    if (! v.isObject()) { err = U ("Neplatný latest.json"); return false; }
    out.version = v["version"].toString().trim().trimCharactersAtStart ("vV");
    out.notes   = v["notes"].toString();
    out.url     = v["download"].toString();
    out.sha256  = v["sha256"].toString().trim().toLowerCase();
    out.size    = (juce::int64) v["size"];
    const bool ok = out.version.containsOnly ("0123456789.") && out.version.isNotEmpty()
                 && out.url.startsWith ("https://") && out.sha256.length() == 64 && out.size > 1000;
    if (! ok) err = U ("latest.json nemá všechny údaje");
    return ok;
}

String Updater::sha256OfFile (const juce::File& f)
{
    juce::FileInputStream in (f);
    if (! in.openedOk()) return {};
    return juce::SHA256 (in).toHexString().toLowerCase();
}

//==============================================================================
Updater::Updater() : juce::Thread ("DeVerb update")
{
    folder().createDirectory();
    readInstallStatus();
    if (state.load() == State::idle)
    {
        // posledni znamy vysledek (at tlacitko hned ukaze "AKTUALIZOVAT", kdyz uz o nove verzi vime)
        const auto s = readJson (stateFile());
        if (s.isObject())
        {
            Info i;
            String e;
            if (parseLatestJson (juce::JSON::toString (s["latest"]), i, e) && compareVersions (i.version, currentVersion()) > 0)
            {
                std::lock_guard<std::mutex> g (lock);
                info = i;
                state = (zipFile().existsAsFile() && sha256OfFile (zipFile()) == i.sha256) ? State::ready : State::available;
            }
        }
    }
}

Updater::~Updater()
{
    stopThread (4000);
}

Updater::Info Updater::getInfo() const   { std::lock_guard<std::mutex> g (lock); return info; }
String Updater::getError() const          { std::lock_guard<std::mutex> g (lock); return error; }

juce::File Updater::zipFile() const
{
    return folder().getChildFile ("DusiX-DeVerb-" + info.version + "-mac.zip");
}

void Updater::fail (const String& message)
{
    {
        std::lock_guard<std::mutex> g (lock);
        error = message;
    }
    state = State::failed;
}

void Updater::check (bool force)
{
    const auto st = state.load();
    if (isThreadRunning() || st == State::downloading || st == State::scheduled || st == State::installed)
        return;
    if (! force)
    {
        const auto s = readJson (stateFile());
        const double last = s.isObject() ? (double) s["lastCheck"] : 0.0;
        if (juce::Time::currentTimeMillis() - last < kAutoCheckHours * 3600000.0)
            return;
    }
    job = Job::check;
    state = State::checking;
    startThread (juce::Thread::Priority::low);
}

void Updater::download()
{
    if (isThreadRunning() || state.load() != State::available) return;
    job = Job::download;
    progress = 0.0f;
    state = State::downloading;
    startThread (juce::Thread::Priority::low);
}

void Updater::cancel()
{
    signalThreadShouldExit();
}

void Updater::run()
{
    const auto j = job.exchange (Job::none);
    if (j == Job::check)    doCheck();
    if (j == Job::download) doDownload();
}

void Updater::saveLastCheck()
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("lastCheck", (double) juce::Time::currentTimeMillis());
    {
        std::lock_guard<std::mutex> g (lock);
        if (info.version.isNotEmpty())
        {
            auto* l = new juce::DynamicObject();
            l->setProperty ("version", info.version);
            l->setProperty ("notes", info.notes);
            l->setProperty ("download", info.url);
            l->setProperty ("sha256", info.sha256);
            l->setProperty ("size", info.size);
            o->setProperty ("latest", juce::var (l));
        }
    }
    stateFile().replaceWithText (juce::JSON::toString (juce::var (o)));
}

void Updater::doCheck()
{
    const juce::URL url ("https://github.com/" + String (repo) + "/releases/latest/download/latest.json");
    int status = 0;
    auto stream = url.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                             .withConnectionTimeoutMs (8000)
                                             .withNumRedirectsToFollow (8)
                                             .withStatusCode (&status));
    if (stream == nullptr)
    {
        state = State::offline;
        return;
    }
    const auto text = stream->readEntireStreamAsString();
    if (status == 404)          // jeste zadne vydani
    {
        saveLastCheck();
        state = State::upToDate;
        return;
    }
    Info i;
    String err;
    if (status >= 400 || ! parseLatestJson (text, i, err))
    {
        fail (err.isNotEmpty() ? err : U ("GitHub odpověděl chybou ") + String (status));
        return;
    }
    {
        std::lock_guard<std::mutex> g (lock);
        info = i;
        error.clear();
    }
    saveLastCheck();
    if (compareVersions (i.version, currentVersion()) <= 0)
        state = State::upToDate;
    else
        state = (zipFile().existsAsFile() && sha256OfFile (zipFile()) == i.sha256) ? State::ready : State::available;
}

void Updater::doDownload()
{
    const auto i = getInfo();
    const auto target = zipFile();
    const auto temp = target.withFileExtension ("part");
    temp.deleteFile();

    int status = 0;
    auto stream = juce::URL (i.url).createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                                           .withConnectionTimeoutMs (15000)
                                                           .withNumRedirectsToFollow (8)
                                                           .withStatusCode (&status));
    if (stream == nullptr || status >= 400)
    {
        fail (U ("Stažení se nepovedlo (připojení k GitHubu)"));
        return;
    }
    {
        juce::FileOutputStream out (temp);
        if (! out.openedOk())
        {
            fail (U ("Nejde zapisovat do složky ") + folder().getFullPathName());
            return;
        }
        juce::HeapBlock<char> buf (65536);
        juce::int64 got = 0;
        while (! stream->isExhausted())
        {
            if (threadShouldExit())
            {
                out.flush();
                temp.deleteFile();
                state = State::available;
                return;
            }
            const int n = stream->read (buf, 65536);
            if (n <= 0) break;
            out.write (buf, (size_t) n);
            got += n;
            progress = (float) juce::jlimit (0.0, 1.0, (double) got / (double) juce::jmax<juce::int64> (1, i.size));
            if (got > i.size + 1024) break;
        }
        out.flush();
    }
    if (temp.getSize() != i.size)
    {
        temp.deleteFile();
        fail (U ("Stažený soubor má špatnou velikost – zkus to znovu"));
        return;
    }
    if (sha256OfFile (temp) != i.sha256)
    {
        temp.deleteFile();
        fail (U ("Kontrolní součet nesedí – soubor se nepoužije"));
        return;
    }
    target.deleteFile();
    temp.moveFileTo (target);
    progress = 1.0f;
    state = State::ready;
}

//==============================================================================
void Updater::install()
{
    if (state.load() != State::ready) return;
    const auto i = getInfo();
    const auto script = folder().getChildFile ("deverb-install.sh");
    if (! script.replaceWithData (BinaryData::deverbinstall_sh, BinaryData::deverbinstall_shSize))
    {
        fail (U ("Nejde zapsat instalační skript"));
        return;
    }
    script.setExecutePermission (true);
    statusFile().deleteFile();

    auto q = [] (const String& s) { return "'" + s.replace ("'", "'\\''") + "'"; };
    const String cmd = "/usr/bin/nohup /bin/bash " + q (script.getFullPathName()) + " " + q (zipFile().getFullPathName())
                     + " " + q (i.version) + " " + q (statusFile().getFullPathName())
                     + " > " + q (folder().getChildFile ("install.log").getFullPathName()) + " 2>&1 &";
    if (std::system (cmd.toRawUTF8()) != 0)
    {
        fail (U ("Instalátor se nespustil"));
        return;
    }
    state = State::scheduled;
}

void Updater::readInstallStatus()
{
    // vysledek posledni instalace (zapisuje ho skript)
    const auto s = readJson (statusFile());
    if (! s.isObject()) return;
    const auto st = s["state"].toString();
    const auto ver = s["version"].toString();
    if (st == "waiting" || st == "installing")
    {
        std::lock_guard<std::mutex> g (lock);
        info.version = ver;
        state = State::scheduled;
    }
    else if (st == "done" && compareVersions (ver, currentVersion()) > 0)
    {
        std::lock_guard<std::mutex> g (lock);
        info.version = ver;
        state = State::installed;     // nainstalovano, ale v pameti bezi jeste stara verze
    }
    else if (st == "failed")
    {
        std::lock_guard<std::mutex> g (lock);
        error = U ("Instalace selhala: ") + s["message"].toString();
        statusFile().deleteFile();
        state = State::failed;
    }
    else if (st == "done")
        statusFile().deleteFile();    // uz bezi nova verze
}
