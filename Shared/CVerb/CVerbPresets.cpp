#include "CVerbPresets.h"

namespace cdaw::CVerbPresets
{
    namespace
    {
        using namespace CVerbParameters;
        using T = ReverbEngine::Type;

        Preset make (const char* name, T t, std::vector<std::pair<int, float>> values)
        {
            if (t != T::hall)
                values.insert (values.begin(), { type, (float) t });
            return { name, ReverbEngine::getTypeNames()[(int) t], values, {} };
        }

        std::vector<Preset> makeFactory()
        {
            std::vector<Preset> p;

            // Hall ------------------------------------------------------------------------------
            p.push_back (make ("Default Hall", T::hall, {}));
            p.push_back (make ("Concert Hall", T::hall, { { size, 0.75f }, { decay, 2.8f }, { preDelay, 30.0f }, { early, 0.5f }, { character, 0.4f },
                                                         { lowDecay, 1.2f }, { postHighCut, 10000.0f } }));
            p.push_back (make ("Cathedral", T::hall, { { size, 1.0f }, { decay, 7.0f }, { preDelay, 45.0f }, { diffusion, 0.85f }, { modulation, 0.4f },
                                                      { highDecay, 0.45f }, { dampFreq, 4000.0f }, { character, 0.7f }, { early, 0.3f }, { lowDecay, 1.3f } }));
            p.push_back (make ("Ducked Vocal Hall", T::hall, { { decay, 2.6f }, { preDelay, 60.0f }, { duck, 0.6f }, { postLowCut, 200.0f },
                                                              { postHighCut, 9000.0f }, { mix, 0.4f } }));
            p.push_back (make ("Dark Hall", T::hall, { { dampFreq, 2500.0f }, { highDecay, 0.3f }, { postHighCut, 6000.0f }, { decay, 3.2f } }));
            p.push_back (make ("Endless Drone", T::hall, { { size, 1.0f }, { decay, 20.0f }, { diffusion, 0.9f }, { modulation, 0.5f }, { highDecay, 0.6f },
                                                          { variation, 0.8f }, { mix, 0.5f }, { postLowCut, 150.0f } }));

            // Room ------------------------------------------------------------------------------
            p.push_back (make ("Small Room", T::room, { { size, 0.25f }, { decay, 0.6f }, { early, 0.7f }, { character, 0.2f }, { preDelay, 5.0f } }));
            p.push_back (make ("Drum Room", T::room, { { size, 0.5f }, { decay, 1.0f }, { early, 0.8f }, { character, 0.6f }, { variation, 0.75f },
                                                      { postLowCut, 80.0f }, { preDelay, 8.0f }, { mix, 0.35f } }));
            p.push_back (make ("Wood Room", T::room, { { size, 0.4f }, { decay, 0.8f }, { variation, 0.3f }, { early, 0.6f }, { highDecay, 0.45f } }));

            // Plate -----------------------------------------------------------------------------
            p.push_back (make ("Vocal Plate", T::plate, { { decay, 1.8f }, { preDelay, 40.0f }, { character, 0.6f }, { variation, 0.6f },
                                                         { postLowCut, 180.0f }, { modulation, 0.35f } }));
            p.push_back (make ("Snare Plate", T::plate, { { decay, 1.3f }, { preDelay, 10.0f }, { character, 0.7f }, { variation, 0.8f },
                                                         { highDecay, 0.7f }, { dampFreq, 7000.0f } }));
            p.push_back (make ("Dark Plate", T::plate, { { decay, 2.5f }, { variation, 0.2f }, { highDecay, 0.35f } }));

            // Shimmer ---------------------------------------------------------------------------
            p.push_back (make ("Shimmer Pad", T::shimmer, { { size, 0.9f }, { decay, 5.0f }, { character, 0.6f }, { variation, 0.6f }, { modulation, 0.5f },
                                                           { diffusion, 0.9f }, { mix, 0.45f }, { preDelay, 40.0f }, { postLowCut, 200.0f } }));
            p.push_back (make ("Octave Down Shimmer", T::shimmer, { { pitch, -12.0f }, { character, 0.5f }, { variation, 0.35f }, { decay, 4.0f }, { size, 0.85f } }));
            p.push_back (make ("Fifth Shimmer", T::shimmer, { { pitch, 7.0f }, { character, 0.45f }, { decay, 4.5f }, { size, 0.8f } }));
            p.push_back (make ("Heaven", T::shimmer, { { character, 0.85f }, { variation, 0.8f }, { decay, 8.0f }, { size, 1.0f }, { mix, 0.5f },
                                                      { highDecay, 0.7f }, { modulation, 0.5f }, { postLowCut, 250.0f } }));

            // Reverse ---------------------------------------------------------------------------
            p.push_back (make ("Reverse Swell", T::reverse, { { character, 0.6f }, { decay, 1.2f }, { variation, 0.4f }, { mix, 0.45f } }));
            p.push_back (make ("Reverse Vocal", T::reverse, { { character, 0.7f }, { decay, 1.6f }, { variation, 0.6f }, { preDelay, 0.0f }, { postLowCut, 200.0f } }));
            p.push_back (make ("Backwards Snare", T::reverse, { { character, 0.45f }, { decay, 0.7f }, { variation, 0.2f }, { size, 0.4f } }));

            // Bloom -----------------------------------------------------------------------------
            p.push_back (make ("Bloom Pad", T::bloom, { { character, 0.55f }, { variation, 0.4f }, { decay, 4.0f }, { size, 0.8f } }));
            p.push_back (make ("Slow Bloom", T::bloom, { { character, 0.85f }, { variation, 0.6f }, { decay, 6.0f }, { size, 1.0f }, { mix, 0.5f } }));

            // Gated -----------------------------------------------------------------------------
            p.push_back (make ("80s Gated Snare", T::gated, { { character, 0.5f }, { variation, 0.35f }, { decay, 3.0f }, { size, 0.5f },
                                                             { early, 0.7f }, { mix, 0.45f }, { preDelay, 0.0f } }));
            p.push_back (make ("Tight Gate", T::gated, { { character, 0.25f }, { variation, 0.2f }, { decay, 1.5f }, { size, 0.35f } }));

            // Cloud -----------------------------------------------------------------------------
            p.push_back (make ("Granular Cloud", T::cloud, { { character, 0.5f }, { variation, 0.6f }, { decay, 4.0f }, { size, 0.8f }, { mix, 0.5f } }));
            p.push_back (make ("Shimmer Cloud", T::cloud, { { character, 0.6f }, { variation, 0.7f }, { decay, 5.0f }, { size, 0.9f }, { mix, 0.5f },
                                                           { postLowCut, 200.0f } }));
            p.push_back (make ("Dust", T::cloud, { { character, 0.15f }, { variation, 0.9f }, { decay, 3.0f }, { pitch, 0.0f }, { postHighCut, 7000.0f } }));

            return p;
        }

