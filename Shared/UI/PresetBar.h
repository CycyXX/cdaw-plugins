#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace cdaw
{
    /** A preset browser for a plugin's header: previous / next, the current preset's name (a
        menu of every preset by category, marked with * once edited) and Save. It knows nothing
        about the plugin: everything goes through the callbacks, which must all be set. */
    class PresetBar : public juce::Component
    {
    public:
        struct Item
        {
            juce::String name, category;        // category "User" for the user's own
            std::vector<float> values;          // every parameter, by ID
        };

        PresetBar();

        std::function<std::vector<Item>()> getPresets;          // factory first, then the user's
        std::function<juce::StringArray()> getCategories;       // the factory categories, in menu order
        std::function<juce::String()> getCurrentName;
        std::function<float (int)> getValue;                    // the parameter's current value
        std::function<float (int)> getTolerance;                // how far a value may be off and still match the preset
        std::function<void (const Item&)> apply;                // sets the values and remembers the name
        std::function<juce::Result (const juce::String&)> save; // saves the current values under a name and remembers it
        std::function<juce::File()> getUserFolder;

        /** Updates the name and edited mark (call from the editor's timer now and then). */
        void refresh();

        void resized() override;

    private:
        void step (int delta);
        void showMenu();
        void showSaveWindow();

        juce::TextButton previousButton { "<" }, nextButton { ">" }, nameButton, saveButton { "Save" };
        std::vector<Item> presets;
    };
}
