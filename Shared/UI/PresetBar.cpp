#include "PresetBar.h"

namespace cdaw
{
    PresetBar::PresetBar()
    {
        for (auto* b : { &previousButton, &nextButton, &nameButton, &saveButton })
            addAndMakeVisible (b);

        previousButton.setTooltip ("Previous preset");
        nextButton.setTooltip ("Next preset");
        nameButton.setTooltip ("Choose a preset");
        saveButton.setTooltip ("Save these settings as a preset of your own");

        previousButton.onClick = [this] { step (-1); };
        nextButton.onClick = [this] { step (1); };
        nameButton.onClick = [this] { showMenu(); };
        saveButton.onClick = [this] { showSaveWindow(); };
    }

    void PresetBar::refresh()
    {
        if (presets.empty())
            presets = getPresets();

        const auto name = getCurrentName();

        for (auto& p : presets)
            if (p.name == name)
            {
                bool edited = false;
                for (int i = 0; i < (int) p.values.size() && ! edited; ++i)
                    edited = std::abs (getValue (i) - p.values[(size_t) i]) > getTolerance (i);

                const auto text = p.category + " / " + name + (edited ? " *" : "");
                if (nameButton.getButtonText() != text)
                    nameButton.setButtonText (text);
                return;
            }

        if (nameButton.getButtonText() != name)
            nameButton.setButtonText (name);
    }

    void PresetBar::step (int delta)
    {
        presets = getPresets();
        if (presets.empty())
            return;

        const auto name = getCurrentName();
        int current = 0;
        for (int i = 0; i < (int) presets.size(); ++i)
            if (presets[(size_t) i].name == name)
                current = i;

        const int n = (int) presets.size();
        apply (presets[(size_t) (((current + delta) % n + n) % n)]);
        refresh();
    }

    void PresetBar::showMenu()
    {
        presets = getPresets();
        const auto current = getCurrentName();
        juce::PopupMenu menu;

        juce::Component::SafePointer<PresetBar> safe (this);
        auto choose = [safe] (const Item& p)
        {
            if (safe != nullptr)
            {
                safe->apply (p);
                safe->refresh();
            }
        };

        for (auto& category : getCategories())
        {
            juce::PopupMenu sub;
            for (auto& p : presets)
                if (p.category == category)
                    sub.addItem (p.name, true, p.name == current, [choose, p] { choose (p); });
            if (sub.getNumItems() > 0)
                menu.addSubMenu (category, sub);
        }

        juce::PopupMenu user;
        for (auto& p : presets)
            if (p.category == "User")
                user.addItem (p.name, true, p.name == current, [choose, p] { choose (p); });
        if (user.getNumItems() == 0)
            user.addItem ("(none yet: use Save)", false, false, [] {});
        menu.addSubMenu ("User", user);

        menu.addSeparator();
        menu.addItem ("Save as preset...", [safe] { if (safe != nullptr) safe->showSaveWindow(); });
        menu.addItem ("Show presets folder", [folder = getUserFolder()] { folder.createDirectory(); folder.revealToUser(); });
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&nameButton));
    }

    void PresetBar::showSaveWindow()
    {
        auto* window = new juce::AlertWindow ("Save preset", "Name of the preset (saved in " + getUserFolder().getFullPathName() + "):",
                                              juce::MessageBoxIconType::NoIcon, this);
        window->addTextEditor ("name", getCurrentName());
        window->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

        juce::Component::SafePointer<PresetBar> safe (this);
        window->enterModalState (true, juce::ModalCallbackFunction::create ([safe, window] (int result)
        {
            if (result != 1 || safe == nullptr)
                return;

            const auto saved = safe->save (window->getTextEditorContents ("name").trim());
            if (saved.failed())
            {
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Preset not saved", saved.getErrorMessage());
                return;
            }

            safe->presets = safe->getPresets();
            safe->refresh();
        }), true);
    }

    void PresetBar::resized()
    {
        auto r = getLocalBounds();
        previousButton.setBounds (r.removeFromLeft (24));
        r.removeFromLeft (2);
        nextButton.setBounds (r.removeFromLeft (24));
        r.removeFromLeft (4);
        saveButton.setBounds (r.removeFromRight (48));
        r.removeFromRight (4);
        nameButton.setBounds (r);
    }
}
