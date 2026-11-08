#include "PluginEditor.h"
#include "Presets.h"

using ui::U;

namespace
{
constexpr int kWidth = 960, kHeight = 760, kHeaderH = 88;
}

//==============================================================================
void TabButton::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    auto r = getLocalBounds().toFloat();
    const bool on = getToggleState();
    if (on)
    {
        g.setColour (juce::Colour (0xff1a2540));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (ui::space);
        g.fillRoundedRectangle (r.removeFromBottom (2.0f).reduced (6.0f, 0.0f), 1.0f);
    }
    g.setColour (on ? ui::text : highlighted ? ui::text2 : ui::textDim);
    g.setFont (ui::font (12.0f, 600, 0.08f));
    g.drawText (getButtonText(), getLocalBounds(), juce::Justification::centred);
}

//==============================================================================
void PresetField::set (const juce::String& name, const juce::String& category)
{
    if (name == presetName && category == presetCategory) return;
    presetName = name;
    presetCategory = category;
    setTooltip (U ("Otevřít prohlížeč presetů"));
    repaint();
}

void PresetField::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (ui::inset);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (open ? ui::space : highlighted ? ui::lineHover : ui::lineStrong);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);

    auto area = getLocalBounds().withTrimmedLeft (10).withTrimmedRight (8);
    ui::drawGlyph (g, open ? ui::Glyph::chevronDown : ui::Glyph::chevronDown,
                   area.removeFromRight (12).toFloat().withSizeKeepingCentre (12.0f, 12.0f), ui::textDim);
    area.removeFromRight (8);
    const auto cf = ui::font (10.0f, 600, 0.06f);
    const auto cat = ui::upper (presetCategory);
    const int cw = juce::GlyphArrangement::getStringWidthInt (cf, cat) + 2;
    g.setColour (ui::textDim);
    g.setFont (cf);
    g.drawText (cat, area.removeFromRight (cw), juce::Justification::centredRight);
    area.removeFromRight (8);
    g.setColour (ui::text);
    g.setFont (ui::font (13.0f, 500));
    g.drawText (presetName, area, juce::Justification::centredLeft, true);
}

//==============================================================================
void AbSegment::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (ui::panel);
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (juce::Colour (0xff1f2a40));
    g.drawRoundedRectangle (r, 7.0f, 1.0f);
    for (int i = 0; i < 2; ++i)
    {
        const auto b = juce::Rectangle<float> (3.0f + 32.0f * (float) i, 3.0f, 30.0f, (float) getHeight() - 6.0f);
        if (i == slot)
        {
            g.setColour (ui::space);
            g.fillRoundedRectangle (b, 5.0f);
        }
        g.setColour (i == slot ? juce::Colour (0xff06101e) : ui::textDim);
        g.setFont (ui::font (12.0f, i == slot ? 700 : 600));
        g.drawText (i == 0 ? "A" : "B", b, juce::Justification::centred);
    }
}

void AbSegment::mouseUp (const juce::MouseEvent& e)
{
    const int s = e.position.x < 34.0f ? 0 : 1;
    if (onSelect) onSelect (s);
    setSlot (s);
}

//==============================================================================
juce::String UpdateButton::label() const
{
    using S = Updater::State;
    const auto v = "v" + Updater::currentVersion();
    const auto nv = updater.getInfo().version;
    switch (updater.getState())
    {
        case S::idle:        return v + U (" · zkontrolovat");
        case S::checking:    return v + U (" · hledám…");
        case S::upToDate:    return v + U (" · aktuální ✓");
        case S::offline:     return v + " · offline";
        case S::available:   return "AKTUALIZOVAT " + nv;
        case S::downloading: return U ("STAHUJI ") + juce::String (juce::roundToInt (updater.getProgress() * 100.0f)) + " %";
        case S::ready:       return "INSTALOVAT " + nv;
        case S::scheduled:   return U ("ZAVŘI LOGIC → ") + nv;
        case S::installed:   return U ("RESTARTUJ LOGIC ✓");
        case S::failed:      return v + U (" · chyba, znovu");
    }
    return v;
}

int UpdateButton::preferredWidth() const
{
    using S = Updater::State;
    const auto st = updater.getState();
    const bool action = st == S::available || st == S::downloading || st == S::ready || st == S::scheduled || st == S::installed;
    const auto f = ui::font (10.0f, action ? 700 : 500, action ? 0.04f : 0.0f);
    return juce::GlyphArrangement::getStringWidthInt (f, label()) + (action ? 32 : 12);
}

