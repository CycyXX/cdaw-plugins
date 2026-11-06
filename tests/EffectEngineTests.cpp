#include "TestSupport.h"
#include "DspScenarios.h"
#include "Shared/Chorus/ChorusParameters.h"
#include "Shared/Delay/DelayParameters.h"
#include "Shared/DSP/CompressorMath.h"
#include "Shared/Esser/EsserParameters.h"
#include "Shared/Filter/FilterParameters.h"
#include "Shared/Phaser/PhaserParameters.h"

namespace cdaw::test
{
    /** The effects' sound and their displays' maths, straight through the engines. */
    struct EffectEngineTests : public juce::UnitTest
    {
        EffectEngineTests() : juce::UnitTest ("Effect engines", "Plugins") {}

        using Buffer = juce::AudioBuffer<float>;
        using Changes = std::initializer_list<std::pair<int, float>>;
        static constexpr double rate = 48000.0;

        /** Settings from a parameter list's defaults with some values changed, as a host would have them. */
        template <typename ToSettings>
        static auto settingsFrom (const ParameterSpecs& specs, Changes changes, ToSettings toSettings)
        {
            std::vector<float> values;
            for (auto& spec : specs)
                values.push_back (spec.defaultValue);
            for (auto& [id, value] : changes)
                values[(size_t) id] = value;
            return toSettings ([&values] (int id) { return values[(size_t) id]; });
        }

        static ChorusEngine::Settings chorusSettings (Changes c)    { return settingsFrom (ChorusParameters::all(), c, ChorusParameters::toSettings); }
        static PhaserEngine::Settings phaserSettings (Changes c)    { return settingsFrom (PhaserParameters::all(), c, PhaserParameters::toSettings); }
        static FilterEngine::Settings filterSettings (Changes c)    { return settingsFrom (FilterParameters::all(), c, FilterParameters::toSettings); }

        /** The input through an engine in host-sized blocks, the song playing, settings changing at `switchAt`. */
        template <typename Engine, typename Settings>
        static Buffer through (const Buffer& input, const Settings& first, const Settings& second, double switchAt)
        {
            auto process = dsp::make<Engine> ("", first, second, rate).create (rate);
            Buffer out (input);
            juce::MidiBuffer midi;

            for (int pos = 0; pos < out.getNumSamples(); pos += 512)
            {
                const auto now = pos / rate;
                process ({ out, pos, juce::jmin (512, out.getNumSamples() - pos), midi, { true, now, now * dsp::bpm / 60.0 }, now >= switchAt });
            }

            return out;
        }

        template <typename Engine, typename Settings>
        static Buffer through (const Buffer& input, const Settings& s)      { return through<Engine> (input, s, s, 1.0e9); }

        static Buffer sine (double hz, float amplitude = 0.25f, double seconds = 1.0)   { return makeSine (hz, amplitude, seconds); }

        /** One click at sample 10 on the chosen channels, then silence. */
        static Buffer click (double seconds, bool left = true, bool right = true)
        {
            Buffer b (2, (int) (seconds * rate));
            b.clear();
            if (left)  b.setSample (0, 10, 1.0f);
            if (right) b.setSample (1, 10, 1.0f);
            return b;
        }

        static float rmsDb (const Buffer& b, double from, double to, int channel = 0)
        {
            auto s = (int) (from * rate), e = juce::jmin (b.getNumSamples(), (int) (to * rate));
            return juce::Decibels::gainToDecibels (b.getRMSLevel (channel, s, e - s), -150.0f);
        }

        /** The gain (dB) an engine applies to a steady sine. */
        template <typename Engine, typename Settings>
        static float gainThrough (double hz, const Settings& s, float amplitude = 0.25f)
        {
            const auto in = sine (hz, amplitude);
            return rmsDb (through<Engine> (in, s), 0.3, 0.9) - rmsDb (in, 0.3, 0.9);
        }

