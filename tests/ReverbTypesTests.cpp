#include "TestSupport.h"
#include "Shared/CVerb/CVerbPresets.h"
#include "Shared/DSP/ReverbEngine.h"

namespace cdaw::test
{
    /** C-Verb's types, ducking, presets and the display maths, straight through the engine. */
    struct ReverbTypesTests : public juce::UnitTest
    {
        ReverbTypesTests() : juce::UnitTest ("C-Verb types", "Plugins") {}

        using Buffer = juce::AudioBuffer<float>;
        using Settings = ReverbEngine::Settings;
        using Type = ReverbEngine::Type;
        static constexpr double rate = 48000.0;

        static Settings wet (Type t)
        {
            Settings s;
            s.type = t;
            s.mix = 1.0f;
            s.preDelay = 0.0f;
            s.postLowCut = 20.0f;
            s.postHighCut = 20000.0f;
            return s;
        }

        static Buffer render (const Buffer& input, const Settings& s, int block = 512, double sampleRate = rate)
        {
            ReverbEngine engine;
            engine.prepare (sampleRate, 512, s);
            engine.jumpTo (s);
            Buffer out (input);

            for (int pos = 0; pos < out.getNumSamples(); pos += block)
                engine.process (out, pos, juce::jmin (block, out.getNumSamples() - pos), s);

            return out;
        }

        /** White noise for `length` seconds, then silence up to `total`. */
        static Buffer noiseBurst (double length, double total, double sampleRate = rate)
        {
            Buffer b (2, (int) (total * sampleRate));
            b.clear();
            juce::Random random (1234);
            for (int i = 0; i < (int) (length * sampleRate); ++i)
                for (int ch = 0; ch < 2; ++ch)
                    b.setSample (ch, i, 0.25f * (random.nextFloat() * 2.0f - 1.0f));
            return b;
        }

        static Buffer sineBurst (double hz, double length, double total)
        {
            Buffer b (2, (int) (total * rate));
            b.clear();
            for (int i = 0; i < (int) (length * rate); ++i)
                for (int ch = 0; ch < 2; ++ch)
                    b.setSample (ch, i, 0.25f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * hz * i / rate));
            return b;
        }

        static Buffer click (double total)
        {
            Buffer b (2, (int) (total * rate));
            b.clear();
            b.setSample (0, 10, 1.0f);
            b.setSample (1, 10, 1.0f);
            return b;
        }

        static float rmsDb (const Buffer& b, double from, double to, double sampleRate = rate)
        {
            const auto s = (int) (from * sampleRate), e = juce::jmin (b.getNumSamples(), (int) (to * sampleRate));
            const float l = b.getRMSLevel (0, s, e - s), r = b.getRMSLevel (1, s, e - s);
            return juce::Decibels::gainToDecibels (std::sqrt (0.5f * (l * l + r * r)), -150.0f);
        }

        /** The level (dB) of the frequencies within 30 Hz of `hz` (left channel, 1 Hz steps). */
        static double bandDb (const Buffer& b, double hz, double from, double to)
        {
            double power = 0.0;
            for (double f = hz - 30.0; f <= hz + 30.0; f += 1.0)
                power += std::pow (10.0, binDb (b, f, from, to) / 10.0);
            return 10.0 * std::log10 (power);
        }

        /** The level (dB) of one frequency in the left channel over a stretch (Goertzel). */
        static double binDb (const Buffer& b, double hz, double from, double to)
        {
            const int s = (int) (from * rate), e = juce::jmin (b.getNumSamples(), (int) (to * rate));
            const double w = 2.0 * juce::MathConstants<double>::pi * hz / rate, coeff = 2.0 * std::cos (w);
            double q1 = 0.0, q2 = 0.0;
            for (int i = s; i < e; ++i)
            {
                const double q0 = coeff * q1 - q2 + b.getSample (0, i);
                q2 = q1;
                q1 = q0;
            }
            const double power = q1 * q1 + q2 * q2 - coeff * q1 * q2;
            return 10.0 * std::log10 (juce::jmax (1.0e-20, power)) - 20.0 * std::log10 ((double) (e - s));
        }

        /** When the loudest 50 ms comes (s). */
        static double loudestAt (const Buffer& b, double until)
        {
            double best = -1000.0, at = 0.0;
            for (double t = 0.0; t + 0.05 <= until; t += 0.01)
                if (auto db = rmsDb (b, t, t + 0.05); db > best)
                {
                    best = db;
                    at = t;
                }
            return at;
        }