bool UpdateButton::refresh()
{
    using S = Updater::State;
    const auto st = updater.getState();
    const int pc = st == S::downloading ? juce::roundToInt (updater.getProgress() * 100.0f) : -1;
    if (st == shown && pc == shownPercent) return false;
    shown = st;
    shownPercent = pc;

    juce::String tip;
    const auto info = updater.getInfo();
    switch (st)
    {
        case S::idle: case S::upToDate: case S::offline:
            tip = U ("Verze ") + Updater::currentVersion() + U (". Klik = zkontrolovat aktualizace na GitHubu."); break;
        case S::checking:    tip = U ("Hledám novou verzi na GitHubu…"); break;
        case S::available:   tip = U ("Nová verze ") + info.version + U (" – klik ji stáhne.\n") + info.notes; break;
        case S::downloading: tip = U ("Stahuji a ověřuji (SHA-256). Klik = zrušit."); break;
        case S::ready:       tip = U ("Staženo a ověřeno. Klik = nainstalovat (proběhne po zavření Logicu)."); break;
        case S::scheduled:   tip = U ("Aktualizace čeká – zavři Logic, nainstaluje se sama a ozve se oznámením."); break;
        case S::installed:   tip = U ("Verze ") + info.version + U (" je nainstalovaná. Otevři Logic znovu."); break;
        case S::failed:      tip = updater.getError() + U ("\nKlik = zkusit znovu."); break;
    }
    setTooltip (tip);
    repaint();
    return true;
}

void UpdateButton::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    using S = Updater::State;
    const auto st = updater.getState();
    const auto r = getLocalBounds().toFloat().reduced (0.5f, 3.5f);
    const auto caption = label();

    juce::Colour col = ui::textDim, border = ui::lineStrong;
    bool action = true;
    ui::Glyph glyph = ui::Glyph::download;
    switch (st)
    {
        case S::available: case S::ready:   col = ui::good; border = ui::good; glyph = ui::Glyph::download; break;
        case S::downloading:                col = ui::good; border = ui::good.withAlpha (0.6f); glyph = ui::Glyph::download; break;
        case S::scheduled:                  col = ui::mid;  border = ui::mid; glyph = ui::Glyph::refresh; break;
        case S::installed:                  col = ui::good; border = ui::good; glyph = ui::Glyph::check; break;
        case S::failed:                     col = ui::bad;  border = ui::bad.withAlpha (0.6f); action = false; break;
        default:                            action = false; if (highlighted) { col = ui::text2; border = ui::lineHover; } break;
    }

    g.setColour (action ? col.withAlpha (0.14f) : juce::Colours::transparentBlack);
    g.fillRoundedRectangle (r, 4.0f);
    if (st == S::downloading)
    {
        g.setColour (col.withAlpha (0.22f));
        g.fillRoundedRectangle (r.withWidth (r.getWidth() * updater.getProgress()), 4.0f);
    }
    g.setColour (border);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    auto tr = r.reduced (5.0f, 0.0f);
    if (action)
    {
        ui::drawGlyph (g, glyph, tr.removeFromLeft (14.0f).withSizeKeepingCentre (12.0f, 12.0f), col);
        tr.removeFromLeft (4.0f);
    }
    g.setColour (col);
    g.setFont (ui::font (10.0f, action ? 700 : 500, action ? 0.04f : 0.0f));
    g.drawText (caption, tr, juce::Justification::centredLeft, false);
}

//==============================================================================
DusiVerbEditor::DusiVerbEditor (DusiVerbProcessor& p)
    : AudioProcessorEditor (&p), proc (p), mainPage (p), effectsPage (p), browser (p)
{
    setLookAndFeel (&look);
    setOpaque (true);

    addAndMakeVisible (mainPage);
    addChildComponent (effectsPage);
    addChildComponent (browser);
    browser.onClose = [this] { showBrowser (false); };
    browser.onPresetLoaded = [this] { refreshPresetField(); };

    for (auto* t : { &tabMain, &tabEffects })
    {
        t->setClickingTogglesState (false);
        addAndMakeVisible (t);
    }
    tabMain.setToggleState (true, juce::dontSendNotification);
    tabMain.onClick    = [this] { showPage (0); };
    tabEffects.onClick = [this] { showPage (1); };

    updateButton.onClick = [this] { updateClicked(); };
    addAndMakeVisible (updateButton);
    updateButton.refresh();

    ecoButton.setClickingTogglesState (true);
    ecoButton.setToggleState (proc.eco.load(), juce::dontSendNotification);
    ecoButton.onClick = [this] { proc.eco.store (ecoButton.getToggleState()); };
    addAndMakeVisible (ecoButton);

    undoButton.onClick = [this] { proc.getUndoManager().undo(); };
    redoButton.onClick = [this] { proc.getUndoManager().redo(); };
    addAndMakeVisible (undoButton);
    addAndMakeVisible (redoButton);

    presetField.onClick = [this] { showBrowser (! browser.isVisible()); };
    prevPreset.onClick = [this] { stepPreset (-1); };
    nextPreset.onClick = [this] { stepPreset (1); };
    randomButton.onClick = [this] { proc.randomize(); refreshPresetField(); };
    saveButton.setTooltip (U ("Uložit vlastní preset do složky Hudba / DusiX DeVerb / Presety"));
    saveButton.onClick = [this] { savePreset(); };
    for (juce::Component* b : { (juce::Component*) &prevPreset, (juce::Component*) &nextPreset, (juce::Component*) &presetField,
                                (juce::Component*) &randomButton, (juce::Component*) &saveButton })
        addAndMakeVisible (b);

    abSegment.setTooltip (U ("Dvě nastavení A a B – přepínej a porovnávej"));
    abSegment.onSelect = [this] (int s) { proc.selectAb (s); };
    addAndMakeVisible (abSegment);
    abCopy.setTooltip (U ("Zkopírovat aktuální nastavení do druhého slotu"));
    abCopy.onClick = [this] { proc.copyAbToOther(); };
    addAndMakeVisible (abCopy);

    sendLookAndFeelChange();   // stranky vznikly pred nastavenim vzhledu - at si ho prevezmou
    setSize (kWidth, kHeight);
    refreshPresetField();
    timerCallback();
    startTimerHz (10);

    updater->check (false);    // automaticka kontrola nejvys jednou za 6 hodin
}

