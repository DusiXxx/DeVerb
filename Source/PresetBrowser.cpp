#include "PresetBrowser.h"
#include "Presets.h"

using ui::U;

namespace
{
const juce::String kAll = U ("Vše"), kFav = U ("Oblíbené"), kMine = U ("Moje");
constexpr int kSideW = 216;
}

PresetBrowser::PresetBrowser (DusiVerbProcessor& p)
    : proc (p), openFolder (U ("Otevřít složku")), deleteButton (U ("Smazat")), closeButton (U ("Zavřít"))
{
    juce::StringArray cats { kAll, kFav, kMine };
    for (auto* c : presetCategories()) cats.add (U (c));
    for (auto& c : cats)
    {
        auto* b = categoryButtons.add (new CategoryButton (c));
        b->setClickingTogglesState (true);
        b->setRadioGroupId (201);
        b->separatorAbove = c == U (presetCategories().front());
        b->onClick = [this, c] { setCategory (c); };
        addAndMakeVisible (b);
    }

    search.setTextToShowWhenEmpty (U ("Hledat preset…"), ui::textDim);
    search.onTextChange = [this] { rebuild(); };
    search.setFont (ui::font (13.0f, 500));
    search.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    search.setIndents (30, 8);
    search.setColour (juce::TextEditor::outlineColourId, ui::lineStrong);
    search.setColour (juce::TextEditor::focusedOutlineColourId, ui::space);
    addAndMakeVisible (search);

    list.setRowHeight (36);
    list.setColour (juce::ListBox::backgroundColourId, ui::panel);
    addAndMakeVisible (list);

    openFolder.setTooltip (U ("Složka s vlastními presety – sem můžeš dát i presety od kamaráda"));
    openFolder.onClick = [this] { proc.getPresetManager().revealFolder(); };
    deleteButton.onClick = [this]
    {
        const int row = list.getSelectedRow();
        if (! juce::isPositiveAndBelow (row, (int) shown.size()) || shown[(size_t) row].factoryIndex >= 0) return;
        const auto file = shown[(size_t) row].file;
        juce::Component::SafePointer<PresetBrowser> safe (this);
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, U ("Smazat preset"),
            U ("Opravdu smazat „") + file.getFileNameWithoutExtension() + U ("“? (Půjde do koše.)"),
            U ("Smazat"), U ("Zrušit"), this,
            juce::ModalCallbackFunction::create ([safe, file] (int r)
            {
                if (safe == nullptr || r == 0) return;
                safe->proc.getPresetManager().remove (file);
                safe->refresh();
            }));
    };
    closeButton.onClick = [this] { if (onClose) onClose(); };
    deleteButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffff7a88));
    closeButton.setColour (juce::TextButton::buttonOnColourId, ui::space);
    for (auto* b : { &openFolder, &deleteButton, &closeButton }) addAndMakeVisible (b);

    categoryButtons[0]->setToggleState (true, juce::dontSendNotification);
    category = kAll;
    refresh();
}

void PresetBrowser::refresh()
{
    all.clear();
    const auto& f = factoryPresets();
    for (int i = 0; i < (int) f.size(); ++i)
        all.push_back ({ U (f[(size_t) i].name), U (f[(size_t) i].category), "f:" + U (f[(size_t) i].name), i, {} });
    for (auto& file : proc.getPresetManager().getUserPresets())
        all.push_back ({ file.getFileNameWithoutExtension(), kMine, "u:" + file.getFileName(), -1, file });
    rebuild();
}

void PresetBrowser::setCategory (const juce::String& c)
{
    category = c;
    rebuild();
}

