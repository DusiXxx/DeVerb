#pragma once

#include "PluginProcessor.h"
#include "Widgets.h"

// Polozka v seznamu kategorii (nazev + pocet)
class CategoryButton : public juce::Button
{
public:
    explicit CategoryButton (const juce::String& n) : juce::Button (n) {}
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    int count = 0;
    bool separatorAbove = false;
};

// Prohlizec presetu: kategorie, hledani, oblibene (hvezdicka), vlastni presety ze souboru.
class PresetBrowser : public juce::Component, private juce::ListBoxModel
{
public:
    explicit PresetBrowser (DusiVerbProcessor&);

    std::function<void()> onClose;
    std::function<void()> onPresetLoaded;

    void refresh();
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void resized() override;

private:
    struct Item
    {
        juce::String name, category, key;
        int factoryIndex = -1;
        juce::File file;
    };

    int getNumRows() override { return (int) shown.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void selectedRowsChanged (int) override { updateButtons(); }

    void rebuild();
    void setCategory (const juce::String& c);
    void updateButtons();

    DusiVerbProcessor& proc;
    std::vector<Item> all, shown;
    juce::String category;

    juce::OwnedArray<CategoryButton> categoryButtons;
    juce::Rectangle<int> dialog;
    juce::String summary;
    juce::TextEditor search;
    juce::ListBox list { "presets", this };
    juce::TextButton openFolder, deleteButton, closeButton;
};
