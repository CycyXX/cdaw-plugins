#include "TestSupport.h"
#include "Shared/Lead/LeadPresets.h"
#include <complex>

namespace cdaw::test
{
    /** C-Lead: pitch, glide, legato and note priority, vibrato and bends, band-limited (and
        hard-synced) oscillators, the ladder filter and drive, voices, presets. */
    struct LeadTests : public juce::UnitTest
    {
        LeadTests() : juce::UnitTest ("C-Lead engine", "Plugins") {}

        using P = LeadParameters::ID;
        using Settings = LeadEngine::Settings;
        using Wave = LeadEngine::Wave;
        static constexpr double rate = 48000.0;

        struct Note { int key; double on, off; int velocity = 100; };

        /** A plain patch to measure with: one sine, an open 12 dB low pass, no drive, no movement,
            no glide or vibrato, no effects, a flat envelope. */
        static Settings plain()
        {
            auto values = LeadPresets::valuesOf ({});
            auto set = [&values] (int id, float v) { values[(size_t) id] = v; };
            set (P::aWave, (float) Wave::sine);
            set (P::aUnison, 1.0f);
            set (P::bOn, 0.0f);
            set (P::drive, 0.0f);
            set (P::filterType, (float) LeadEngine::FilterType::lowPass);
            set (P::cutoff, 20000.0f);
            set (P::resonance, 0.0f);
            set (P::keyTrack, 0.0f);
            set (P::filterEnv, 0.0f);
            set (P::filterVelocity, 0.0f);
            set (P::ampAttack, 0.001f);
            set (P::ampDecay, 0.001f);
            set (P::ampSustain, 1.0f);
            set (P::ampRelease, 0.05f);
            set (P::ampVelocity, 0.0f);
            set (P::vibratoDepth, 0.0f);
            set (P::glide, 0.0f);
            set (P::chorusOn, 0.0f);
            set (P::delayOn, 0.0f);
            set (P::reverbOn, 0.0f);
            set (P::volume, 0.0f);
            for (int k = 0; k < LeadEngine::numSlots; ++k)
                set (LeadParameters::slot (k, 0), 0.0f);
            return LeadParameters::toSettings ([&values] (int id) { return values[(size_t) id]; });
        }

        /** Plays notes through an engine, block by block; `each` runs after every block. */
        static juce::AudioBuffer<float> play (LeadEngine& engine, const Settings& s, const std::vector<Note>& notes, double seconds,
                                              std::function<void (int block, juce::MidiBuffer&)> extra = {},
                                              std::function<void (int block)> each = {}, int blockSize = 512)
        {
            engine.prepare (rate, blockSize);
            const int total = (int) (seconds * rate);
            juce::AudioBuffer<float> out (2, total);
            out.clear();
            juce::MidiBuffer midi;

            for (int start = 0, block = 0; start < total; start += blockSize, ++block)
            {
                const int n = juce::jmin (blockSize, total - start);
                midi.clear();
                for (auto& note : notes)
                {
                    const int on = (int) (note.on * rate), off = (int) (note.off * rate);
                    if (off >= start && off < start + n)
                        midi.addEvent (juce::MidiMessage::noteOff (1, note.key), off - start);
                    if (on >= start && on < start + n)
                        midi.addEvent (juce::MidiMessage::noteOn (1, note.key, (juce::uint8) note.velocity), on - start);
                }

                if (extra)
                    extra (block, midi);

                engine.process (out, start, n, midi, s, 120.0);

                if (each)
                    each (block);
            }

            return out;
        }

        /** The frequency between two times, from upward zero crossings (interpolated). */
        static double frequency (const juce::AudioBuffer<float>& b, double from, double to, int channel = 0)
        {
            const auto* d = b.getReadPointer (channel);
            double first = -1.0, last = -1.0;
            int crossings = 0;

            for (int i = (int) (from * rate); i < (int) (to * rate); ++i)
                if (d[i - 1] < 0.0f && d[i] >= 0.0f)
                {
                    const double at = i - 1 + d[i - 1] / (d[i - 1] - d[i]);
                    if (first < 0.0) first = at;
                    else             ++crossings;
                    last = at;
                }

            return crossings > 0 ? crossings * rate / (last - first) : 0.0;
        }

        static double noteHz (double note)      { return 440.0 * std::pow (2.0, (note - 69.0) / 12.0); }

        static float peak (const juce::AudioBuffer<float>& b, double from, double to)
        {
            return b.getMagnitude (0, (int) (from * rate), (int) ((to - from) * rate));
        }