void PresetBrowser::rebuild()
{
    shown.clear();
    const auto text = search.getText().trim();
    auto& pm = proc.getPresetManager();
    for (auto* b : categoryButtons)
    {
        const auto c = b->getButtonText();
        int n = 0;
        for (auto& it : all)
            if (c == kAll || (c == kFav ? pm.isFavourite (it.key) : it.category == c)) ++n;
        b->count = n;
        b->repaint();
    }
    for (auto& it : all)
    {
        if (category == kFav && ! pm.isFavourite (it.key)) continue;
        if (category != kAll && category != kFav && it.category != category) continue;
        if (text.isNotEmpty() && ! it.name.containsIgnoreCase (text) && ! it.category.containsIgnoreCase (text)) continue;
        shown.push_back (it);
    }
    list.updateContent();
    list.repaint();
    updateButtons();
    summary = juce::String ((int) shown.size()) + U (" presetů") + (category == kAll ? juce::String() : U (" ve skupině ") + category);
    repaint (dialog.withHeight (56));
}

void PresetBrowser::updateButtons()
{
    const int row = list.getSelectedRow();
    deleteButton.setEnabled (juce::isPositiveAndBelow (row, (int) shown.size()) && shown[(size_t) row].factoryIndex < 0);
}

void PresetBrowser::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (! juce::isPositiveAndBelow (row, (int) shown.size())) return;
    const auto& it = shown[(size_t) row];
    const bool current = it.name == proc.getPresetName();
    const auto r = juce::Rectangle<float> (0.0f, 1.0f, (float) width, (float) height - 2.0f);

    if (selected || current)
    {
        g.setColour (ui::space.withAlpha (current ? 0.14f : 0.07f));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (ui::space.withAlpha (current ? 0.7f : 0.3f));
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    }

    // hvezdicka
    const bool fav = proc.getPresetManager().isFavourite (it.key);
    const auto starBox = juce::Rectangle<float> (10.0f, (float) height * 0.5f - 9.0f, 18.0f, 18.0f);
    juce::Path star;
    star.addStar (starBox.getCentre(), 5, 3.2f, 7.5f);
    g.setColour (fav ? ui::chorus : ui::disabled);
    if (fav) g.fillPath (star); else g.strokePath (star, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const int catX = width - 10 - 80 - 12 - 130;
    g.setColour (current ? ui::text : ui::text2);
    g.setFont (ui::font (13.0f, current ? 600 : 500));
    g.drawText (it.name, 40, 0, catX - 52, height, juce::Justification::centredLeft, true);
    g.setColour (ui::textDim);
    g.setFont (ui::font (12.0f, 500));
    g.drawText (it.category, catX, 0, 130, height, juce::Justification::centredLeft);
    g.setColour (it.factoryIndex >= 0 ? ui::textDim : ui::space);
    g.drawText (it.factoryIndex >= 0 ? "DusiX" : U ("ty"), catX + 142, 0, 80, height, juce::Justification::centredLeft);
}

void PresetBrowser::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    if (! juce::isPositiveAndBelow (row, (int) shown.size())) return;
    const auto it = shown[(size_t) row];
    if (e.x < 34)   // klik na hvezdicku
    {
        auto& pm = proc.getPresetManager();
        pm.setFavourite (it.key, ! pm.isFavourite (it.key));
        if (category == kFav) rebuild(); else list.repaintRow (row);
        return;
    }
    if (it.factoryIndex >= 0)
    {
        proc.setCurrentProgram (it.factoryIndex);
        proc.updateHostDisplay (juce::AudioProcessorListener::ChangeDetails().withProgramChanged (true));
    }
    else
        proc.loadUserPreset (it.file);
    list.repaint();
    if (onPresetLoaded) onPresetLoaded();
}

void CategoryButton::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    auto r = getLocalBounds().toFloat();
    if (separatorAbove)
    {
        g.setColour (ui::line);
        g.fillRect (r.removeFromTop (11.0f).withSizeKeepingCentre (r.getWidth() - 16.0f, 1.0f));
    }
    const bool on = getToggleState();
    if (on || highlighted)
    {
        g.setColour (on ? juce::Colour (0xff1a2540) : ui::raised);
        g.fillRoundedRectangle (r, 6.0f);
    }
    auto t = r.reduced (10.0f, 0.0f);
    g.setColour (ui::textDim);
    g.setFont (ui::font (11.0f, 500));
    g.drawText (juce::String (count), t.removeFromRight (30.0f), juce::Justification::centredRight);
    g.setColour (on ? ui::text : ui::text2);
    g.setFont (ui::font (13.0f, on ? 600 : 500));
    g.drawText (getButtonText(), t, juce::Justification::centredLeft);
}

