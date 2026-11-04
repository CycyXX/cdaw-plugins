#include "DistPresets.h"

namespace cdaw::DistPresets
{
    namespace
    {
        using namespace DistParameters;
        using V = DistEngine::Voicing;

        constexpr float on = 1.0f;
        float voiced (V v)      { return (float) v; }

        Preset make (const char* name, V v, std::vector<std::pair<int, float>> values)
        {
            values.insert (values.begin(), { voicing, voiced (v) });
            return { name, DistEngine::profile (v).name, values, {} };
        }

        std::vector<Preset> makeFactory()
        {
            std::vector<Preset> p;

            // Lead -----------------------------------------------------------------------------
            p.push_back (make ("Singing Lead", V::lead, {}));
            p.push_back (make ("Liquid Solo", V::lead, { { drive, 0.8f }, { mid, 3.0f }, { midFreq, 1000.0f }, { smooth, 6500.0f },
                                                          { gateOn, on }, { gateThreshold, -65.0f } }));
            p.push_back (make ("Screamer", V::lead, { { drive, 0.35f }, { mid, 4.0f }, { midFreq, 720.0f }, { tight, 140.0f }, { character, 0.15f } }));

            // Boost ----------------------------------------------------------------------------
            p.push_back (make ("Glass Boost", V::boost, { { drive, 0.3f }, { treble, 2.0f }, { character, 0.2f }, { smooth, 16000.0f } }));
            p.push_back (make ("Edge of Breakup", V::boost, { { drive, 0.75f }, { mid, 2.0f }, { character, 0.5f } }));
            p.push_back (make ("Clean Lift", V::boost, { { drive, 0.1f }, { character, 0.1f }, { smooth, 20000.0f }, { tight, 20.0f } }));

            // Crunch ---------------------------------------------------------------------------
            p.push_back (make ("British Crunch", V::crunch, { { drive, 0.5f }, { mid, 3.0f }, { midFreq, 900.0f }, { character, 0.4f } }));
            p.push_back (make ("Blues Edge", V::crunch, { { drive, 0.3f }, { bass, -2.0f }, { treble, 1.0f }, { smooth, 7000.0f } }));
            p.push_back (make ("Rhythm Crunch", V::crunch, { { drive, 0.7f }, { tight, 120.0f }, { character, 0.5f }, { treble, 2.0f } }));

            // Modern ---------------------------------------------------------------------------
            p.push_back (make ("Modern Chug", V::modern, { { drive, 0.7f }, { tight, 140.0f }, { bass, 3.0f }, { mid, -3.0f }, { midFreq, 700.0f },
                                                            { treble, 2.0f }, { gateOn, on }, { gateThreshold, -55.0f } }));
            p.push_back (make ("Djent Tight", V::modern, { { drive, 0.85f }, { tight, 250.0f }, { mid, 3.0f }, { midFreq, 1400.0f }, { gateOn, on },
                                                            { gateThreshold, -50.0f }, { gateRelease, 0.04f } }));
            p.push_back (make ("Wall of Sound", V::modern, { { drive, 1.0f }, { tight, 110.0f }, { bass, 2.0f }, { mid, -5.0f }, { midFreq, 500.0f },
                                                              { smooth, 7000.0f }, { gateOn, on } }));
            p.push_back (make ("Thrash", V::modern, { { drive, 0.8f }, { tight, 180.0f }, { mid, -6.0f }, { midFreq, 600.0f }, { treble, 4.0f },
                                                       { gateOn, on }, { gateThreshold, -55.0f } }));

            // Fuzz -----------------------------------------------------------------------------
            p.push_back (make ("Wall of Fuzz", V::fuzz, { { drive, 0.9f }, { character, 0.2f }, { mid, -4.0f }, { smooth, 6000.0f } }));
            p.push_back (make ("Sputter", V::fuzz, { { drive, 0.8f }, { character, 0.9f } }));
            p.push_back (make ("Velvet Fuzz", V::fuzz, { { drive, 0.5f }, { character, 0.1f }, { treble, -3.0f }, { smooth, 4500.0f } }));
            p.push_back (make ("Doom", V::fuzz, { { drive, 1.0f }, { tight, 40.0f }, { bass, 5.0f }, { mid, -2.0f }, { smooth, 5000.0f } }));

            // Bass -----------------------------------------------------------------------------
            p.push_back (make ("Bass Grind", V::bass, { { drive, 0.6f }, { tight, 200.0f }, { blend, 0.8f }, { mid, 3.0f }, { midFreq, 1000.0f } }));
            p.push_back (make ("Bass Growl", V::bass, { { drive, 0.8f }, { tight, 150.0f }, { treble, 3.0f } }));
            p.push_back (make ("Subtle Bass Drive", V::bass, { { drive, 0.3f }, { tight, 300.0f }, { blend, 0.6f } }));

            // Each preset's level, so they play at about the same loudness (set from measurements).
            static const std::map<juce::String, float> levels {
                { "Liquid Solo", -0.2f }, { "Screamer", 0.3f }, { "Glass Boost", 1.3f }, { "Edge of Breakup", 1.2f }, { "Clean Lift", -1.2f },
                { "British Crunch", 0.3f }, { "Blues Edge", 1.9f }, { "Modern Chug", -0.4f }, { "Djent Tight", 2.3f }, { "Wall of Sound", -0.3f },
                { "Thrash", 2.6f }, { "Wall of Fuzz", -6.5f }, { "Sputter", 2.5f }, { "Velvet Fuzz", -6.0f }, { "Doom", -2.5f },
                { "Bass Growl", -1.8f }, { "Subtle Bass Drive", 0.4f } };

            for (auto& preset : p)
                if (auto found = levels.find (preset.name); found != levels.end())
                    preset.values.push_back ({ level, found->second });

            return p;
        }

        juce::String userExtension()     { return ".cdist"; }

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
        for (auto& spec : DistParameters::all())
            values.push_back (spec.defaultValue);

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
                   .getChildFile ("CDAW").getChildFile ("Presets").getChildFile ("C-Dist");
    }

    std::vector<Preset> loadUser()
    {
        std::vector<Preset> presets;
        const auto& specs = DistParameters::all();

        for (auto& file : getUserFolder().findChildFiles (juce::File::findFiles, false, "*" + userExtension()))
        {
            auto xml = juce::XmlDocument::parse (file);
            if (xml == nullptr || ! xml->hasTagName ("CDistPreset"))
                continue;

            Preset p { xml->getStringAttribute ("name", file.getFileNameWithoutExtension()), "User", {}, file };

            for (auto* param : xml->getChildWithTagNameIterator ("Param"))
            {
                const auto id = param->getStringAttribute ("id");
                for (int i = 0; i < (int) specs.size(); ++i)
                    if (id == specs[(size_t) i].id)
                        p.values.push_back ({ i, juce::jlimit (specs[(size_t) i].minimum, specs[(size_t) i].maximum, (float) param->getDoubleAttribute ("value")) });
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

        auto folder = getUserFolder();
        if (auto made = folder.createDirectory(); made.failed())
            return made;

        juce::XmlElement xml ("CDistPreset");
        xml.setAttribute ("name", name.trim());
        xml.setAttribute ("version", 1);

        const auto& specs = DistParameters::all();
        for (int i = 0; i < (int) specs.size(); ++i)
        {
            auto* param = xml.createNewChildElement ("Param");
            param->setAttribute ("id", specs[(size_t) i].id);
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
