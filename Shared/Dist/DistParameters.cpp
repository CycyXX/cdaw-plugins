#include "DistParameters.h"

namespace cdaw::DistParameters
{
    namespace
    {
        using namespace SpecText;

        juce::String seconds (float v)          { return v < 1.0f ? juce::String (juce::roundToInt (v * 1000.0f)) + " ms" : juce::String (v, 2) + " s"; }
        float parseSeconds (const juce::String& s)
        {
            auto v = s.getFloatValue();
            return s.containsIgnoreCase ("ms") ? v * 0.001f : v;
        }

        ParameterSpecs makeSpecs()
        {
            ParameterSpecs specs ((size_t) count);
            const auto voicings = DistEngine::getVoicingNames();

            specs[voicing]       = { "voicing",     "Voicing",        0.0f, (float) (DistEngine::numVoicings - 1), 2.0f, 0.0f, 1.0f, choice (voicings), parseChoice (voicings) };
            specs[input]         = { "inputgain",   "Input",          -12.0f, 12.0f, 0.0f, 0.0f, 0.0f, db, parseFloat };
            specs[drive]         = { "drive",       "Drive",          0.0f, 1.0f, 0.5f, 0.0f, 0.0f, percent, parsePercent };
            specs[tight]         = { "tight",       "Tight",          20.0f, 800.0f, 90.0f, 140.0f, 0.0f, hz, parseHz };
            specs[character]     = { "character",   "Character",      0.0f, 1.0f, 0.3f, 0.0f, 0.0f, percent, parsePercent };
            specs[bass]          = { "bass",        "Bass",           -12.0f, 12.0f, 0.0f, 0.0f, 0.0f, db, parseFloat };
            specs[mid]           = { "mid",         "Mid",            -12.0f, 12.0f, 0.0f, 0.0f, 0.0f, db, parseFloat };
            specs[midFreq]       = { "midfreq",     "Mid Freq",       200.0f, 4000.0f, 800.0f, 900.0f, 0.0f, hz, parseHz };
            specs[treble]        = { "treble",      "Treble",         -12.0f, 12.0f, 0.0f, 0.0f, 0.0f, db, parseFloat };
            specs[smooth]        = { "smooth",      "Smooth",         2000.0f, 20000.0f, 9000.0f, 7000.0f, 0.0f,
                                     [] (float v) { return v >= 19900.0f ? juce::String ("Off") : hz (v); }, parseHz };
            specs[level]         = { "outlevel",    "Level",          -18.0f, 18.0f, 0.0f, 0.0f, 0.0f, db, parseFloat };
            specs[blend]         = { "blend",       "Blend",          0.0f, 1.0f, 1.0f, 0.0f, 0.0f, percent, parsePercent };
            specs[gateOn]        = { "gateon",      "Gate",           0.0f, 1.0f, 0.0f, 0.0f, 1.0f, onOff, parseOnOff };
            specs[gateThreshold] = { "gatethresh",  "Gate Threshold", -90.0f, -20.0f, -60.0f, 0.0f, 0.0f, db, parseFloat };
            specs[gateRelease]   = { "gaterelease", "Gate Release",   0.01f, 1.0f, 0.08f, 0.15f, 0.0f, seconds, parseSeconds };
            return specs;
        }
    }

    const ParameterSpecs& all()
    {
        static const auto specs = makeSpecs();
        return specs;
    }

    const ParameterSpec& get (int id)
    {
        return all()[(size_t) id];
    }

    DistEngine::Settings toSettings (const std::function<float (int)>& valueOf)
    {
        DistEngine::Settings s;
        s.voicing = (DistEngine::Voicing) juce::jlimit (0, DistEngine::numVoicings - 1, juce::roundToInt (valueOf (voicing)));
        s.inputDb = valueOf (input);
        s.drive = valueOf (drive);
        s.tight = valueOf (tight);
        s.character = valueOf (character);
        s.bassDb = valueOf (bass);
        s.midDb = valueOf (mid);
        s.midHz = valueOf (midFreq);
        s.trebleDb = valueOf (treble);
        s.smooth = valueOf (smooth);
        s.levelDb = valueOf (level);
        s.blend = valueOf (blend);
        s.gateOn = valueOf (gateOn) >= 0.5f;
        s.gateThresholdDb = valueOf (gateThreshold);
        s.gateRelease = valueOf (gateRelease);
        return s;
    }
}
