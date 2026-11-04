#include "TestSupport.h"
#include "Shared/Chorus/ChorusParameters.h"
#include "Shared/CVerb/CVerbParameters.h"
#include "Shared/Compressor/CompressorParameters.h"
#include "Shared/Delay/DelayParameters.h"
#include "Shared/Dist/DistParameters.h"
#include "Shared/Esser/EsserParameters.h"
#include "Shared/Filter/FilterParameters.h"
#include "Shared/Lead/LeadParameters.h"
#include "Shared/Multiband/MultibandParameters.h"
#include "Shared/Phaser/PhaserParameters.h"
#include "Shared/Synth/SynthParameters.h"

namespace cdaw::test
{
    /** What saved projects depend on, pinned so a change can't break them unnoticed: each plugin's
        name and codes (a host finds the plugin by them) and its parameter IDs (saved settings are
        stored by them). New parameters may be added at the end of a list; nothing else may change.

        To add a plugin or parameters, run the tests with CDAW_WRITE_PINNED_IDS=1 (it rewrites
        tests/pinned-parameter-ids.txt) and check the diff only adds lines. */
    struct IdentityTests : public juce::UnitTest
    {
        IdentityTests() : juce::UnitTest ("Plugin identity", "Plugins") {}

        static std::map<juce::String, juce::StringArray> currentIds()
        {
            std::map<juce::String, juce::StringArray> ids;
            auto fromSpecs = [&ids] (const char* plugin, const ParameterSpecs& specs)
            {
                for (auto& spec : specs)
                    ids[plugin].add (spec.id);
            };

            fromSpecs ("C-Delay", DelayParameters::all());
            fromSpecs ("C-Chorus", ChorusParameters::all());
            fromSpecs ("C-Phaser", PhaserParameters::all());
            fromSpecs ("C-Filter", FilterParameters::all());
            fromSpecs ("C-Dist", DistParameters::all());
            fromSpecs ("C-Esser", EsserParameters::all());
            fromSpecs ("C-Synth", SynthParameters::all());
            fromSpecs ("C-Lead", LeadParameters::all());

            for (int i = 0; i < CVerbParameters::count; ++i)
                ids["C-Verb"].add (CVerbParameters::get ((CVerbParameters::ID) i).id);
            for (int i = 0; i < CompressorParameters::count; ++i)
                ids["C-Comp"].add (CompressorParameters::get ((CompressorParameters::ID) i).id);
            for (int i = 0; i < MultibandParameters::count; ++i)
                ids["C-Multicomp"].add (MultibandParameters::get (i).id);

            return ids;
        }