        /** The loudest sample (and where) within +-`window` samples of `around`. */
        static std::pair<float, int> peakNear (const Buffer& b, int channel, int around, int window = 200)
        {
            float best = 0.0f;
            int at = -1;
            for (int i = juce::jmax (0, around - window); i < juce::jmin (b.getNumSamples(), around + window); ++i)
                if (std::abs (b.getSample (channel, i)) > best)
                {
                    best = std::abs (b.getSample (channel, i));
                    at = i;
                }
            return { best, at };
        }

        /** How much the level of a steady sine moves (dB, loudest minus quietest 20 ms window). */
        static float levelSwing (const Buffer& b, double from, double to, int channel = 0)
        {
            float lo = 1000.0f, hi = -1000.0f;
            for (double t = from; t + 0.02 <= to; t += 0.02)
            {
                auto db = rmsDb (b, t, t + 0.02, channel);
                lo = juce::jmin (lo, db);
                hi = juce::jmax (hi, db);
            }
            return hi - lo;
        }

        /** Decay rate (dB per second) between two times: a straight-line fit to 50 ms energies. */
        static double decaySlope (const Buffer& b, double from, double to)
        {
            std::vector<std::pair<double, double>> points;
            const int window = (int) (0.05 * rate);

            for (double t = from; t + 0.05 <= to; t += 0.05)
            {
                auto start = (int) (t * rate);
                auto rms = juce::jmax (b.getRMSLevel (0, start, window), b.getRMSLevel (1, start, window));
                points.push_back ({ t, 20.0 * std::log10 (juce::jmax (1.0e-9f, rms)) });
            }

            double mx = 0, my = 0;
            for (auto& [x, y] : points) { mx += x; my += y; }
            mx /= (double) points.size();
            my /= (double) points.size();

            double num = 0, den = 0;
            for (auto& [x, y] : points) { num += (x - mx) * (y - my); den += (x - mx) * (x - mx); }
            return num / den;
        }

        static ReverbEngine::Settings plainReverb()
        {
            ReverbEngine::Settings s;
            s.mix = 1.0f;
            s.preDelay = 0.0f;
            s.early = 0.0f;
            s.modulation = 0.0f;
            s.lowDecay = 1.0f;
            s.highDecay = 1.0f;
            s.postLowCut = 20.0f;
            s.postHighCut = 20000.0f;
            s.decay = 2.0f;
            return s;
        }