        static float rmsDb (const juce::AudioBuffer<float>& b, double from, double to, int channel = 0)
        {
            const int s = (int) (from * rate), e = juce::jmin (b.getNumSamples(), (int) (to * rate));
            return juce::Decibels::gainToDecibels (b.getRMSLevel (channel, s, e - s), -150.0f);
        }

        /** The level (dB) of a frequency in a stretch of audio (Goertzel, Hann window). */
        static double toneDb (const juce::AudioBuffer<float>& b, double hz, double from, double to)
        {
            const int s = (int) (from * rate), n = (int) ((to - from) * rate);
            const double w = juce::MathConstants<double>::twoPi * hz / rate;
            double re = 0.0, im = 0.0;
            for (int i = 0; i < n; ++i)
            {
                const double window = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * i / (n - 1));
                const double x = b.getSample (0, s + i) * window;
                re += x * std::cos (w * i);
                im -= x * std::sin (w * i);
            }
            return 20.0 * std::log10 (juce::jmax (1.0e-12, std::sqrt (re * re + im * im) / (n * 0.25)));
        }

        /** Everything below `maxHz` that isn't a harmonic of f0, against the harmonics (dB): aliasing. */
        static double offHarmonicDb (const juce::AudioBuffer<float>& b, double f0, double from, double maxHz = 24000.0)
        {
            constexpr int order = 14, size = 1 << order;
            juce::dsp::FFT fft (order);
            juce::dsp::WindowingFunction<float> window ((size_t) size, juce::dsp::WindowingFunction<float>::blackmanHarris, false);
            std::vector<float> data ((size_t) size * 2, 0.0f);
            for (int i = 0; i < size; ++i)
                data[(size_t) i] = b.getSample (0, (int) (from * rate) + i);
            window.multiplyWithWindowingTable (data.data(), (size_t) size);
            fft.performFrequencyOnlyForwardTransform (data.data());

            double harmonic = 0.0, other = 0.0;
            for (int k = 2; k < size / 2 && k * rate / size < maxHz; ++k)
            {
                const double hz = k * rate / size;
                const double nearest = std::round (hz / f0) * f0;
                const double power = (double) data[(size_t) k] * data[(size_t) k];
                (std::abs (hz - nearest) < 4.0 * rate / size ? harmonic : other) += power;
            }
            return 10.0 * std::log10 (other / harmonic);
        }

        /** The level of harmonic `h` relative to the fundamental for a sine of `amplitude` through a drive curve. */
        static double curveHarmonicDb (LeadEngine::DriveType type, float drive, float amplitude, int h)
        {
            constexpr int n = 4096;
            std::complex<double> first, wanted;
            for (int i = 0; i < n; ++i)
            {
                const double phase = juce::MathConstants<double>::twoPi * i / n;
                const double y = LeadEngine::driveCurve (type, drive, amplitude * (float) std::sin (phase));
                first += y * std::polar (1.0, -phase);
                wanted += y * std::polar (1.0, -phase * h);
            }
            return 20.0 * std::log10 (std::abs (wanted) / std::abs (first));
        }

        static bool finite (const juce::AudioBuffer<float>& b)
        {
            for (int ch = 0; ch < b.getNumChannels(); ++ch)
                for (int i = 0; i < b.getNumSamples(); ++i)
                    if (! std::isfinite (b.getSample (ch, i)))
                        return false;
            return true;
        }