void PresetBrowser::mouseUp (const juce::MouseEvent& e)
{
    if (! dialog.contains (e.getPosition()) && onClose) onClose();   // klik mimo okno = zavrit
}

void PresetBrowser::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xc704070c));
    const auto d = dialog.toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (d.translated (0.0f, 6.0f).expanded (2.0f), 12.0f);
    g.setColour (ui::panel);
    g.fillRoundedRectangle (d, 10.0f);
    g.setColour (ui::lineStrong);
    g.drawRoundedRectangle (d.reduced (0.5f), 10.0f, 1.0f);

    g.setColour (ui::line);
    g.fillRect (dialog.getX() + 1, dialog.getY() + 56, dialog.getWidth() - 2, 1);
    g.fillRect (dialog.getX() + 1, dialog.getBottom() - 57, dialog.getWidth() - 2, 1);
    g.fillRect (dialog.getX() + kSideW, dialog.getY() + 57, 1, dialog.getHeight() - 114);

    g.setColour (ui::text);
    g.setFont (ui::font (12.0f, 600, 0.08f));
    g.drawText ("PRESETY", dialog.getX() + 16, dialog.getY(), 184, 56, juce::Justification::centredLeft);
    g.setColour (ui::textDim);
    g.setFont (ui::font (11.0f, 500));
    g.drawText (summary, dialog.getX() + 600, dialog.getY(), dialog.getWidth() - 616, 56, juce::Justification::centredRight);
    g.setColour (ui::inset);
    g.fillRoundedRectangle (search.getBounds().toFloat(), 6.0f);
    ui::drawGlyph (g, ui::Glyph::search, search.getBounds().toFloat().removeFromLeft (30.0f).withSizeKeepingCentre (14.0f, 14.0f), ui::textDim);

    // hlavicka seznamu
    const auto lb = list.getBounds();
    g.setFont (ui::font (10.0f, 600, 0.08f));
    const int catX = lb.getX() + lb.getWidth() - 10 - 80 - 12 - 130;
    g.drawText (U ("NÁZEV"), lb.getX() + 40, lb.getY() - 28, 200, 28, juce::Justification::centredLeft);
    g.drawText ("KATEGORIE", catX, lb.getY() - 28, 130, 28, juce::Justification::centredLeft);
    g.drawText ("AUTOR", catX + 142, lb.getY() - 28, 80, 28, juce::Justification::centredLeft);

    // paticka
    g.setFont (ui::font (11.0f, 500));
    g.drawText (U ("tovární presety nejdou smazat"), deleteButton.getRight() + 12, dialog.getBottom() - 56, 200, 56,
                juce::Justification::centredLeft);
    g.drawText (U ("klik = načíst · hvězdička = oblíbené"), closeButton.getX() - 260, dialog.getBottom() - 56, 248, 56,
                juce::Justification::centredRight);
}

void PresetBrowser::resized()
{
    dialog = { 24, 11, getWidth() - 48, getHeight() - 23 };
    search.setBounds (dialog.getX() + 16 + 184 + 16, dialog.getY() + 12, 380, 32);

    auto side = juce::Rectangle<int> (dialog.getX() + 8, dialog.getY() + 57 + 10, kSideW - 16, dialog.getHeight() - 114 - 20);
    for (auto* b : categoryButtons)
    {
        if (b->separatorAbove) b->setBounds (side.removeFromTop (41));
        else                   b->setBounds (side.removeFromTop (30));
        side.removeFromTop (2);
    }

    list.setBounds (dialog.getX() + kSideW + 12, dialog.getY() + 57 + 8 + 28, dialog.getWidth() - kSideW - 24, dialog.getHeight() - 114 - 8 - 28 - 4);

    const int fy = dialog.getBottom() - 56 + 12;
    openFolder.setBounds (dialog.getX() + 16, fy, 132, 32);
    deleteButton.setBounds (openFolder.getRight() + 8, fy, 84, 32);
    closeButton.setBounds (dialog.getRight() - 16 - 96, fy, 96, 32);
}
