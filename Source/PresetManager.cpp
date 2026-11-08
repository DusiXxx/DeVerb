#include "PresetManager.h"

juce::File PresetManager::getFolder() const
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory)
               .getChildFile ("DusiX DeVerb").getChildFile ("Presety");
}

juce::Array<juce::File> PresetManager::getUserPresets() const
{
    auto files = getFolder().findChildFiles (juce::File::findFiles, false, juce::String ("*") + kExtension);
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
               { return a.getFileNameWithoutExtension().compareNatural (b.getFileNameWithoutExtension()) < 0; });
    return files;
}

bool PresetManager::save (const juce::String& name, const juce::ValueTree& state, juce::String& error) const
{
    const auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty())
    {
        error = juce::String::fromUTF8 ("Zadej název presetu.");
        return false;
    }
    const auto folder = getFolder();
    if (! folder.createDirectory())
    {
        error = juce::String::fromUTF8 ("Nejde vytvořit složku ") + folder.getFullPathName();
        return false;
    }

    juce::XmlElement root ("DusiXPreset");
    root.setAttribute ("name", name.trim());
    root.setAttribute ("version", 3);
    if (auto xml = state.createXml())
        root.addChildElement (xml.release());

    const auto file = folder.getChildFile (clean + kExtension);
    if (! root.writeTo (file))
    {
        error = juce::String::fromUTF8 ("Nejde uložit soubor ") + file.getFullPathName();
        return false;
    }
    return true;
}

juce::ValueTree PresetManager::load (const juce::File& file) const
{
    if (auto xml = juce::XmlDocument::parse (file))
        if (xml->hasTagName ("DusiXPreset"))
            if (auto* child = xml->getFirstChildElement())
                return juce::ValueTree::fromXml (*child);
    return {};
}

bool PresetManager::remove (const juce::File& file) const
{
    return file.existsAsFile() && file.moveToTrash();
}

void PresetManager::revealFolder() const
{
    const auto folder = getFolder();
    folder.createDirectory();
    folder.revealToUser();
}

void PresetManager::loadFavourites() const
{
    if (favouritesLoaded) return;
    favouritesLoaded = true;
    favourites.clear();
    const auto f = favouritesFile();
    if (f.existsAsFile())
    {
        favourites.addLines (f.loadFileAsString());
        favourites.removeEmptyStrings();
    }
}

void PresetManager::storeFavourites() const
{
    const auto f = favouritesFile();
    f.getParentDirectory().createDirectory();
    f.replaceWithText (favourites.joinIntoString ("\n"));
}

bool PresetManager::isFavourite (const juce::String& key) const
{
    loadFavourites();
    return favourites.contains (key);
}

void PresetManager::setFavourite (const juce::String& key, bool fav)
{
    loadFavourites();
    if (fav) favourites.addIfNotAlreadyThere (key);
    else     favourites.removeString (key);
    storeFavourites();
}
