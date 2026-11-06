#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "AnalyserFifo.h"
#include "EqMath.h"
#include "PeakMeter.h"
#include <memory>
#include <vector>

namespace cdaw
{
    /** The reverb DSP shared by C-Verb and the synths' reverb: pre-delay -> early reflections
        (which also feed the tail) -> input diffusion -> a 16-line feedback delay network (Hadamard
        mixing, an allpass inside every line, randomly and slowly modulated delays) with separate
        decay times for lows, mids and highs. Low / high cuts on the signal going into the reverb
        ("pre") and on the reverb coming out ("post"), stereo width, ducking, freeze and an
        equal-power dry / wet mix.

        Types voice the network differently and add their own stage: Hall, Room and Plate are
        spaces; Shimmer pitch-shifts part of the tail back into it; Reverse plays the tail
        backwards in windows; Bloom swells in; Gated cuts a dense burst off; Cloud scatters the
        tail into grains. Character and Variation are two knobs whose meaning depends on the
        type (see getKnobNames). */
    class ReverbEngine
    {
    public:
        enum class Type { hall, room, plate, shimmer, reverse, bloom, gated, cloud };
        static constexpr int numTypes = 8;

        /** Everything the sound depends on, in plain units. */
        struct Settings
        {
            float mix = 0.3f;               // 0..1
            float preDelay = 20.0f;         // ms
            float size = 0.55f;             // 0..1
            float decay = 2.2f;             // RT60, s
            float diffusion = 0.75f;        // 0..1
            float modulation = 0.3f;        // 0..1
            float lowDecay = 1.1f;          // decay multiplier below 250 Hz
            float highDecay = 0.5f;         // decay multiplier above dampFreq
            float dampFreq = 5000.0f;       // Hz
            float early = 0.4f;             // early reflections level, 0..1
            float width = 1.0f;             // 0..2
            float preLowCut = 20.0f, preHighCut = 20000.0f, postLowCut = 100.0f, postHighCut = 12000.0f;   // Hz; 20 / 20k = off
            bool freeze = false;

            Type type = Type::hall;
            float character = 0.5f;         // 0..1, the type's first knob
            float variation = 0.5f;         // 0..1, the type's second knob
            float pitch = 12.0f;            // semitones: Shimmer's interval, Cloud's transposed grains
            float duck = 0.0f;              // 0..1: how far the reverb is turned down while the input plays
        };

        void prepare (double sampleRate, int maxBlockSize, const Settings&);
        void reset();

        /** Takes the size and mix at once instead of gliding to them, so a reverb switched on
            with a sound starts the same way whatever the host's block size. */
        void jumpTo (const Settings&);

        /** Processes `n` samples from `start` in place (stereo, or mono in / mono out). */
        void process (juce::AudioBuffer<float>&, int start, int n, const Settings&);

        double getSampleRate() const        { return rate; }

        //==============================================================================
        static juce::StringArray getTypeNames();

        /** What Character and Variation do in a type (knob labels). */
        static std::pair<juce::String, juce::String> getKnobNames (Type);

        /** Whether the pitch setting does anything in a type. */
        static bool usesPitch (Type t)      { return t == Type::shimmer || t == Type::cloud; }

        /** Decay time (RT60, seconds) at a frequency, from decay and the low / high multipliers. */
        static double getDecayAt (const Settings&, double hz);

        /** The reverb's shape `seconds` after the pre-delay, whatever the frequency: its build-up
            and the type's shape (Bloom's swell, Reverse's windows, Gated's cut), in one part or
            two (Reverse's two read heads), each a level (dB) and how long its sound has been
            decaying. No parts: silent. */
        struct Envelope
        {
            int parts = 0;
            double shapeDb[2] {}, decaySeconds[2] {};

            /** The level (dB) for a frequency that rings for `rt60` seconds. */
            double levelDb (double rt60, bool frozen) const;
        };

        static Envelope getEnvelope (const Settings&, double seconds);

        /** The level (dB, 0 = loudest) of the reverb `seconds` after the pre-delay, for a
            frequency that rings for `rt60` seconds (from getDecayAt). The cuts aren't included. */
        static double getLevelDb (const Settings& s, double rt60, double seconds)  { return getEnvelope (s, seconds).levelDb (rt60, s.freeze); }

        /** How long the reverb can go on after the input stops (s), pre-delay included. */
        static double getTailSeconds (const Settings&);

        /** Reverse's window or Gated's hold plus release (s); 0 for the other types (displays mark it). */
        static double getShapeSeconds (const Settings&);

        /** Combined response (dB) of the pre or post cuts at a frequency. */
        static double getCutsDb (const Settings&, double hz, bool post, double sampleRate);

        /** Early reflection taps: time after the pre-delay (s) and gain, for displays. */
        struct Tap { double seconds; float gain; };
        static std::vector<Tap> getEarlyTaps (const Settings&);

        static constexpr float lowCutOff = 20.0f, highCutOff = 20000.0f;    // at these the cut is off
        static constexpr double lowCrossover = 250.0;                      // lows decay by lowDecay below this

        // Meters for editors: dry input and reverb (wet) output peaks, and the reverb's output
        // (after the post cuts, before the mix) for a spectrum.
        PeakMeter dryPeak, wetPeak;
        std::shared_ptr<AnalyserFifo> wetFifo = std::make_shared<AnalyserFifo>();

        static constexpr int numLines = 16, maxDiffusers = 6, numTaps = 24, numBloomTaps = 16, numGrains = 8;