        void runTest() override
        {
            const juce::File root (CDAW_PLUGINS_SOURCE_DIR);

            beginTest ("Plugin names and codes haven't changed");
            {
                struct Identity { const char* folder; const char* product; const char* code; bool synth; };
                const Identity plugins[] { { "CVerb", "C-Verb", "Cvrb", false }, { "CMulticomp", "C-Multicomp", "Cmcp", false }, { "CComp", "C-Comp", "Ccmp", false },
                                           { "CDelay", "C-Delay", "Cdly", false }, { "CChorus", "C-Chorus", "Cchr", false }, { "CPhaser", "C-Phaser", "Cphs", false },
                                           { "CFilter", "C-Filter", "Cflt", false }, { "CDist", "C-Dist", "Cdst", false }, { "CEsser", "C-Esser", "Cess", false },
                                           { "CSynth", "C-Synth", "Csyn", true }, { "CLead", "C-Lead", "Clea", true } };

                const auto shared = root.getChildFile ("VST3/Common/CDAWEffect.cmake").loadFileAsString();
                for (auto& p : plugins)
                {
                    const auto cmake = root.getChildFile ("VST3").getChildFile (p.folder).getChildFile ("CMakeLists.txt").loadFileAsString();
                    expect (cmake.contains (juce::String ("PRODUCT_NAME \"") + p.product + "\""), juce::String (p.folder) + " is still named " + p.product);
                    expect (cmake.contains (juce::String ("PLUGIN_CODE ") + p.code), juce::String (p.product) + " still has the code " + p.code);

                    // The manufacturer and company are in the shared function, or in the plugin's own target.
                    const auto& where = cmake.contains ("PLUGIN_MANUFACTURER_CODE") ? cmake : shared;
                    expect (where.contains ("PLUGIN_MANUFACTURER_CODE Cdaw"), juce::String (p.product) + " keeps the manufacturer code");
                    expect (where.contains ("COMPANY_NAME \"Cyrill Durrer\""), juce::String (p.product) + " keeps the company name");
                }
            }

            const auto current = currentIds();
            auto pinnedFile = root.getChildFile ("tests/pinned-parameter-ids.txt");

            if (juce::SystemStats::getEnvironmentVariable ("CDAW_WRITE_PINNED_IDS", {}) == "1")
            {
                juce::StringArray lines;
                for (auto& [plugin, ids] : current)
                    for (auto& id : ids)
                        lines.add (plugin + " " + id);
                pinnedFile.replaceWithText (lines.joinIntoString ("\n") + "\n");
            }

            beginTest ("Parameter IDs haven't changed (new ones only at the end)");
            {
                std::map<juce::String, juce::StringArray> pinned;
                juce::StringArray lines;
                lines.addLines (pinnedFile.loadFileAsString());
                for (auto& line : lines)
                    if (line.isNotEmpty())
                        pinned[line.upToFirstOccurrenceOf (" ", false, false)].add (line.fromFirstOccurrenceOf (" ", false, false));

                expect (! pinned.empty(), "the pinned list is there");

                for (auto& [plugin, ids] : pinned)
                {
                    auto found = current.find (plugin);
                    expect (found != current.end(), plugin + " still has its parameters");
                    if (found == current.end())
                        continue;

                    expectGreaterOrEqual (found->second.size(), ids.size(), plugin + " has lost parameters");
                    for (int i = 0; i < juce::jmin (ids.size(), found->second.size()); ++i)
                        expectEquals (found->second[i], ids[i], plugin + " parameter " + juce::String (i));
                }

                for (auto& [plugin, ids] : current)
                    expect (pinned.count (plugin) > 0, plugin + " is in tests/pinned-parameter-ids.txt");
            }

            beginTest ("Parameter IDs are unique and leave Tracktion's own plugin properties alone");
            {
                // CDAW stores each parameter as a property of the plugin's state, next to these.
                const juce::StringArray reserved { "type", "id", "uid", "enabled", "process", "frozen", "quickParamName", "customName",
                                                   "masterPluginID", "sidechainSourceID", "windowX", "windowY", "windowLocked",
                                                   "manufacturer", "name", "state", "filename", "programNum", "parameters" };

                for (auto* specs : { &DelayParameters::all(), &ChorusParameters::all(), &PhaserParameters::all(), &FilterParameters::all(),
                                     &SynthParameters::all(), &LeadParameters::all(), &DistParameters::all(), &EsserParameters::all() })
                {
                    juce::StringArray ids, names;
                    for (auto& spec : *specs)
                    {
                        expect (! reserved.contains (spec.id), juce::String (spec.id) + " is one of Tracktion's plugin properties");
                        expect (! ids.contains (spec.id), juce::String (spec.id) + " is used twice");
                        expect (! names.contains (spec.name), juce::String (spec.name) + " is a name used twice (hosts list parameters by name)");
                        names.add (spec.name);
                        expect (spec.defaultValue >= spec.minimum && spec.defaultValue <= spec.maximum, juce::String (spec.id) + "'s default is in range");
                        expectWithinAbsoluteError (spec.fromText (spec.toText (spec.defaultValue)), spec.defaultValue, (spec.maximum - spec.minimum) * 0.01f,
                                                   juce::String (spec.id) + "'s text (" + spec.toText (spec.defaultValue) + ") reads back");
                        ids.add (spec.id);
                    }
                }
            }
        }
    };

    static IdentityTests identityTests;
}