        static bool healthy (const Buffer& b, float& peak)
        {
            peak = 0.0f;
            for (int ch = 0; ch < b.getNumChannels(); ++ch)
                for (int i = 0; i < b.getNumSamples(); ++i)
                {
                    const float x = b.getSample (ch, i);
                    if (! std::isfinite (x))
                        return false;
                    peak = juce::jmax (peak, std::abs (x));
                }
            return true;
        }

        void runTest() override
        {
            const auto names = ReverbEngine::getTypeNames();

            beginTest ("Every type rings, stays finite and bounded, at 44.1 and 48 kHz");
            {
                for (double sampleRate : { 44100.0, 48000.0 })
                    for (int t = 0; t < ReverbEngine::numTypes; ++t)
                    {
                        const auto s = wet ((Type) t);
                        const auto out = render (noiseBurst (0.2, 3.0, sampleRate), s, 512, sampleRate);
                        float peak = 0.0f;
                        expect (healthy (out, peak), names[t] + " gives finite samples");
                        expectLessThan (peak, 4.0f, names[t] + " peak");

                        const auto tail = rmsDb (out, 0.2, 0.6, sampleRate);
                        if (sampleRate == rate)
                            logMessage (names[t] + ": " + juce::String (rmsDb (out, 0.0, 0.2), 1) + " dB during a -15 dB noise burst, "
                                        + juce::String (tail, 1) + " dB just after");
                        expectGreaterThan (tail, -50.0f, names[t] + " rings after the input stops");
                    }

                // Steady noise in: every type at about the same loudness as the Hall.
                const auto steady = [this] (Type t) { return rmsDb (render (noiseBurst (3.0, 3.0), wet (t)), 2.0, 3.0); };
                const auto hall = steady (Type::hall);
                juce::String levels;
                for (int t = 0; t < ReverbEngine::numTypes; ++t)
                {
                    const auto level = steady ((Type) t);
                    levels << names[t] << " " << juce::String (level - hall, 1) << "  ";
                    expectWithinAbsoluteError (level, hall, 6.0f, names[t] + " plays about as loud as the Hall");
                }
                logMessage ("Steady level against the Hall (" + juce::String (hall, 1) + " dB): " + levels);
            }

            beginTest ("Shimmer: the tail climbs an octave");
            {
                auto shimmer = wet (Type::shimmer);
                shimmer.character = 0.7f;
                shimmer.decay = 3.0f;
                auto hall = wet (Type::hall);
                hall.decay = 3.0f;

                auto octaveOverRoot = [this] (const Settings& s)
                {
                    const auto out = render (sineBurst (440.0, 0.5, 2.5), s);
                    return bandDb (out, 880.0, 1.0, 2.0) - bandDb (out, 440.0, 1.0, 2.0);
                };

                const auto withShimmer = octaveOverRoot (shimmer), without = octaveOverRoot (hall);
                logMessage ("880 Hz against 440 Hz in the tail: " + juce::String (withShimmer, 1) + " dB with Shimmer, "
                            + juce::String (without, 1) + " dB in the Hall");
                expectGreaterThan (withShimmer, without + 20.0);
            }

            beginTest ("Shimmer and Cloud feed back without running away");
            {
                // Long decay, everything up: no louder than a little over the Hall with the same
                // settings, and dying away once the input stops.
                auto hall = wet (Type::hall);
                hall.decay = 20.0f;
                hall.lowDecay = 2.0f;
                hall.highDecay = 1.0f;
                float hallPeak = 0.0f;
                expect (healthy (render (noiseBurst (6.0, 10.0), hall), hallPeak));

                for (auto t : { Type::shimmer, Type::cloud })
                {
                    auto s = hall;
                    s.type = t;
                    s.character = t == Type::shimmer ? 1.0f : 0.5f;
                    s.variation = 1.0f;
                    const auto out = render (noiseBurst (6.0, 10.0), s);
                    float peak = 0.0f;
                    expect (healthy (out, peak), names[(int) t] + " stays finite");
                    logMessage (names[(int) t] + ": peak " + juce::String (juce::Decibels::gainToDecibels (peak), 1) + " dB, the Hall "
                                + juce::String (juce::Decibels::gainToDecibels (hallPeak), 1) + " dB");
                    expectLessThan (peak, hallPeak * 2.0f, names[(int) t] + " peak");
                    expectLessThan (rmsDb (out, 9.0, 10.0), rmsDb (out, 5.0, 6.0) - 3.0f, names[(int) t] + " dies away");
                }

                // Frozen: held, not growing.
                auto frozen = wet (Type::shimmer);
                frozen.character = 1.0f;
                Buffer in = noiseBurst (1.0, 6.0);
                ReverbEngine engine;
                engine.prepare (rate, 512, frozen);
                engine.jumpTo (frozen);
                for (int pos = 0; pos < in.getNumSamples(); pos += 512)
                {
                    auto s = frozen;
                    s.freeze = pos >= (int) (1.5 * rate);
                    engine.process (in, pos, juce::jmin (512, in.getNumSamples() - pos), s);
                }
                float peak = 0.0f;
                expect (healthy (in, peak));
                expectLessThan (rmsDb (in, 5.0, 6.0), rmsDb (in, 2.0, 3.0) + 3.0f, "frozen shimmer holds without growing");
                expectGreaterThan (rmsDb (in, 5.0, 6.0), rmsDb (in, 2.0, 3.0) - 12.0f, "frozen shimmer holds");
            }

            beginTest ("Reverse and Bloom swell, the Hall doesn't");
            {
                auto reverse = wet (Type::reverse);
                reverse.decay = 1.0f;
                const auto window = ReverbEngine::getShapeSeconds (reverse);
                const auto reversed = render (click (2.0), reverse);
                const auto reverseAt = loudestAt (reversed, 1.5);

                auto bloom = wet (Type::bloom);
                bloom.character = 0.6f;
                const auto bloomAt = loudestAt (render (click (2.0), bloom), 1.5);
                const auto hallAt = loudestAt (render (click (2.0), wet (Type::hall)), 1.5);

                logMessage ("Loudest at " + juce::String (reverseAt, 2) + " s in Reverse (window " + juce::String (window, 2) + " s), "
                            + juce::String (bloomAt, 2) + " s in Bloom, " + juce::String (hallAt, 2) + " s in the Hall");
                expectLessThan (hallAt, 0.15);
                expectGreaterThan (reverseAt, window * 0.6);
                expectGreaterThan (bloomAt, 0.3);
                expectGreaterThan (rmsDb (reversed, reverseAt, reverseAt + 0.05), rmsDb (reversed, 0.0, 0.05) + 10.0f, "Reverse starts quiet");
            }

            beginTest ("Gated: cut off after the hold");
            {
                auto gated = wet (Type::gated);
                gated.decay = 3.0f;
                auto room = wet (Type::room);
                room.decay = 3.0f;

                const auto cut = render (noiseBurst (0.2, 1.5), gated), open = render (noiseBurst (0.2, 1.5), room);
                const auto during = rmsDb (cut, 0.1, 0.2), after = rmsDb (cut, 0.6, 1.0), ringing = rmsDb (open, 0.6, 1.0);
                logMessage ("Gated: " + juce::String (during, 1) + " dB during, " + juce::String (after, 1) + " dB after the gate; the Room "
                            + juce::String (ringing, 1) + " dB");
                expectLessThan (after, during - 60.0f);
                expectLessThan (after, ringing - 40.0f);
            }

            beginTest ("Duck: the reverb gets out of the way while the input plays");
            {
                auto plain = wet (Type::hall);
                auto ducked = plain;
                ducked.duck = 1.0f;

                const auto in = sineBurst (1000.0, 1.0, 2.5);
                const auto a = render (in, plain), b = render (in, ducked);
                const auto playing = rmsDb (a, 0.5, 0.9) - rmsDb (b, 0.5, 0.9), after = rmsDb (a, 1.8, 2.2) - rmsDb (b, 1.8, 2.2);
                logMessage ("Ducked by " + juce::String (playing, 1) + " dB while playing, " + juce::String (after, 1) + " dB 0.8 s after");
                expectGreaterThan (playing, 12.0f);
                expectLessThan (after, 3.0f);
            }

            beginTest ("Every type comes out the same whatever the block size");
            {
                for (int t = 0; t < ReverbEngine::numTypes; ++t)
                {
                    auto s = wet ((Type) t);
                    s.duck = 0.5f;
                    const auto in = noiseBurst (0.3, 1.5);
                    const auto big = render (in, s, 512), small = render (in, s, 7);

                    float worst = 0.0f;
                    for (int ch = 0; ch < 2; ++ch)
                        for (int i = 0; i < big.getNumSamples(); ++i)
                            worst = juce::jmax (worst, std::abs (big.getSample (ch, i) - small.getSample (ch, i)));

                    expectLessThan (worst, 1.0e-5f, names[t]);
                }
            }

            beginTest ("The display's shapes match the types");
            {
                auto reverse = wet (Type::reverse);
                const auto window = ReverbEngine::getShapeSeconds (reverse);
                expectGreaterThan (ReverbEngine::getLevelDb (reverse, 2.0, window * 0.9), ReverbEngine::getLevelDb (reverse, 2.0, window * 0.1));
                expectEquals (ReverbEngine::getLevelDb (reverse, 2.0, window * 1.01), -200.0);

                auto gated = wet (Type::gated);
                expectEquals (ReverbEngine::getLevelDb (gated, 3.0, ReverbEngine::getShapeSeconds (gated) + 0.01), -200.0);
                expectGreaterThan (ReverbEngine::getLevelDb (gated, 3.0, 0.1), -10.0);

                auto hall = wet (Type::hall);
                expectWithinAbsoluteError (ReverbEngine::getLevelDb (hall, 2.0, 1.0), -30.0, 0.5);
                expectEquals (ReverbEngine::getShapeSeconds (hall), 0.0);

                for (int t = 0; t < ReverbEngine::numTypes; ++t)
                    expectGreaterThan (ReverbEngine::getTailSeconds (wet ((Type) t)), ReverbEngine::getShapeSeconds (wet ((Type) t)));

                expect (ReverbEngine::getEarlyTaps (wet (Type::plate)).empty(), "a plate has no early reflections");
                expectEquals ((int) ReverbEngine::getEarlyTaps (wet (Type::room)).size(), 2 * ReverbEngine::numTaps);
            }

            beginTest ("Presets: the defaults first, every value in range, every type covered");
            {
                const auto& factory = CVerbPresets::factory();
                expectEquals (factory[0].name, juce::String ("Default Hall"));
                expect (factory[0].values.empty(), "the first preset is the defaults");
                expectGreaterThan ((int) factory.size(), 20);

                juce::StringArray seenNames, seenTypes;
                for (auto& preset : factory)
                {
                    expect (! seenNames.contains (preset.name), preset.name + " is listed once");
                    seenNames.add (preset.name);
                    seenTypes.addIfNotAlreadyThere (preset.category);

                    const auto values = CVerbPresets::valuesOf (preset);
                    for (int i = 0; i < CVerbParameters::count; ++i)
                    {
                        const auto& spec = CVerbParameters::get ((CVerbParameters::ID) i);
                        expect (values[(size_t) i] >= spec.minimum && values[(size_t) i] <= spec.maximum, preset.name + ": " + spec.id + " in range");
                    }

                    expectEquals ((int) CVerbParameters::typeOf (values[CVerbParameters::type]), names.indexOf (preset.category),
                                  preset.name + " is filed under its type");
                }

                expectEquals (seenTypes.size(), ReverbEngine::numTypes);
            }

            beginTest ("Presets: your own are saved and read back");
            {
                auto folder = getTestDir().getChildFile ("CVerbPresets");
                folder.deleteRecursively();
                CVerbPresets::setUserFolder (folder);

                const auto values = CVerbPresets::valuesOf (CVerbPresets::factory()[12]);
                expect (CVerbPresets::saveUser ("My Space", [&values] (int id) { return values[(size_t) id]; }).wasOk());
                const auto loaded = CVerbPresets::loadUser();
                expectEquals ((int) loaded.size(), 1);

                if (loaded.size() == 1)
                {
                    expectEquals (loaded[0].name, juce::String ("My Space"));
                    expect (loaded[0].isUser());
                    const auto back = CVerbPresets::valuesOf (loaded[0]);
                    for (int i = 0; i < CVerbParameters::count; ++i)
                        expectWithinAbsoluteError (back[(size_t) i], values[(size_t) i], 1.0e-4f);
                }

                expect (CVerbPresets::saveUser ("  ", [&values] (int id) { return values[(size_t) id]; }).failed(), "a preset needs a name");
                CVerbPresets::setUserFolder ({});
                folder.deleteRecursively();
            }
        }
    };

    static ReverbTypesTests reverbTypesTests;
}
