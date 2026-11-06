#pragma once

#include "CVerbParameters.h"

namespace cdaw::CVerbPresets
{
    /** A C-Verb sound. Factory presets list only what differs from the parameters' defaults;
        user presets (XML files) list every parameter by ID. The category is the type's name
        (or "User"). */
    struct Preset
    {
        juce::String name, category;
        std::vector<std::pair<int, float>> values;
        juce::File file;                        // set for user presets

        bool isUser() const                     { return file != juce::File(); }
    };

    /** The factory presets, the first being the defaults (Default Hall). */
    const std::vector<Preset>& factory();

    /** Where user presets live: %APPDATA%\CDAW\Presets\C-Verb (CDAW and the VST3 share it). */
    juce::File getUserFolder();
    void setUserFolder (const juce::File&);     // tests; an empty File goes back to the usual one

    std::vector<Preset> loadUser();

    /** Saves the values as a user preset (replacing one of the same name). Fails for an empty
        name or a factory preset's name. */
    juce::Result saveUser (const juce::String& name, const std::function<float (int)>& valueOf);

    /** Every parameter's value in a preset, by ID. */
    std::vector<float> valuesOf (const Preset&);

    /** The factory presets, then the user's. */
    std::vector<Preset> everything();
}