DusiVerbEditor::~DusiVerbEditor()
{
    setLookAndFeel (nullptr);
}

void DusiVerbEditor::showPage (int index)
{
    showBrowser (false);
    mainPage.setVisible (index == 0);
    effectsPage.setVisible (index == 1);
    tabMain.setToggleState (index == 0, juce::dontSendNotification);
    tabEffects.setToggleState (index == 1, juce::dontSendNotification);
}

void DusiVerbEditor::showBrowser (bool show)
{
    if (show) browser.refresh();
    browser.setVisible (show);
    if (show) browser.toFront (false);
    presetField.open = show;
    presetField.repaint();
}

void DusiVerbEditor::refreshPresetField()
{
    const auto name = proc.getPresetName();
    juce::String category = U ("Vlastní");
    if (name == U ("Náhodný"))
        category = U ("Náhoda");
    else
        for (const auto& fp : factoryPresets())
            if (name == U (fp.name)) { category = U (fp.category); break; }
    presetField.set (name, category);
    shownPresetName = name;
}

void DusiVerbEditor::stepPreset (int delta)
{
    const int n = proc.getNumPrograms();
    const int cur = proc.getPresetName() == proc.getProgramName (proc.getCurrentProgram()) ? proc.getCurrentProgram() : (delta > 0 ? -1 : 0);
    proc.setCurrentProgram ((cur + delta + n) % n);
    proc.updateHostDisplay (juce::AudioProcessorListener::ChangeDetails().withProgramChanged (true));
    refreshPresetField();
    browser.repaint();
}

void DusiVerbEditor::savePreset()
{
    auto* w = new juce::AlertWindow (U ("Uložit preset"), U ("Název presetu:"), juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", proc.getPresetName());
    w->addButton (U ("Uložit"), 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton (U ("Zrušit"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<DusiVerbEditor> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int r)
    {
        if (safe == nullptr || r == 0) return;
        juce::String error;
        if (safe->proc.saveUserPreset (w->getTextEditorContents ("name"), error))
        {
            safe->refreshPresetField();
            safe->browser.refresh();
        }
        else
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, U ("Preset se neuložil"), error);
    }), true);
}

void DusiVerbEditor::updateClicked()
{
    using S = Updater::State;
    switch (updater->getState())
    {
        case S::available:   updater->download(); break;
        case S::downloading: updater->cancel(); break;
        case S::ready:
        {
            const auto info = updater->getInfo();
            juce::Component::SafePointer<DusiVerbEditor> safe (this);
            juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::NoIcon,
                U ("Nainstalovat DeVerb ") + info.version + "?",
                (info.notes.isNotEmpty() ? U ("Novinky: ") + info.notes + "\n\n" : juce::String())
                    + U ("Instalace proběhne sama, až zavřeš Logic (projekt si ulož). "
                         "Starou verzi zazálohuju, kdyby něco selhalo, vrátí se zpět. "
                         "Pak Logic znovu otevři – hotovo."),
                U ("Nainstalovat"), U ("Později"), this,
                juce::ModalCallbackFunction::create ([safe] (int r) { if (safe != nullptr && r == 1) safe->updater->install(); }));
            break;
        }
        case S::scheduled:
        case S::installed:
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, U ("Aktualizace"),
                                                    updateButton.getTooltip(), "OK", this);
            break;
        case S::checking: break;
        default: updater->check (true); break;
    }
    updateButton.refresh();
}