        void runTest() override
        {
            using DP = DelayParameters::ID;
            using CP = ChorusParameters::ID;
            using PP = PhaserParameters::ID;
            using FP = FilterParameters::ID;
            using Type = FilterEngine::Type;

            // Each delay test sets these, then its own values on top.
            auto delayWith = [] (std::vector<std::pair<int, float>> c)
            {
                std::vector<float> values;
                for (auto& spec : DelayParameters::all())
                    values.push_back (spec.defaultValue);
                values[DP::mix] = 1.0f;
                values[DP::lowCut] = DelayEngine::lowCutOff;
                values[DP::highCut] = DelayEngine::highCutOff;
                values[DP::sync] = 0.0f;
                for (auto& [id, value] : c)
                    values[(size_t) id] = value;
                return DelayParameters::toSettings ([&values] (int id) { return values[(size_t) id]; });
            };

            const auto dry = through<DelayEngine> (click (2.0), delayWith ({ { DP::mix, 0.0f } }));
            const auto [clickLevel, clickAt] = peakNear (dry, 0, 10, 20);

            beginTest ("C-Delay: echoes arrive on time and fade by the feedback");
            {
                auto r = through<DelayEngine> (click (2.0), delayWith ({ { DP::time, 250.0f }, { DP::feedback, 0.5f } }));
                const int step = (int) std::round (0.25 * rate);
                auto [first, firstAt] = peakNear (r, 0, clickAt + step);
                auto [second, secondAt] = peakNear (r, 0, clickAt + 2 * step);
                auto [third, thirdAt] = peakNear (r, 0, clickAt + 3 * step);
                juce::ignoreUnused (thirdAt);

                expectWithinAbsoluteError (firstAt, clickAt + step, 1);
                expectWithinAbsoluteError (secondAt, clickAt + 2 * step, 1);
                expectWithinAbsoluteError (first / clickLevel, 1.0f, 0.02f);
                expectWithinAbsoluteError (second / clickLevel, 0.5f, 0.02f);
                expectWithinAbsoluteError (third / clickLevel, 0.25f, 0.02f);
                expectLessThan (std::abs (r.getSample (0, clickAt)), 1.0e-4f, "mix 100 % has no dry signal");
            }

            beginTest ("C-Delay: synced to the tempo, the display's echoes match the sound");
            {
                const auto settings = delayWith ({ { DP::sync, 1.0f }, { DP::division, 9.0f }, { DP::feedback, 0.6f } });     // 1/4: half a second at 120 bpm
                auto r = through<DelayEngine> (click (3.0), settings);
                const int quarter = (int) std::round (0.5 * rate);
                expectWithinAbsoluteError (peakNear (r, 0, clickAt + quarter).second, clickAt + quarter, 1, "a quarter note at 120 bpm");

                auto echoes = DelayEngine::getEchoes (settings, 120.0, rate);
                expect (echoes.size() >= 3);
                for (int i = 0; i < juce::jmin (3, (int) echoes.size()); ++i)
                {
                    auto& e = echoes[(size_t) i];
                    auto level = peakNear (r, 0, clickAt + (int) std::round (e.seconds * rate)).first / clickLevel;
                    expectWithinAbsoluteError (juce::Decibels::gainToDecibels (level), e.gainDb, 0.3f, "echo " + juce::String (i + 1) + " as loud as drawn");
                }
            }

            beginTest ("C-Delay: ping-pong bounces from left to right");
            {
                auto r = through<DelayEngine> (click (2.0), delayWith ({ { DP::time, 200.0f }, { DP::feedback, 0.5f }, { DP::mode, 1.0f } }));
                const int step = (int) std::round (0.2 * rate);
                expectWithinAbsoluteError (peakNear (r, 0, clickAt + step).first / clickLevel, 1.0f, 0.02f);
                expectLessThan (peakNear (r, 1, clickAt + step).first / clickLevel, 0.01f);
                expectLessThan (peakNear (r, 0, clickAt + 2 * step).first / clickLevel, 0.01f);
                expectWithinAbsoluteError (peakNear (r, 1, clickAt + 2 * step).first / clickLevel, 0.5f, 0.02f);
            }

            beginTest ("C-Delay: mix 0 is dry, full feedback stays bounded, cuts darken the repeats");
            {
                expectGreaterThan (clickLevel, 0.5f, "the dry click comes through");
                expectLessThan (peakNear (dry, 0, clickAt + (int) (0.375 * rate), 2000).first, 1.0e-5f, "no echoes at mix 0");

                auto repeatsAfter3s = [&] (float drive)
                {
                    auto r = through<DelayEngine> (click (4.0), delayWith ({ { DP::time, 50.0f }, { DP::feedback, 1.0f }, { DP::drive, drive } }));
                    return r.getMagnitude (0, (int) (3.0 * rate), (int) (0.5 * rate));
                };

                auto clean = repeatsAfter3s (0.0f), driven = repeatsAfter3s (1.0f);
                expectWithinAbsoluteError (clean / clickLevel, 1.0f, 0.01f, "clean repeats go on forever, at the same level");
                expectGreaterThan (driven, 0.05f, "driven repeats still go on");
                expectLessThan (driven, clean, "but the drive wears them down");

                auto bright = DelayEngine::Settings {};
                bright.lowCut = DelayEngine::lowCutOff;
                bright.highCut = 3000.0f;
                expectLessThan (DelayEngine::getLoopFilterDb (bright, 8000.0, rate), -6.0, "8 kHz is cut by a 3 kHz high cut");
                expectWithinAbsoluteError (DelayEngine::getLoopFilterDb (bright, 200.0, rate), 0.0, 0.1);
            }

            beginTest ("C-Chorus: a still voice is a delay, and the comb it makes is the one drawn");
            {
                const auto settings = chorusSettings ({ { CP::voices, 1.0f }, { CP::depth, 0.0f }, { CP::delay, 1.0f }, { CP::feedback, 0.0f },
                                                        { CP::spread, 0.0f }, { CP::tone, ChorusEngine::toneOff }, { CP::mix, 0.5f } });
                const float delays[ChorusEngine::maxVoices] { 1.0f, 1.0f, 1.0f, 1.0f };

                for (double hz : { 500.0, 750.0, 1000.0, 1250.0 })
                {
                    auto measured = gainThrough<ChorusEngine> (hz, settings);
                    auto predicted = (float) ChorusEngine::getResponseDb (settings, delays, hz, rate);
                    logMessage (juce::String (hz) + " Hz through a 1 ms voice at 50 %: " + juce::String (measured, 2) + " dB (drawn "
                                + juce::String (juce::jmax (-99.0f, predicted), 2) + " dB)");
                    if (predicted > -20.0f)
                        expectWithinAbsoluteError (measured, predicted, 0.5f);
                    else
                        expectLessThan (measured, -25.0f, "the notch");
                }

                expectWithinAbsoluteError (gainThrough<ChorusEngine> (1000.0, chorusSettings ({ { CP::mix, 0.0f } })), 0.0f, 0.01f, "mix 0 = dry");
            }

            beginTest ("C-Chorus: voices move and spread the sound apart");
            {
                auto r = through<ChorusEngine> (sine (2000.0), chorusSettings ({ { CP::voices, 2.0f }, { CP::spread, 1.0f }, { CP::depth, 0.8f }, { CP::rate, 2.0f }, { CP::mix, 0.5f } }));
                const int start = (int) (0.2 * rate), n = (int) (0.7 * rate);
                Buffer difference (1, n);
                difference.copyFrom (0, 0, r, 0, start, n);
                difference.addFrom (0, 0, r, 1, start, n, -1.0f);
                expectGreaterThan (difference.getRMSLevel (0, 0, n) / juce::jmax (1.0e-9f, r.getRMSLevel (0, start, n)), 0.2f, "the sides differ");
                expectGreaterThan (levelSwing (r, 0.2, 0.9), 3.0f, "the comb moves, so the level of a tone does");
            }

            beginTest ("C-Phaser: with the sweep still, it sounds as the response is drawn; the LFO moves it");
            {
                const auto settings = phaserSettings ({ { PP::depth, 0.0f }, { PP::centre, 1000.0f }, { PP::stages, 4.0f }, { PP::feedback, 0.4f }, { PP::mix, 0.5f } });

                for (double hz : { 300.0, 1000.0, 2414.0, 6000.0 })
                {
                    auto measured = gainThrough<PhaserEngine> (hz, settings);
                    auto predicted = (float) PhaserEngine::getResponseDb (settings, 1000.0, hz, rate);
                    logMessage (juce::String (hz) + " Hz, 4 stages at 1 kHz: " + juce::String (measured, 2) + " dB (drawn " + juce::String (predicted, 2) + " dB)");
                    if (predicted > -20.0f)
                        expectWithinAbsoluteError (measured, predicted, 0.6f);
                    else
                        expectLessThan (measured, -18.0f, "the notch");
                }

                auto moving = through<PhaserEngine> (sine (1000.0), phaserSettings ({ { PP::depth, 0.8f }, { PP::rate, 2.0f } }));
                expectGreaterThan (levelSwing (moving, 0.1, 0.95), 10.0f, "a sweep moves a tone's level");

                PhaserEngine::Settings synced;
                synced.sync = true;
                synced.division = 9;    // 1/4
                expectWithinAbsoluteError (PhaserEngine::getRateHz (synced, 120.0), 2.0, 1.0e-9, "a quarter note at 120 bpm is 2 Hz");
                expectEquals (TempoSync::nearest (0.375, 120.0), 8, "0.375 s is a dotted eighth at 120 bpm");
            }

            beginTest ("LFOs keep in step with the song");
            {
                TempoSync::LfoClock clock;
                expect (! clock.shouldLock ({ false, 1.0, 2.0 }, 512, rate), "stopped: runs freely");
                expect (clock.shouldLock ({ true, 1.0, 2.0 }, 512, rate), "playback starts: picks up the song position");
                expect (! clock.shouldLock ({ true, 1.0 + 512 / rate, 2.0 }, 512, rate), "playing on: runs on");
                expect (clock.shouldLock ({ true, 5.0, 10.0 }, 512, rate), "a jump: picks it up again");

                expectWithinAbsoluteError (TempoSync::LfoClock::syncedPhaseAt ({ true, 0.0, 9.5 }, 9), 0.5, 1.0e-9, "a 1/4 LFO halfway through beat 10");
                expectWithinAbsoluteError (TempoSync::LfoClock::syncedPhaseAt ({ true, 0.0, 6.0 }, 15), 0.5, 1.0e-9, "a 1/1 LFO halfway through bar 2");
                expectWithinAbsoluteError (TempoSync::LfoClock::phaseAt ({ true, 2.25, 0.0 }, 2.0), 0.5, 1.0e-9, "2 Hz, 2.25 s in");
            }

            beginTest ("C-Filter: each type cuts where its curve says; resonance, drive, mix, output");
            {
                struct Case { Type type; bool steep; float resonance; double hz; };
                FilterEngine::Settings last;

                for (auto c : { Case { Type::lowPass, false, 0.0f, 1000.0 }, Case { Type::lowPass, false, 0.0f, 4000.0 },
                                Case { Type::highPass, true, 0.0f, 250.0 }, Case { Type::highPass, true, 0.0f, 4000.0 },
                                Case { Type::bandPass, false, 0.5f, 1000.0 }, Case { Type::bandPass, true, 0.5f, 2000.0 },
                                Case { Type::notch, false, 0.0f, 1000.0 }, Case { Type::lowPass, false, 1.0f, 1000.0 } })
                {
                    last = filterSettings ({ { FP::type, (float) c.type }, { FP::slope, c.steep ? 1.0f : 0.0f }, { FP::cutoff, 1000.0f }, { FP::resonance, c.resonance } });

                    // A full resonance peak would take a -12 dB tone past full scale: test it quieter.
                    auto measured = gainThrough<FilterEngine> (c.hz, last, c.resonance > 0.9f ? 0.02f : 0.25f);
                    auto predicted = (float) FilterEngine::getResponseDb (last, 1000.0, c.hz, rate);
                    logMessage (FilterParameters::getTypeNames()[(int) c.type] + (c.steep ? " 24" : " 12") + " at 1 kHz, resonance "
                                + juce::String (c.resonance, 1) + ", " + juce::String (c.hz) + " Hz: " + juce::String (measured, 2)
                                + " dB (drawn " + juce::String (juce::jmax (-99.0f, predicted), 2) + " dB)");
                    if (predicted > -40.0f)
                        expectWithinAbsoluteError (measured, predicted, 0.3f);
                    else
                        expectLessThan (measured, -35.0f);
                }

                expectWithinAbsoluteError (FilterEngine::getResponseDb (last, 1000.0, 1000.0, rate), 20.0 * std::log10 (FilterEngine::getQ (last)), 0.1,
                                           "full resonance peaks by Q");

                expectWithinAbsoluteError (gainThrough<FilterEngine> (1000.0, filterSettings ({ { FP::cutoff, 100.0f }, { FP::mix, 0.0f }, { FP::output, 6.0f } })),
                                           6.0f, 0.05f, "mix 0 passes the input, output adds its gain");
                expectLessThan (gainThrough<FilterEngine> (100.0, filterSettings ({ { FP::cutoff, 20000.0f }, { FP::drive, 1.0f } })), -1.0f, "drive squashes loud signals");
            }

            beginTest ("C-Filter: the LFO and the envelope move the cutoff");
            {
                auto withEnvelope = [] (float amount) { return gainThrough<FilterEngine> (2000.0, filterSettings ({ { FP::cutoff, 200.0f }, { FP::envAmount, amount } })); };
                expectGreaterThan (withEnvelope (1.0f), withEnvelope (0.0f) + 15.0f, "the envelope opens the filter");

                auto wobble = through<FilterEngine> (sine (2000.0), filterSettings ({ { FP::cutoff, 1000.0f }, { FP::lfoAmount, 0.5f }, { FP::lfoRate, 2.0f } }));
                expectGreaterThan (levelSwing (wobble, 0.1, 0.95), 10.0f, "the LFO moves a tone's level");

                for (int shape = 0; shape < (int) FilterEngine::Shape::numShapes; ++shape)
                    for (double phase : { 0.0, 0.13, 0.5, 0.77 })
                    {
                        auto v = FilterEngine::lfoValue ((FilterEngine::Shape) shape, phase, 0.3f);
                        expect (v >= -1.0f && v <= 1.0f, "LFO shapes stay within -1..1");
                    }
            }

            beginTest ("C-Esser: the band is turned down by the range above the threshold, left alone below it");
            {
                auto esser = [] (double hz, std::function<void (EsserEngine::Settings&)> change, float amplitude = 0.25f)
                {
                    EsserEngine::Settings s;
                    s.thresholdDb = -40.0f;
                    s.rangeDb = 12.0f;
                    change (s);

                    EsserEngine e;
                    e.prepare (rate, 4800);
                    auto buffer = makeSine (hz, amplitude, 1.0);
                    for (int start = 0; start < buffer.getNumSamples(); start += 480)
                        e.process (buffer, start, 480, s);

                    const float rms = buffer.getRMSLevel (0, 24000, 24000);
                    return juce::Decibels::gainToDecibels (rms / (amplitude * juce::MathConstants<float>::sqrt2 * 0.5f), -150.0f);
                };

                EsserEngine::Settings defaults;
                const auto centre = EsserEngine::getCentreHz (defaults);
                expectWithinAbsoluteError ((float) centre, 6325.0f, 1.0f, "the band's centre is between its edges");

                expectWithinAbsoluteError (esser (centre, [] (auto& s) { s.thresholdDb = 0.0f; }), 0.0f, 0.01f, "below the threshold: untouched");
                expectWithinAbsoluteError (esser (centre, [] (auto&) {}), -12.0f, 0.5f, "far above: turned down by the range");
                expectWithinAbsoluteError (esser (centre, [] (auto& s) { s.rangeDb = 6.0f; }), -6.0f, 0.5f, "a smaller range reduces less");
                expectGreaterThan (esser (500.0, [] (auto&) {}), -0.3f, "below the band: the detector doesn't hear it");
                expectWithinAbsoluteError (esser (centre, [] (auto& s) { s.mode = EsserEngine::Mode::wideband; }), -12.0f, 0.5f, "wideband");

                const float level = esser (centre, [] (auto& s) { s.listen = EsserEngine::Listen::band; s.thresholdDb = 0.0f; }) - 12.0f;
                const float slight = esser (centre, [level] (auto& s) { s.thresholdDb = level - 4.0f; });
                expectWithinAbsoluteError (slight, -EsserEngine::getReductionDb ({}, level, level - 4.0f), 0.6f, "a soft 4:1 just above the threshold");
                expectGreaterThan (slight, -4.0f);

                expectWithinAbsoluteError (esser (centre, [] (auto& s) { s.listen = EsserEngine::Listen::removed; }),
                                           juce::Decibels::gainToDecibels (1.0f - juce::Decibels::decibelsToGain (-12.0f)), 0.5f,
                                           "listening to what is removed");

                const float autoLoud = esser (centre, [] (auto& s) { s.autoThreshold = true; s.thresholdDb = 0.0f; });
                const float autoQuiet = esser (centre, [] (auto& s) { s.autoThreshold = true; s.thresholdDb = 0.0f; }, 0.025f);
                expect (autoLoud < -3.0f && autoLoud > -11.0f, "auto reduces a sibilant: " + juce::String (autoLoud));
                expectWithinAbsoluteError (autoQuiet, autoLoud, 0.5f, "auto follows the level: 20 dB quieter, the same reduction");

                EsserEngine::Settings split;
                expectWithinAbsoluteError ((float) EsserEngine::getResponseDb (split, 12.0f, centre, rate), -12.0f, 0.01f);
                expectGreaterThan ((float) EsserEngine::getResponseDb (split, 12.0f, 500.0, rate), -0.5f, "split: the lows stay");
                expectWithinAbsoluteError ((float) EsserEngine::getResponseDb (split, 0.0f, 2000.0, rate), 0.0f, 0.001f, "flat when not reducing");
            }

            beginTest ("The filter maths shared by the crossovers and cuts: peak, shelf and steep cuts");
            {
                using T = EqMath::BandType;
                expectWithinAbsoluteError (EqMath::design (T::peak, 0, 1000.0, 6.0, 1.0, rate).magnitudeDb (1000.0, rate), 6.0, 0.01);
                expectWithinAbsoluteError (EqMath::design (T::peak, 0, 1000.0, 6.0, 1.0, rate).magnitudeDb (50.0, rate), 0.0, 0.1);
                expectWithinAbsoluteError (EqMath::design (T::lowShelf, 0, 200.0, -4.0, 0.71, rate).magnitudeDb (20.0, rate), -4.0, 0.15);

                auto cut48 = EqMath::design (T::lowCut, 3, 1000.0, 0.0, 0.71, rate);
                expectWithinAbsoluteError (cut48.magnitudeDb (1000.0, rate), -3.01, 0.1);
                expectWithinAbsoluteError (cut48.magnitudeDb (500.0, rate), -48.2, 0.5);
                expectWithinAbsoluteError (cut48.magnitudeDb (8000.0, rate), 0.0, 0.05);

                auto cut6 = EqMath::design (T::highCut, 0, 1000.0, 0.0, 0.71, rate);
                expectWithinAbsoluteError (cut6.magnitudeDb (1000.0, rate), -3.01, 0.15);
                expectLessThan (EqMath::design (T::notch, 0, 1000.0, 0.0, 4.0, rate).magnitudeDb (1000.0, rate), -60.0);
            }

            beginTest ("C-Comp: the curve's threshold, ratio and knee");
            {
                using CompressorMath::transfer;
                expectWithinAbsoluteError (transfer (-6.0f, -20.0f, 4.0f, 0.0f), -16.5f, 0.001f);
                expectWithinAbsoluteError (transfer (-30.0f, -20.0f, 4.0f, 0.0f), -30.0f, 0.001f);
                expectWithinAbsoluteError (transfer (0.0f, -20.0f, 1000.0f, 0.0f), -20.0f, 0.05f, "limiter holds the threshold");
                expectWithinAbsoluteError (transfer (-23.0f, -20.0f, 4.0f, 6.0f), -23.0f, 0.001f);
                expectWithinAbsoluteError (transfer (-17.0f, -20.0f, 4.0f, 6.0f), -19.25f, 0.001f);
            }

            beginTest ("C-Comp: reduces a steady tone by the expected amount and meters it");
            {
                CompressorEngine::Settings s;
                s.thresholdDb = -24.0f;
                s.ratio = 4.0f;
                s.kneeDb = 0.0f;
                s.attackMs = 1.0f;
                s.releaseMs = 100.0f;

                CompressorEngine e;
                e.prepare (rate);
                const auto in = sine (1000.0);
                auto out = in;
                for (int pos = 0; pos < out.getNumSamples(); pos += 512)
                    e.process (out, pos, juce::jmin (512, out.getNumSamples() - pos), s);

                // A sine at 0.25 peaks at -12 dB: 12 dB over a -24 dB threshold at 4:1 -> 9 dB less.
                expectWithinAbsoluteError (rmsDb (out, 0.3, 0.9) - rmsDb (in, 0.3, 0.9), -9.0f, 1.0f);
                expectWithinAbsoluteError (e.gainReduction.take(), 9.0f, 1.0f, "metered gain reduction");
                expectGreaterThan (e.inputPeak[0].take(), 0.2f);
            }

            beginTest ("C-Multicomp: crossovers add back up flat");
            {
                MultibandEngine::Settings open;
                for (auto& band : open.bands)
                {
                    band.on = true;
                    band.threshold = 0.0f;      // never reached
                }

                for (double hz : { 40.0, 120.0, 700.0, 1000.0, 3000.0, 6000.0, 12000.0 })
                    expectWithinAbsoluteError (gainThrough<MultibandEngine> (hz, open), 0.0f, 0.05f, juce::String (hz) + " Hz through the uncompressed bands");
            }

            beginTest ("C-Multicomp: each band compresses only its own range");
            {
                MultibandEngine::Settings s;
                for (auto& band : s.bands)
                    band.threshold = 0.0f;
                s.bands[0].on = true;
                s.bands[0].threshold = -30.0f;
                s.bands[0].ratio = 4.0f;
                s.bands[0].attack = 1.0f;

                // A -12 dB peak at 50 Hz: 18 dB over -30 at 4:1 -> about 13.5 dB less.
                expectWithinAbsoluteError (gainThrough<MultibandEngine> (50.0, s), -13.3f, 1.5f);
                expectWithinAbsoluteError (gainThrough<MultibandEngine> (5000.0, s), 0.0f, 0.05f);

                MultibandEngine::Settings soloed;
                soloed.bands[3].solo = true;
                expectLessThan (gainThrough<MultibandEngine> (50.0, soloed), -60.0f, "50 Hz with only the high band soloed");
            }

            beginTest ("C-Verb: the tail decays at the set RT60");
            {
                auto rt60 = -60.0 / decaySlope (through<ReverbEngine> (click (4.0), plainReverb()), 0.3, 1.5);
                logMessage ("Measured RT60: " + juce::String (rt60, 2) + " s (set to 2.0 s)");
                expectWithinAbsoluteError (rt60, 2.0, 0.3);
            }

            beginTest ("C-Verb: level, cuts, mix");
            {
                // About +4 dB: the network is flat, its level set to match the earlier engine on
                // broadband sound (whose diffusers put 1 kHz in a dip).
                expectWithinAbsoluteError (gainThrough<ReverbEngine> (1000.0, plainReverb()), 3.0f, 6.0f, "1 kHz, fully wet");

                auto cutAt = [] (bool pre, bool post)
                {
                    auto s = plainReverb();
                    if (pre)  s.preLowCut = 800.0f;
                    if (post) s.postLowCut = 800.0f;
                    return gainThrough<ReverbEngine> (80.0, s);
                };

                const auto open = cutAt (false, false), preCut = cutAt (true, false), postCut = cutAt (false, true);
                logMessage ("80 Hz into the reverb: " + juce::String (open, 1) + " dB, post cut at 800 Hz: " + juce::String (postCut, 1)
                            + " dB, pre cut at 800 Hz: " + juce::String (preCut, 1) + " dB");
                expectLessThan (postCut, open - 30.0f);
                expectLessThan (preCut, open - 30.0f);
                expectWithinAbsoluteError (postCut, preCut, 0.5f, "the reverb is linear: a cut before it and after it come out the same");

                auto dryMix = plainReverb();
                dryMix.mix = 0.0f;
                expectWithinAbsoluteError (gainThrough<ReverbEngine> (1000.0, dryMix), 0.0f, 0.01f, "mix 0 = dry");
            }

            beginTest ("C-Verb: freeze holds the tail");
            {
                auto render = [] (bool freeze)
                {
                    Buffer burst (2, (int) (5.0 * rate));
                    burst.clear();
                    const auto tone = makeSine (440.0, 0.25f, 1.0);
                    for (int ch = 0; ch < 2; ++ch)
                        burst.copyFrom (ch, 0, tone, ch, 0, tone.getNumSamples());

                    auto s = plainReverb();
                    s.decay = 0.8f;
                    auto held = s;
                    held.freeze = freeze;
                    return rmsDb (through<ReverbEngine> (burst, s, held, 0.96), 3.5, 4.5);
                };

                auto normal = render (false), frozen = render (true);
                logMessage ("Level 3 s after the sound stops: " + juce::String (normal, 1) + " dB normally, " + juce::String (frozen, 1) + " dB frozen");
                expectGreaterThan (frozen, normal + 40.0f);
                expectGreaterThan (frozen, -30.0f);
            }
        }
    };

    static EffectEngineTests effectEngineTests;
}