        juce::String userExtension()     { return ".cverb"; }

        juce::File& userFolderOverride()
        {
            static juce::File folder;
            return folder;
        }
    }

    const std::vector<Preset>& factory()
    {
        static const auto presets = makeFactory();
        return presets;
    }

    std::vector<float> valuesOf (const Preset& preset)
    {
        std::vector<float> values;
        for (int i = 0; i < count; ++i)
            values.push_back (get ((ID) i).defaultValue);

        for (auto& [id, value] : preset.values)
            if (id >= 0 && id < (int) values.size())
                values[(size_t) id] = value;

        return values;
    }

    void setUserFolder (const juce::File& folder)
    {
        userFolderOverride() = folder;
    }

    juce::File getUserFolder()
    {
        if (userFolderOverride() != juce::File())
            return userFolderOverride();

        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("CDAW").getChildFile ("Presets").getChildFile ("C-Verb");
    }

    std::vector<Preset> loadUser()
    {
        std::vector<Preset> presets;

        for (auto& file : getUserFolder().findChildFiles (juce::File::findFiles, false, "*" + userExtension()))
        {
            auto xml = juce::XmlDocument::parse (file);
            if (xml == nullptr || ! xml->hasTagName ("CVerbPreset"))
                continue;

            Preset p { xml->getStringAttribute ("name", file.getFileNameWithoutExtension()), "User", {}, file };

            for (auto* param : xml->getChildWithTagNameIterator ("Param"))
            {
                const auto id = param->getStringAttribute ("id");
                for (int i = 0; i < count; ++i)
                {
                    const auto& spec = get ((ID) i);
                    if (id == spec.id)
                        p.values.push_back ({ i, juce::jlimit (spec.minimum, spec.maximum, (float) param->getDoubleAttribute ("value")) });
                }
            }

            presets.push_back (std::move (p));
        }

        std::sort (presets.begin(), presets.end(), [] (const Preset& a, const Preset& b) { return a.name.compareNatural (b.name) < 0; });
        return presets;
    }

    juce::Result saveUser (const juce::String& name, const std::function<float (int)>& valueOf)
    {
        const auto clean = juce::File::createLegalFileName (name.trim());
        if (clean.isEmpty())
            return juce::Result::fail ("The preset needs a name");

        // One name, one preset: the browser finds presets by name.
        for (auto& p : factory())
            if (p.name.equalsIgnoreCase (name.trim()))
                return juce::Result::fail ("\"" + p.name + "\" is a factory preset: choose another name");

        auto folder = getUserFolder();
        if (auto made = folder.createDirectory(); made.failed())
            return made;

        juce::XmlElement xml ("CVerbPreset");
        xml.setAttribute ("name", name.trim());
        xml.setAttribute ("version", 1);

        for (int i = 0; i < count; ++i)
        {
            auto* param = xml.createNewChildElement ("Param");
            param->setAttribute ("id", get ((ID) i).id);
            param->setAttribute ("value", valueOf (i));
        }

        auto file = folder.getChildFile (clean + userExtension());
        return xml.writeTo (file) ? juce::Result::ok() : juce::Result::fail ("Could not write " + file.getFullPathName());
    }

    std::vector<Preset> everything()
    {
        auto all = factory();
        auto user = loadUser();
        all.insert (all.end(), user.begin(), user.end());
        return all;
    }
}