    private:
        struct Delay
        {
            std::vector<float> data;
            int write = 0;

            void setMaxSamples (int n)              { data.assign ((size_t) juce::jmax (4, n), 0.0f); write = 0; }
            void clear()                            { std::fill (data.begin(), data.end(), 0.0f); write = 0; }
            int size() const noexcept               { return (int) data.size(); }
            void push (float x) noexcept            { data[(size_t) write] = x; advance(); }
            void advance() noexcept                 { if (++write == (int) data.size()) write = 0; }

            float read (int samplesAgo) const noexcept
            {
                auto i = write - 1 - samplesAgo;
                while (i < 0) i += (int) data.size();
                return data[(size_t) i];
            }

            /** Schroeder allpass using this line as its delay (its whole length): flat in level at
                every frequency, so it can sit inside the feedback loop. */
            float allPass (float x, float g) noexcept
            {
                const float delayed = data[(size_t) write];     // the oldest sample: delayed by the full length
                const float w = x + g * delayed;
                data[(size_t) write] = w;
                advance();
                return delayed - g * w;
            }

            float readFractional (double samplesAgo) const noexcept
            {
                const auto whole = (int) samplesAgo;
                const auto frac = (float) (samplesAgo - whole);
                return read (whole) * (1.0f - frac) + read (whole + 1) * frac;
            }

            /** Cubic (Hermite) interpolation: keeps more of the top than linear when modulated. */
            float readCubic (double samplesAgo) const noexcept
            {
                const auto whole = (int) samplesAgo;
                const auto t = (float) (samplesAgo - whole);
                const float ym1 = read (whole - 1), y0 = read (whole), y1 = read (whole + 1), y2 = read (whole + 2);
                const float c1 = 0.5f * (y1 - ym1);
                const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
                const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
                return ((c3 * t + c2) * t + c1) * t + y0;
            }
        };

        struct Filter   // one biquad, two channels
        {
            EqMath::Biquad c;
            double z[2][2] {};
            bool active = false;

            float process (int ch, float x) noexcept
            {
                const double y = c.b0 * x + z[ch][0];
                z[ch][0] = c.b1 * x - c.a1 * y + z[ch][1];
                z[ch][1] = c.b2 * x - c.a2 * y;
                return (float) y;
            }
        };

        /** A small, fixed-seed random generator, so every render is the same. */
        struct Random
        {
            uint32_t state = 0x9e3779b9u;
            void seed (uint32_t s) noexcept         { state = s != 0 ? s : 0x9e3779b9u; }
            float next() noexcept                   // 0..1
            {
                state ^= state << 13;
                state ^= state >> 17;
                state ^= state << 5;
                return (float) (state >> 8) * (1.0f / 16777216.0f);
            }
            float bipolar() noexcept                { return next() * 2.0f - 1.0f; }
        };

        struct Grain
        {
            bool active = false;
            double delay = 0.0, step = 0.0;     // samples behind the write position; change per sample
            int age = 0, length = 1;
            float left = 0.0f, right = 0.0f;
        };

        struct Voicing;
        static Voicing voicingOf (const Settings&);

        void updateFilters (const Settings&);
        void startGrain (const Settings&);

        static void hadamard (float* x) noexcept
        {
            for (int h = 1; h < numLines; h <<= 1)
                for (int i = 0; i < numLines; i += h * 2)
                    for (int j = i; j < i + h; ++j)
                    {
                        const float a = x[j], b = x[j + h];
                        x[j] = a + b;
                        x[j + h] = a - b;
                    }
        }

        double rate = 48000.0;

        Delay preDelayLine[2];
        Delay diffusers[2][maxDiffusers];
        Delay lines[numLines], loopAllpass[numLines];
        double lineLength[numLines] {};                         // smoothed, samples
        double lfoPhase[numLines] {};
        float walkFrom[numLines] {}, walkTo[numLines] {}, walkPos[numLines] {};
        Random lineRandom[numLines];
        float lowState[numLines] {}, dampState[numLines] {};    // one-pole states per line
        float earlyState[2][3] {};                              // early reflections' air absorption, per group
        Filter preLow, preHigh, postLow, postHigh;
        float designedCuts[4] { -1.0f, -1.0f, -1.0f, -1.0f };
        float lastDry = 1.0f, lastWet = 0.0f;
        float lastTankPeak = 0.0f;                          // the network's output level in the last block
        double silentSeconds = 1.0e9;                       // how long the input has been silent

        // Shimmer
        Delay shimmerLine;
        double shimmerPhase = 0.0;
        float shimmerLow = 0.0f, shimmerHigh = 0.0f, shimmerFeed = 0.0f;

        // Reverse
        Delay reverseLine[2], smear[2][2];
        int reverseAge[2] {}, reverseLength[2] { 1, 1 };

        // Bloom
        Delay bloomLine[2];
        float bloomLast[2] {};

        // Gated
        float gateEnvelope = 0.0f, gateGain = 0.0f;
        int gateHold = 0;

        // Cloud
        Delay cloudLine;
        Grain grains[numGrains];
        Random cloudRandom;
        int nextGrain = 0;                                  // samples until the next grain starts
        float cloudFeed = 0.0f;

        // Duck
        float duckEnvelope = 0.0f;

        juce::AudioBuffer<float> wetBuffer, inputBuffer;    // reverb output; diffused input to the network (+ the gate's detector)
    };
}