        void runTest() override
        {
            beginTest ("A sine plays at the note's pitch and level; octave and semitone shifts");
            {
                LeadEngine engine;
                auto s = plain();
                auto out = play (engine, s, { { 69, 0.0, 1.0 } }, 1.2);
                const auto hz = frequency (out, 0.2, 0.9);
                logMessage ("A4: " + juce::String (hz, 3) + " Hz, peak " + juce::String (peak (out, 0.2, 0.5), 3));
                expectWithinAbsoluteError (hz, 440.0, 0.05);
                expectWithinAbsoluteError (peak (out, 0.2, 0.5), 0.4f, 0.005f, "0.8 level through the voice headroom");
                expectWithinAbsoluteError (rmsDb (out, 1.1, 1.2), -150.0f, 1.0f, "released and silent");

                s.osc[0].octave = 1;
                s.osc[0].semitones = 7;
                expectWithinAbsoluteError (frequency (play (engine, s, { { 69, 0.0, 1.0 } }, 1.0), 0.2, 0.9), noteHz (69 + 19), 0.2);
            }

            beginTest ("Glide: a straight line in pitch over its time; 'legato only' glides only between held keys");
            {
                LeadEngine engine;
                auto s = plain();
                s.glide = 0.2f;
                s.glideMode = LeadEngine::GlideMode::always;
                auto out = play (engine, s, { { 57, 0.0, 0.45 }, { 69, 0.5, 1.0 } }, 1.0);

                for (double t : { 0.55, 0.6, 0.65 })
                {
                    const auto hz = frequency (out, t - 0.006, t + 0.006);
                    const auto expected = noteHz (57.0 + 12.0 * (t - 0.5) / 0.2);
                    logMessage ("Gliding, " + juce::String (t, 2) + " s: " + juce::String (hz, 1) + " Hz (a straight line says " + juce::String (expected, 1) + ")");
                    expectWithinAbsoluteError (hz, expected, expected * 0.01);
                }
                expectWithinAbsoluteError (frequency (out, 0.75, 0.95), 440.0, 0.1, "arrived");

                s.glideMode = LeadEngine::GlideMode::legatoOnly;
                auto detached = play (engine, s, { { 57, 0.0, 0.45 }, { 69, 0.5, 1.0 } }, 1.0);
                expectWithinAbsoluteError (frequency (detached, 0.505, 0.55), 440.0, 0.2, "detached notes jump");
                auto joined = play (engine, s, { { 57, 0.0, 0.6 }, { 69, 0.5, 1.0 } }, 1.0);
                const auto during = frequency (joined, 0.594, 0.606);
                expectWithinAbsoluteError (during, noteHz (63.0), 3.0, "overlapping notes glide");
            }

            beginTest ("Legato moves the voice without a new attack and falls back to a key still held; mono retriggers");
            {
                LeadEngine engine;
                auto s = plain();
                s.ampEnv = { 0.001f, 0.1f, 0.5f, 0.05f };
                s.mode = LeadEngine::VoiceMode::legato;
                int most = 0;
                auto legato = play (engine, s, { { 57, 0.0, 1.2 }, { 64, 0.5, 0.8 } }, 1.4, {},
                                    [&] (int) { most = juce::jmax (most, engine.activeVoices.load()); });

                const auto sustain = peak (legato, 0.3, 0.45);
                logMessage ("Legato: sustain " + juce::String (sustain, 3) + ", at the second key " + juce::String (peak (legato, 0.5, 0.55), 3)
                            + ", back on the first " + juce::String (peak (legato, 0.85, 1.0), 3));
                expectEquals (most, 1, "one voice");
                expectWithinAbsoluteError (sustain, 0.2f, 0.005f);
                expectWithinAbsoluteError (peak (legato, 0.5, 0.55), sustain, 0.005f, "no new attack");
                expectWithinAbsoluteError (frequency (legato, 0.55, 0.78), noteHz (64), 0.2);
                expectWithinAbsoluteError (frequency (legato, 0.85, 1.15), 220.0, 0.1, "back to the key still held");
                expectWithinAbsoluteError (peak (legato, 0.85, 1.15), sustain, 0.005f, "without a release or attack");

                s.mode = LeadEngine::VoiceMode::mono;
                auto mono = play (engine, s, { { 57, 0.0, 1.2 }, { 64, 0.5, 0.8 } }, 1.4);
                logMessage ("Mono: at the second key " + juce::String (peak (mono, 0.5, 0.52), 3));
                expectGreaterThan (peak (mono, 0.5, 0.52), 0.38f, "a new attack");
                expectWithinAbsoluteError (frequency (mono, 0.85, 1.15), 220.0, 0.1, "mono returns to the held key too");
            }

            beginTest ("Note priority: last, high, low");
            {
                LeadEngine engine;
                auto s = plain();
                const std::vector<Note> up { { 60, 0.0, 1.0 }, { 55, 0.3, 1.0 }, { 67, 0.6, 1.0 } };

                auto last = play (engine, s, up, 1.0);
                expectWithinAbsoluteError (frequency (last, 0.35, 0.58), noteHz (55), 0.1, "last: the newest key");
                expectWithinAbsoluteError (frequency (last, 0.65, 0.95), noteHz (67), 0.1);

                s.priority = LeadEngine::Priority::high;
                auto high = play (engine, s, up, 1.0);
                expectWithinAbsoluteError (frequency (high, 0.35, 0.58), noteHz (60), 0.1, "high: a lower key changes nothing");
                expectWithinAbsoluteError (frequency (high, 0.65, 0.95), noteHz (67), 0.1, "high: a higher key takes over");

                s.priority = LeadEngine::Priority::low;
                auto low = play (engine, s, up, 1.0);
                expectWithinAbsoluteError (frequency (low, 0.35, 0.58), noteHz (55), 0.1, "low: a lower key takes over");
                expectWithinAbsoluteError (frequency (low, 0.65, 0.95), noteHz (55), 0.1, "low: a higher key changes nothing");
            }

            beginTest ("Vibrato waits for its delay, then swings its depth; the mod wheel and pitch bend");
            {
                LeadEngine engine;
                auto s = plain();
                s.vibratoDepth = 50.0f;
                s.vibratoRate = 5.0f;
                s.vibratoDelay = 0.5f;
                s.vibratoFade = 0.0f;
                auto out = play (engine, s, { { 69, 0.0, 2.0 } }, 2.0);

                auto range = [&] (const juce::AudioBuffer<float>& b, double from, double to)
                {
                    double lowest = 1.0e9, highest = 0.0;
                    for (double t = from; t + 0.02 < to; t += 0.005)
                    {
                        const auto hz = frequency (b, t, t + 0.02);
                        lowest = juce::jmin (lowest, hz);
                        highest = juce::jmax (highest, hz);
                    }
                    return std::pair (lowest, highest);
                };

                // Over 20 ms a 5 Hz swing reads 1.6 % shallower.
                const auto expected = std::pow (2.0, 2.0 * 50.0 * 0.9836 / 1200.0);
                auto [stillLow, stillHigh] = range (out, 0.05, 0.48);
                auto [low, high] = range (out, 0.7, 1.9);
                logMessage ("Before the delay " + juce::String (stillLow, 2) + " to " + juce::String (stillHigh, 2) + " Hz; after "
                            + juce::String (low, 1) + " to " + juce::String (high, 1) + " Hz");
                expectLessThan (stillHigh - stillLow, 0.1, "steady before the delay");
                expectWithinAbsoluteError (high / low, expected, 0.004, "a 50 cent swing either way");

                auto wheel = plain();
                wheel.vibratoRate = 5.0f;
                wheel.vibratoWheel = 50.0f;
                auto moved = play (engine, wheel, { { 69, 0.0, 2.0 } }, 2.0,
                                   [] (int block, juce::MidiBuffer& midi) { if (block == 0) midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, 127), 0); });
                auto [wheelLow, wheelHigh] = range (moved, 0.3, 1.9);
                expectWithinAbsoluteError (wheelHigh / wheelLow, expected, 0.004, "the mod wheel adds its depth");