void DusiVerbEditor::timerCallback()
{
    auto& um = proc.getUndoManager();
    // kazde pusteni mysi = nova polozka v historii "Zpet"
    if (! juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown())
        um.beginNewTransaction();
    undoButton.setEnabled (um.canUndo());
    redoButton.setEnabled (um.canRedo());

    if (shownPresetName != proc.getPresetName())
        refreshPresetField();
    abSegment.setSlot (proc.getAbSlot());
    const auto copyText = proc.getAbSlot() == 0 ? U ("A→B") : U ("B→A");
    if (abCopy.getButtonText() != copyText) abCopy.setButtonText (copyText);
    if (ecoButton.getToggleState() != proc.eco.load())
        ecoButton.setToggleState (proc.eco.load(), juce::dontSendNotification);

    if (updateButton.refresh())
        updateButton.setBounds (updateButton.getX(), updateButton.getY(), updateButton.preferredWidth(), updateButton.getHeight());
}

void DusiVerbEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::background);

    // logo: zdroj + dve vlny
    {
        const float x = 16.0f, y = 12.0f;
        g.setColour (ui::space);
        g.fillEllipse (x + 7.0f - 3.2f, y + 12.0f - 3.2f, 6.4f, 6.4f);
        juce::Path a1, a2;
        a1.addCentredArc (x + 6.9f, y + 12.0f, 7.5f, 7.5f, 0.0f, 0.748f, 2.394f, true);
        a2.addCentredArc (x + 8.75f, y + 12.0f, 11.5f, 11.5f, 0.0f, 0.739f, 2.403f, true);
        const auto stroke = juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        g.setColour (ui::verb);
        g.strokePath (a1, stroke);
        g.setColour (ui::verb.withAlpha (0.5f));
        g.strokePath (a2, stroke);
    }
    const auto titleFont = ui::font (18.0f, 700, -0.01f);
    g.setFont (titleFont);
    g.setColour (ui::text);
    g.drawText ("DusiX", 50, 0, 80, 48, juce::Justification::centredLeft);
    const int w1 = juce::GlyphArrangement::getStringWidthInt (titleFont, "DusiX ");
    g.setColour (juce::Colour (0xffb7aaff));
    g.setFont (ui::font (18.0f, 500, -0.01f));
    g.drawText ("DeVerb", 50 + w1, 0, 90, 48, juce::Justification::centredLeft);

    // segment zalozek
    {
        const auto seg = juce::Rectangle<float> (328.5f, 6.5f, 303.0f, 35.0f);
        g.setColour (ui::panel);
        g.fillRoundedRectangle (seg, 8.0f);
        g.setColour (juce::Colour (0xff1f2a40));
        g.drawRoundedRectangle (seg, 8.0f, 1.0f);
    }

    g.setColour (ui::textDim);
    g.setFont (ui::font (11.0f, 500, 0.08f));
    g.drawText ("PRESET", 16, 49, 54, 39, juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xff1a2336));
    g.fillRect (0, kHeaderH - 1, getWidth(), 1);
}

void DusiVerbEditor::resized()
{
    // radek 1 (48 px): logo + verze | zalozky | ECO, zpet, vpred
    updateButton.setBounds (176, 12, updateButton.preferredWidth(), 24);
    tabMain.setBounds (331, 9, 148, 30);
    tabEffects.setBounds (481, 9, 148, 30);
    redoButton.setBounds (kWidth - 16 - 32, 9, 32, 30);
    undoButton.setBounds (redoButton.getX() - 6 - 32, 9, 32, 30);
    ecoButton.setBounds (undoButton.getX() - 12 - 66, 9, 66, 30);

    // radek 2 (40 px): preset, nahodny, ulozit | A/B
    const int y2 = 54;
    prevPreset.setBounds (76, y2, 28, 28);
    presetField.setBounds (110, y2, 320, 28);
    nextPreset.setBounds (436, y2, 28, 28);
    randomButton.setBounds (474, y2, 96, 28);
    saveButton.setBounds (576, y2, 74, 28);
    abCopy.setBounds (kWidth - 16 - 48, y2, 48, 28);
    abSegment.setBounds (abCopy.getX() - 6 - 68, y2, 68, 28);

    const auto pageArea = juce::Rectangle<int> (0, kHeaderH, getWidth(), getHeight() - kHeaderH);
    mainPage.setBounds (pageArea);
    effectsPage.setBounds (pageArea);
    browser.setBounds (0, kHeaderH + 1, getWidth(), getHeight() - kHeaderH - 1);
}
