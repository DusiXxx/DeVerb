#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Uzivatelske presety (soubory .dvpreset v ~/Music/DusiX DeVerb/Presety) a oblibene.
// Soubor jde poslat kamaradovi - staci ho dat do stejne slozky.
class PresetManager
{
public:
    static constexpr const char* kExtension = ".dvpreset";

    juce::File getFolder() const;
    juce::Array<juce::File> getUserPresets() const;

    bool save (const juce::String& name, const juce::ValueTree& state, juce::String& error) const;
    juce::ValueTree load (const juce::File& file) const;
    bool remove (const juce::File& file) const;
    void revealFolder() const;

    // Oblibene: klic "f:<nazev>" pro tovarni, "u:<soubor>" pro vlastni
    bool isFavourite (const juce::String& key) const;
    void setFavourite (const juce::String& key, bool fav);

private:
    void loadFavourites() const;
    void storeFavourites() const;
    juce::File favouritesFile() const { return getFolder().getParentDirectory().getChildFile ("oblibene.txt"); }

    mutable juce::StringArray favourites;
    mutable bool favouritesLoaded = false;
};