                auto bend = plain();
                bend.bendDown = 12.0f;
                auto bent = play (engine, bend, { { 69, 0.0, 2.0 } }, 2.0, [] (int block, juce::MidiBuffer& midi)
                {
                    if (block == 0)                         midi.addEvent (juce::MidiMessage::pitchWheel (1, 16383), 0);
                    if (block == (int) (1.0 * rate) / 512)  midi.addEvent (juce::MidiMessage::pitchWheel (1, 0), 0);
                });
                expectWithinAbsoluteError (frequency (bent, 0.2, 0.9), noteHz (69.0 + 2.0 * 8191.0 / 8192.0), 0.1, "two semitones up");
                expectWithinAbsoluteError (frequency (bent, 1.2, 1.9), 220.0, 0.1, "an octave down");
            }

            beginTest ("Oscillators are band-limited: a high saw and a hard-synced saw don't alias");
            {
                LeadEngine engine;
                auto s = plain();
                s.osc[0].wave = Wave::saw;
                auto high = play (engine, s, { { 100, 0.0, 1.0 } }, 1.0);      // E7, 2637 Hz
                auto c6 = play (engine, s, { { 84, 0.0, 1.0 } }, 1.0);

                auto synced = plain();
                synced.osc[0].wave = Wave::saw;
                synced.osc[0].level = 0.0f;
                synced.osc[1] = synced.osc[0];
                synced.osc[1].on = true;
                synced.osc[1].semitones = 7;
                synced.osc[1].fine = 30.0f;
                synced.osc[1].level = 0.8f;
                synced.sync = true;
                auto sync = play (engine, synced, { { 76, 0.0, 1.0 } }, 1.0);    // E5 with B 7.3 semitones up, restarted with A

                auto pulse = plain();
                pulse.osc[0].wave = Wave::pulse;
                pulse.osc[0].pulseWidth = 0.2f;
                auto narrow = play (engine, pulse, { { 96, 0.0, 1.0 } }, 1.0);  // C7

                // Up to 20 kHz: the half-band filter lets a little of 24-28 kHz fold into 20-24 kHz,
                // above hearing; what lands lower is the oscillators' own aliasing.
                const auto sawDb = offHarmonicDb (high, noteHz (100), 0.3, 20000.0);
                const auto c6Db = offHarmonicDb (c6, noteHz (84), 0.3, 20000.0);
                const auto syncDb = offHarmonicDb (sync, noteHz (76), 0.3, 20000.0);
                const auto pulseDb = offHarmonicDb (narrow, noteHz (96), 0.3, 20000.0);
                logMessage ("Off the harmonics below 20 kHz: C6 saw " + juce::String (c6Db, 1) + " dB, E7 saw " + juce::String (sawDb, 1) + " dB, synced saw on E5 " + juce::String (syncDb, 1)
                            + " dB, C7 pulse at 20 % " + juce::String (pulseDb, 1) + " dB (to 24 kHz: " + juce::String (offHarmonicDb (high, noteHz (100), 0.3), 1)
                            + " / " + juce::String (offHarmonicDb (sync, noteHz (76), 0.3), 1) + " / " + juce::String (offHarmonicDb (narrow, noteHz (96), 0.3), 1) + " dB)");
                expectLessThan (c6Db, -57.0);
                expectLessThan (sawDb, -52.0, "even near the top of the keyboard");
                expectLessThan (syncDb, -57.0, "the synced wave repeats with A and stays clean");
                expectLessThan (pulseDb, -52.0);
                expect (finite (sync));
            }

            beginTest ("Ring modulation makes sum and difference tones; FM adds sidebands");
            {
                LeadEngine engine;
                auto s = plain();
                s.osc[0].level = 0.0f;
                s.osc[1] = s.osc[0];
                s.osc[1].on = true;
                s.osc[1].semitones = 7;
                s.osc[1].level = 0.0f;
                s.ring = 1.0f;
                auto ring = play (engine, s, { { 57, 0.0, 1.0 } }, 1.0);
                const double a = noteHz (57), b = noteHz (64);
                logMessage ("Ring: " + juce::String (a, 0) + " Hz " + juce::String (toneDb (ring, a, 0.3, 0.9), 1) + " dB, "
                            + juce::String (b - a, 0) + " Hz " + juce::String (toneDb (ring, b - a, 0.3, 0.9), 1) + " dB, "
                            + juce::String (b + a, 0) + " Hz " + juce::String (toneDb (ring, b + a, 0.3, 0.9), 1) + " dB");
                expectGreaterThan (toneDb (ring, b - a, 0.3, 0.9), toneDb (ring, a, 0.3, 0.9) + 40.0);
                expectGreaterThan (toneDb (ring, b + a, 0.3, 0.9), toneDb (ring, b, 0.3, 0.9) + 40.0);

                auto fm = plain();
                fm.osc[0].level = 0.0f;
                fm.osc[1] = fm.osc[0];
                fm.osc[1].on = true;
                fm.osc[1].level = 0.8f;
                auto clean = toneDb (play (engine, fm, { { 57, 0.0, 1.0 } }, 1.0), 2.0 * a, 0.3, 0.9);
                fm.fm = 0.3f;
                auto modulated = toneDb (play (engine, fm, { { 57, 0.0, 1.0 } }, 1.0), 2.0 * a, 0.3, 0.9);
                logMessage ("FM: the second harmonic " + juce::String (clean, 1) + " dB without, " + juce::String (modulated, 1) + " dB at 30 %");
                expectGreaterThan (modulated, clean + 40.0);
            }

            beginTest ("The ladder cuts as its curve says, its resonance peaks, and it stays stable when pushed");
            {
                LeadEngine engine;
                auto s = plain();
                s.osc[0].wave = Wave::saw;
                s.osc[0].level = 0.05f;         // quiet, so the ladder's saturation stays out of it
                s.filterType = LeadEngine::FilterType::ladder;
                s.resonance = 0.1f;
                const double f0 = noteHz (36);
                auto open = play (engine, s, { { 36, 0.0, 1.0 } }, 1.0);
                s.cutoff = 800.0f;
                auto cut = play (engine, s, { { 36, 0.0, 1.0 } }, 1.0);

                for (int h : { 2, 6, 12, 20, 30 })
                {
                    const auto measured = toneDb (cut, f0 * h, 0.3, 0.9) - toneDb (open, f0 * h, 0.3, 0.9);
                    const auto drawn = LeadEngine::getFilterResponseDb (s, 800.0, f0 * h, rate) - LeadEngine::getFilterResponseDb (s, 20000.0, f0 * h, rate);
                    logMessage ("Harmonic " + juce::String (h) + " (" + juce::String (f0 * h, 0) + " Hz) through an 800 Hz ladder: "
                                + juce::String (measured, 2) + " dB (drawn " + juce::String (drawn, 2) + ")");
                    expectWithinAbsoluteError (measured, drawn, 0.5);
                }

                s.resonance = 0.75f;
                auto peaked = play (engine, s, { { 36, 0.0, 1.0 } }, 1.0);
                const int nearest = juce::roundToInt (800.0 / f0);
                const auto gain = toneDb (peaked, f0 * nearest, 0.3, 0.9) - toneDb (open, f0 * nearest, 0.3, 0.9);
                const auto drawnGain = LeadEngine::getFilterResponseDb (s, 800.0, f0 * nearest, rate)
                                     - LeadEngine::getFilterResponseDb ({ .resonance = 0.1f }, 20000.0, f0 * nearest, rate);
                logMessage ("At 75 % resonance, harmonic " + juce::String (nearest) + ": " + juce::String (gain, 1) + " dB (drawn " + juce::String (drawnGain, 1) + ")");
                expectWithinAbsoluteError (gain, drawnGain, 1.0);
                expectGreaterThan (gain, 6.0, "a resonant peak");

                // Everything at full: self-oscillating, driven, swept by the envelope.
                auto pushed = plain();
                pushed.osc[0].wave = Wave::saw;
                pushed.osc[0].level = 1.0f;
                pushed.subLevel = 1.0f;
                pushed.filterType = LeadEngine::FilterType::ladder;
                pushed.cutoff = 200.0f;
                pushed.resonance = 1.0f;
                pushed.drive = 1.0f;
                pushed.filterEnv = 1.0f;
                pushed.modEnv = { 0.001f, 0.3f, 0.0f, 0.2f };
                auto wild = play (engine, pushed, { { 30, 0.0, 0.8 }, { 42, 0.5, 1.5 }, { 90, 1.0, 2.0 } }, 2.5);
                logMessage ("Pushed ladder: peak " + juce::String (wild.getMagnitude (0, wild.getNumSamples()), 3));
                expect (finite (wild));
                expectLessThan (wild.getMagnitude (0, wild.getNumSamples()), 1.0f);
                expectGreaterThan (rmsDb (wild, 0.1, 0.4), -40.0f, "and still sounding");
            }

            beginTest ("Drive shapes the sound as its curve says");
            {
                LeadEngine engine;
                auto s = plain();
                const double f0 = 440.0;
                s.drive = 0.5f;
                auto soft = play (engine, s, { { 69, 0.0, 1.0 } }, 1.0);
                const auto softH3 = toneDb (soft, 3.0 * f0, 0.3, 0.9) - toneDb (soft, f0, 0.3, 0.9);
                const auto curveH3 = curveHarmonicDb (LeadEngine::DriveType::soft, 0.5f, 0.8f, 3);
                const auto softH2 = toneDb (soft, 2.0 * f0, 0.3, 0.9) - toneDb (soft, f0, 0.3, 0.9);

                s.driveType = LeadEngine::DriveType::tube;
                auto tube = play (engine, s, { { 69, 0.0, 1.0 } }, 1.0);
                const auto tubeH2 = toneDb (tube, 2.0 * f0, 0.3, 0.9) - toneDb (tube, f0, 0.3, 0.9);
                const auto curveH2 = curveHarmonicDb (LeadEngine::DriveType::tube, 0.5f, 0.8f, 2);

                logMessage ("Soft drive at 50 %: 3rd harmonic " + juce::String (softH3, 2) + " dB (curve " + juce::String (curveH3, 2) + "), 2nd "
                            + juce::String (softH2, 1) + " dB; tube: 2nd " + juce::String (tubeH2, 2) + " dB (curve " + juce::String (curveH2, 2) + ")");
                expectWithinAbsoluteError (softH3, curveH3, 0.3);
                expectLessThan (softH2, -80.0, "soft is symmetric: no even harmonics");
                expectWithinAbsoluteError (tubeH2, curveH2, 0.3);
                expectGreaterThan (tubeH2, -30.0, "tube's uneven halves make even harmonics");

                s.driveType = LeadEngine::DriveType::fold;
                s.drive = 1.0f;
                auto folded = play (engine, s, { { 69, 0.0, 1.0 } }, 1.0);
                expect (finite (folded));
                expectLessThan (folded.getMagnitude (0, folded.getNumSamples()), 0.51f, "folding stays within the voice's headroom");
            }

            beginTest ("The built-in distortion: C-Dist's harmonics, at about the same level, its latency only while on");
            {
                LeadEngine engine;
                auto s = plain();
                auto clean = play (engine, s, { { 57, 0.0, 1.0 } }, 1.2);
                s.distOn = true;
                s.distVoicing = 1;      // Lead
                s.distDrive = 0.7f;
                auto driven = play (engine, s, { { 57, 0.0, 1.0 } }, 1.2);

                const auto addedH3 = (toneDb (driven, 660.0, 0.3, 0.9) - toneDb (driven, 220.0, 0.3, 0.9))
                                   - (toneDb (clean, 660.0, 0.3, 0.9) - toneDb (clean, 220.0, 0.3, 0.9));
                const auto levelChange = rmsDb (driven, 0.3, 0.9) - rmsDb (clean, 0.3, 0.9);
                auto onset = [] (const juce::AudioBuffer<float>& b)
                {
                    for (int i = 0; i < b.getNumSamples(); ++i)
                        if (std::abs (b.getSample (0, i)) > 1.0e-3f)
                            return i;
                    return -1;
                };
                logMessage ("Distortion on a sine: 3rd harmonic +" + juce::String (addedH3, 1) + " dB, level " + juce::String (levelChange, 1)
                            + " dB, starting " + juce::String (onset (driven) - onset (clean)) + " samples later");
                expect (finite (driven));
                expectGreaterThan (addedH3, 30.0, "it distorts");
                expectWithinAbsoluteError (levelChange, 0.0f, 6.0f, "the drive changes the sound more than the level");
                expectWithinAbsoluteError (onset (driven) - onset (clean), DistEngine::latencySamples, 3, "C-Dist's latency");

                for (int v = 0; v < 4; ++v)
                {
                    s.distVoicing = v;
                    s.distDrive = 1.0f;
                    s.distTone = 1.0f;
                    expect (finite (play (engine, s, { { 45, 0.0, 0.5 }, { 81, 0.3, 0.9 } }, 1.0)), LeadParameters::getDistVoicingNames()[v] + " stays finite");
                }
            }

            beginTest ("Poly: voices up to the polyphony, the oldest stolen; a unison spreads in stereo");
            {
                LeadEngine engine;
                auto s = plain();
                s.mode = LeadEngine::VoiceMode::poly;
                s.polyphony = 4;
                std::vector<Note> cluster;
                for (int i = 0; i < 6; ++i)
                    cluster.push_back ({ 60 + i * 2, 0.02 * i, 1.0 });

                int most = 0;
                bool newestPlaying = false, oldestGone = true;
                auto out = play (engine, s, cluster, 1.2, {}, [&] (int block)
                {
                    most = juce::jmax (most, engine.activeVoices.load());
                    if (block == (int) (0.5 * rate) / 512)
                        for (auto& v : engine.voiceViews)
                        {
                            newestPlaying = newestPlaying || v.note.load() == 70;
                            oldestGone = oldestGone && v.note.load() != 60;
                        }
                });
                expectEquals (most, 4);
                expect (newestPlaying && oldestGone, "the last note took the first one's voice");
                expect (finite (out));

                auto spread = plain();
                spread.osc[0].wave = Wave::saw;
                spread.osc[0].unison = 7;
                spread.osc[0].detune = 0.5f;
                spread.osc[0].spread = 1.0f;
                auto wide = play (engine, spread, { { 60, 0.0, 1.0 } }, 1.0);
                double side = 0.0, mid = 0.0;
                for (int i = (int) (0.2 * rate); i < (int) (0.9 * rate); ++i)
                {
                    const double l = wide.getSample (0, i), r = wide.getSample (1, i);
                    side += (l - r) * (l - r);
                    mid += (l + r) * (l + r);
                }
                logMessage ("Seven-voice supersaw: side / mid " + juce::String (std::sqrt (side / mid), 3));
                expectGreaterThan (std::sqrt (side / mid), 0.3);
            }

            beginTest ("The UI keyboard: a held mouse key plays, moves and releases like a MIDI note");
            {
                LeadEngine engine;
                auto s = plain();

                // Press A4 after the first block, drag to A5 at 0.5 s, let go at 1 s.
                auto out = play (engine, s, {}, 1.4, {}, [&engine] (int block)
                {
                    if (block == 0)     engine.mouseKey = 69;
                    if (block == 47)    engine.mouseKey = 81;
                    if (block == 94)    engine.mouseKey = -1;
                });

                expectWithinAbsoluteError (frequency (out, 0.1, 0.45), 440.0, 0.1);
                expectWithinAbsoluteError (frequency (out, 0.6, 0.95), 880.0, 0.2);
                expectLessThan (rmsDb (out, 1.3, 1.39), -100.0f, "released");
                expectEquals (engine.activeVoices.load(), 0);

                // A key held across a reset sounds again.
                juce::AudioBuffer<float> buffer (2, 512);
                juce::MidiBuffer midi;
                engine.mouseKey = 60;
                engine.process (buffer, 0, 512, midi, s, 120.0);
                expectEquals (engine.activeVoices.load(), 1);
                engine.reset();
                engine.process (buffer, 0, 512, midi, s, 120.0);
                expectEquals (engine.activeVoices.load(), 1);
            }

            beginTest ("Renders repeat exactly, whatever the block size");
            {
                LeadEngine a, b;
                auto s = LeadParameters::toSettings ([] (int id) { return LeadParameters::get (id).defaultValue; });
                const std::vector<Note> line { { 64, 0.0, 0.4 }, { 67, 0.3, 0.7 }, { 71, 0.6, 1.4 }, { 69, 1.5, 2.2 } };
                auto first = play (a, s, line, 3.0, {}, {}, 512);
                auto second = play (b, s, line, 3.0, {}, {}, 160);

                double signal = 0.0, difference = 0.0;
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < first.getNumSamples(); ++i)
                    {
                        signal += juce::square (first.getSample (ch, i));
                        difference += juce::square (first.getSample (ch, i) - second.getSample (ch, i));
                    }
                const auto db = 10.0 * std::log10 (juce::jmax (1.0e-30, difference) / signal);
                logMessage ("The default lead in blocks of 512 and 160: difference " + juce::String (db, 1) + " dB");

                expectLessThan (db, -100.0);
                expect (finite (first));
            }

            beginTest ("Every factory preset loads and plays a phrase within range");
            {
                // A legato line, then a chord (poly presets play it all, mono ones its last key).
                const std::vector<Note> phrase { { 64, 0.0, 0.3, 100 }, { 67, 0.25, 0.55, 90 }, { 71, 0.5, 0.8, 110 }, { 72, 0.75, 1.5, 100 },
                                                 { 60, 1.6, 2.4, 110 }, { 64, 1.6, 2.4, 100 }, { 67, 1.6, 2.4, 100 } };
                juce::StringArray names;

                for (auto& preset : LeadPresets::factory())
                {
                    expect (! names.contains (preset.name), preset.name + " twice");
                    names.add (preset.name);
                    expectEquals (preset.macroNames.size(), LeadEngine::numMacros);

                    const auto values = LeadPresets::valuesOf (preset);
                    auto s = LeadParameters::toSettings ([&values] (int id) { return values[(size_t) id]; });
                    LeadEngine engine;
                    auto out = play (engine, s, phrase, 4.0);
                    const auto top = juce::jmax (out.getMagnitude (0, 0, out.getNumSamples()), out.getMagnitude (1, 0, out.getNumSamples()));
                    const auto level = juce::jmax (rmsDb (out, 0.0, 3.0, 0), rmsDb (out, 0.0, 3.0, 1));
                    logMessage (preset.category + " / " + preset.name + ": peak " + juce::String (juce::Decibels::gainToDecibels (top), 1)
                                + " dB, level " + juce::String (level, 1) + " dB");
                    expect (finite (out), preset.name + " stays finite");
                    expectLessThan (top, 1.0f, preset.name + " stays below full scale");
                    expectGreaterThan (level, -45.0f, preset.name + " is audible");
                    expectWithinAbsoluteError (juce::Decibels::gainToDecibels (top), -6.0f, 1.5f, preset.name + " is level-matched");
                }

                expectGreaterThan ((int) LeadPresets::factory().size(), 18);
            }

            beginTest ("User presets save and load");
            {
                auto folder = getTestDir().getChildFile ("lead_presets");
                folder.deleteRecursively();
                LeadPresets::setUserFolder (folder);

                auto values = LeadPresets::valuesOf (LeadPresets::factory()[3]);
                values[(size_t) P::cutoff] = 1234.0f;
                const juce::StringArray macros { "One", "Two", "Three", "Four" };
                expect (LeadPresets::saveUser ("My Lead", [&values] (int id) { return values[(size_t) id]; }, macros).wasOk());

                auto loaded = LeadPresets::loadUser();
                expectEquals ((int) loaded.size(), 1);
                if (! loaded.empty())
                {
                    expectEquals (loaded[0].name, juce::String ("My Lead"));
                    expect (loaded[0].isUser());
                    expectEquals (loaded[0].macroNames[2], juce::String ("Three"));
                    auto back = LeadPresets::valuesOf (loaded[0]);
                    for (int i = 0; i < P::count; ++i)
                        expectWithinAbsoluteError (back[(size_t) i], values[(size_t) i], 1.0e-4f, LeadParameters::get (i).id);
                }

                expect (LeadPresets::saveUser ("  ", [&values] (int id) { return values[(size_t) id]; }, macros).failed(), "a preset needs a name");
                expectEquals ((int) LeadPresets::everything().size(), (int) LeadPresets::factory().size() + 1);
                LeadPresets::setUserFolder ({});
            }
        }
    };

    static LeadTests leadTests;
}
