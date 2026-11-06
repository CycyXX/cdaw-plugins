#include "ReverbEngine.h"

namespace cdaw
{
    namespace
    {
        // Delay network line lengths at 48 kHz (distinct primes, ~25-70 ms), scaled by size and type.
        // Mixed so the lines fed from each side, and those heard on each side, cover the whole spread.
        constexpr int baseLengths[16] { 1201, 2297, 1583, 3049, 1867, 2593, 1453, 2897,
                                        2011, 1327, 2741, 1721, 3361, 2153, 2441, 3203 };

        // The allpass inside each line's loop (samples at 48 kHz, ~3-11 ms).
        constexpr int loopAllpassLengths[16] { 307, 139, 433, 227, 503, 197, 359, 277,
                                               163, 457, 251, 409, 331, 521, 383, 479 };

        constexpr double lfoRates[16] { 0.13, 0.21, 0.29, 0.37, 0.17, 0.27, 0.33, 0.41,
                                        0.11, 0.19, 0.23, 0.31, 0.39, 0.15, 0.25, 0.35 };

        // Input diffusers (ms), slightly different per side for a wide image. Plate and Gated use all six.
        constexpr double diffuserMs[2][6] { { 4.77, 3.59, 12.73, 9.30, 7.13, 15.91 }, { 5.11, 3.83, 13.61, 9.97, 7.61, 16.73 } };

        // Reverse's smearing allpasses (ms).
        constexpr double smearMs[2][2] { { 19.3, 31.7 }, { 21.1, 29.3 } };

        constexpr double maxPreDelayMs = 250.0, maxTapMs = 200.0;
        constexpr double maxSizeScale = 1.6, maxLengthScale = 1.15, maxModulationSamples48k = 10.0;
        constexpr double maxReverseSeconds = 2.0, maxBloomSeconds = 2.5, cloudSeconds = 3.0, shimmerWindowSeconds = 0.07;

        double sizeScale (double size)              { return 0.35 + 1.25 * size; }
        double tapScale (double size)               { return 0.4 + 0.9 * size; }

        double reverseSeconds (const ReverbEngine::Settings& s)     { return 0.1 * std::pow (20.0, (double) s.character); }
        double bloomSeconds (const ReverbEngine::Settings& s)       { return 0.1 * std::pow (25.0, (double) s.character); }
        double gateHoldSeconds (const ReverbEngine::Settings& s)    { return 0.05 * std::pow (16.0, (double) s.character); }
        double gateReleaseSeconds (const ReverbEngine::Settings& s) { return 0.005 * std::pow (50.0, (double) s.variation); }
        double grainSeconds (const ReverbEngine::Settings& s)       { return 0.02 * std::pow (15.0, (double) s.character); }

        /** Keeps feedback paths that could build up from running away: untouched up to 1, then
            bending over towards 2. */
        inline float safety (float x) noexcept
        {
            const float a = std::abs (x);
            if (a <= 1.0f)
                return x;
            const float over = a - 1.0f;
            return std::copysign (1.0f + over / (1.0f + over), x);
        }

        inline float hann (double phase) noexcept       { return (float) (0.5 - 0.5 * std::cos (2.0 * EqMath::pi * phase)); }

        /** Early reflections: a fixed, room-like scatter of taps per side (sparse first, then
            denser), falling with distance, with random signs. */
        struct EarlyPattern
        {
            double ms[2][ReverbEngine::numTaps];
            double alternative[2][ReverbEngine::numTaps];      // 0..1: where an irregular room moves the tap
            float gain[2][ReverbEngine::numTaps];
            float norm;                                         // keeps the level of the old 10-tap pattern
            float loudest;
        };

        const EarlyPattern& earlyPattern()
        {
            static const EarlyPattern pattern = []
            {
                EarlyPattern p {};
                uint32_t state = 0x2545f491u;
                auto random = [&state]
                {
                    state ^= state << 13;
                    state ^= state >> 17;
                    state ^= state << 5;
                    return (double) (state >> 8) / 16777216.0;
                };

                constexpr int n = ReverbEngine::numTaps;
                double sumSquares = 0.0;
                p.loudest = 0.0f;

                for (int side = 0; side < 2; ++side)
                    for (int k = 0; k < n; ++k)
                    {
                        const double x = juce::jlimit (0.0, 1.0, (k + 0.5 + 0.35 * (random() * 2.0 - 1.0)) / n);
                        p.ms[side][k] = 2.5 + 77.5 * std::pow (x, 0.8) + side * 1.3 * random();
                        p.alternative[side][k] = random();
                        const double sign = (random() < 0.5) ? -1.0 : 1.0;
                        p.gain[side][k] = (float) (sign * (0.75 + 0.25 * random()) / std::pow (1.0 + p.ms[side][k] / 12.0, 0.7));

                        if (side == 0)
                            sumSquares += (double) p.gain[side][k] * p.gain[side][k];
                        p.loudest = juce::jmax (p.loudest, std::abs (p.gain[side][k]));
                    }

                p.norm = (float) std::sqrt ((1.0 / (1.0 - 0.64)) / sumSquares);
                return p;
            }();

            return pattern;
        }

        /** Bloom's swell: taps spread over the bloom time, getting louder. */
        struct BloomPattern
        {
            double position[2][ReverbEngine::numBloomTaps];    // 0..1 of the bloom time
            float gain[ReverbEngine::numBloomTaps];
            float energyNorm, sumNorm;
        };

        const BloomPattern& bloomPattern()
        {
            static const BloomPattern pattern = []
            {
                BloomPattern p {};
                constexpr int n = ReverbEngine::numBloomTaps;
                double sum = 0.0, sumSquares = 0.0;

                for (int k = 0; k < n; ++k)
                {
                    for (int side = 0; side < 2; ++side)
                        p.position[side][k] = (k + 1.0 + 0.3 * std::sin (k * 2.39 + side * 1.7)) / n;

                    p.gain[k] = (float) std::pow ((k + 1.0) / n, 1.5);
                    sum += p.gain[k];
                    sumSquares += (double) p.gain[k] * p.gain[k];
                }

                p.energyNorm = (float) (1.0 / std::sqrt (sumSquares));
                p.sumNorm = (float) (1.0 / sum);
                return p;
            }();

            return pattern;
        }
    }

    //==============================================================================
    /** How a type sets the network up, from the settings. */
    struct ReverbEngine::Voicing
    {
        double lengthScale = 1.0;           // line lengths
        float earlyOut = 1.0f;              // early reflections heard
        float earlyFeed = 0.3f;             // early reflections fed into the tail
        double earlyTime = 1.0;             // spreads the reflections out
        float earlyIrregular = 0.0f;        // 0 = the regular pattern, 1 = moved about
        float earlyBright = 0.8f;           // the reflections' air absorption (higher = brighter)
        int numDiffusers = 4;
        float diffusionGain = 0.5f;
        float loopGain = 0.5f;              // the allpasses inside the loop
        double dampScale = 1.0;             // multiplies the damping frequency
        float modRandom = 0.5f;             // 0 = sine, 1 = random walk
        float modDepth = 1.0f;
        double modRate = 1.0;
        float tankOut = 1.0f;               // the network heard directly
        double buildUp = 0.02;              // s: how fast the tail fills in (displays)
    };

    ReverbEngine::Voicing ReverbEngine::voicingOf (const Settings& s)
    {
        Voicing v;
        const float c = juce::jlimit (0.0f, 1.0f, s.character), var = juce::jlimit (0.0f, 1.0f, s.variation);
        const float d = juce::jlimit (0.0f, 1.0f, s.diffusion);

        v.diffusionGain = 0.75f * d;
        v.loopGain = 0.25f + 0.35f * d;
        v.buildUp = 0.004 + 0.05 * d * (0.3 + s.size);

        switch (s.type)
        {
            case Type::hall:
                v.earlyOut = 1.5f - c;              // Distance: further back, less of the room's first reflections
                v.earlyTime = 0.85 + 0.3 * c;
                v.modRandom = var;                  // Spin: smooth sine to wandering
                v.modRate = 0.6 + 0.8 * var;
                break;

            case Type::room:
                v.lengthScale = 0.45;
                v.earlyOut = 1.4f;
                v.earlyFeed = 0.45f;
                v.earlyTime = 0.55 + 0.5 * c;       // Shape: a small box to a large, irregular room
                v.earlyIrregular = c;
                v.earlyBright = 0.35f + 1.0f * var; // Walls: soft and dark to hard and bright
                v.dampScale = 0.7 + 0.7 * var;
                v.loopGain = 0.3f + 0.35f * d;
                v.modRandom = 0.7f;
                v.modDepth = 0.5f;
                v.modRate = 0.8;
                v.buildUp = 0.002 + 0.02 * d * (0.3 + s.size);
                break;

            case Type::plate:
                v.lengthScale = 0.75;
                v.earlyOut = 0.0f;
                v.earlyFeed = 0.0f;
                v.numDiffusers = 6;
                v.diffusionGain = 0.5f + 0.25f * d;
                v.loopGain = 0.3f + 0.4f * c;       // Density
                v.dampScale = 0.6 + 1.0 * var;      // Brightness
                v.modRandom = 0.3f;
                v.buildUp = 0.003 + 0.015 * d;
                break;

            case Type::shimmer:
                v.lengthScale = 1.15;
                v.earlyOut = 0.5f;
                v.loopGain = 0.35f + 0.3f * d;
                v.modRandom = 0.6f;
                break;

            case Type::reverse:
                v.lengthScale = 0.8;
                v.earlyOut = 0.8f;
                break;

            case Type::bloom:
                v.lengthScale = 1.1;
                v.earlyOut = 0.3f;
                v.modRandom = 0.6f;
                break;

            case Type::gated:
                v.lengthScale = 0.55;
                v.earlyOut = 1.2f;
                v.earlyFeed = 0.4f;
                v.numDiffusers = 6;
                v.diffusionGain = 0.6f + 0.2f * d;
                v.loopGain = 0.55f;
                v.modDepth = 0.5f;
                v.buildUp = 0.002 + 0.01 * d;
                break;

            case Type::cloud:
                v.earlyOut = 0.3f;
                v.tankOut = 0.4f;
                v.modRandom = 0.8f;
                v.buildUp += 0.5 * grainSeconds (s);
                break;
        }

        return v;
    }

    //==============================================================================
    juce::StringArray ReverbEngine::getTypeNames()
    {
        return { "Hall", "Room", "Plate", "Shimmer", "Reverse", "Bloom", "Gated", "Cloud" };
    }

    std::pair<juce::String, juce::String> ReverbEngine::getKnobNames (Type t)
    {
        switch (t)
        {
            case Type::hall:    return { "Distance", "Spin" };
            case Type::room:    return { "Shape", "Walls" };
            case Type::plate:   return { "Density", "Brightness" };
            case Type::shimmer: return { "Amount", "Tone" };
            case Type::reverse: return { "Length", "Smear" };
            case Type::bloom:   return { "Bloom", "Feedback" };
            case Type::gated:   return { "Hold", "Release" };
            case Type::cloud:   return { "Grain", "Scatter" };
        }

        return { "Character", "Variation" };
    }

    double ReverbEngine::getDecayAt (const Settings& settings, double hz)
    {
        // Same split as the network: lows below 250 Hz, highs above the damping frequency.
        const double rt = settings.decay;
        const double damp = juce::jlimit (500.0, 20000.0, settings.dampFreq * voicingOf (settings).dampScale);
        const double wLow = 1.0 / (1.0 + std::pow (hz / lowCrossover, 2.0));
        const double wHigh = 1.0 / (1.0 + std::pow (damp / hz, 2.0));
        const double wMid = juce::jmax (0.0, 1.0 - wLow - wHigh);
        const double multiplier = (wLow * settings.lowDecay + wHigh * settings.highDecay + wMid) / (wLow + wHigh + wMid);
        return rt * multiplier;
    }

    double ReverbEngine::Envelope::levelDb (double rt60, bool frozen) const
    {
        const double slope = frozen ? 0.0 : 60.0 / juce::jmax (0.05, rt60);

        if (parts == 1)
            return shapeDb[0] - slope * decaySeconds[0];

        double power = 0.0;
        for (int i = 0; i < parts; ++i)
            power += std::pow (10.0, (shapeDb[i] - slope * decaySeconds[i]) * 0.1);

        return power > 1.0e-20 ? 10.0 * std::log10 (power) : -200.0;
    }

    ReverbEngine::Envelope ReverbEngine::getEnvelope (const Settings& s, double t)
    {
        Envelope e;
        if (t <= 0.0)
            return e;

        const double buildUp = voicingOf (s).buildUp;
        auto buildDb = [&] (double time) { return 20.0 * std::log10 (juce::jmax (1.0e-6, 1.0 - std::exp (-juce::jmax (0.0, time) / buildUp))); };
        auto add = [&e] (double shape, double decaying)
        {
            e.shapeDb[e.parts] = shape;
            e.decaySeconds[e.parts] = decaying;
            ++e.parts;
        };

        switch (s.type)
        {
            case Type::bloom:
            {
                const double swell = bloomSeconds (s);
                if (t < swell)  add (30.0 * std::log10 (juce::jmax (1.0e-3, t / swell)) + buildDb (t), 0.0);
                else            add (0.0, t - swell);
                return e;
            }

            case Type::reverse:
            {
                // As process() does it: two heads half a window apart, each playing the window
                // before its own backwards (so the tail of age 2 * start - t), faded in and out.
                const double window = reverseSeconds (s);
                for (int head = 0; head < 2; ++head)
                {
                    const double sinceStart = t - head * 0.5 * window;
                    if (sinceStart < 0.0)
                        continue;

                    const double intoWindow = std::fmod (sinceStart, window);
                    const double age = (t - intoWindow) - intoWindow;
                    const double fade = hann (intoWindow / window);
                    if (age >= 0.0 && fade > 1.0e-4)
                        add (20.0 * std::log10 (fade) + buildDb (age + 0.002), age);
                }
                return e;
            }

            case Type::gated:
            {
                const double hold = gateHoldSeconds (s), release = gateReleaseSeconds (s);
                if (t < hold)                 add (buildDb (t), t);
                else if (t < hold + release)  add (buildDb (t) - 60.0 * (t - hold) / release, t);
                return e;
            }

            case Type::hall: case Type::room: case Type::plate: case Type::shimmer: case Type::cloud:
                break;
        }

        add (buildDb (t), t);
        return e;
    }

    double ReverbEngine::getShapeSeconds (const Settings& s)
    {
        if (s.type == Type::reverse)    return reverseSeconds (s);
        if (s.type == Type::gated)      return gateHoldSeconds (s) + gateReleaseSeconds (s);
        return 0.0;
    }

    double ReverbEngine::getTailSeconds (const Settings& s)
    {
        double tail = s.preDelay * 0.001 + 0.15 + 1.5 * s.decay * juce::jmax (1.0f, s.lowDecay, s.highDecay);

        switch (s.type)
        {
            case Type::reverse: tail += 2.0 * reverseSeconds (s); break;
            case Type::bloom:   tail += bloomSeconds (s) * (1.0 + 3.0 * s.variation); break;
            case Type::shimmer: tail += 4.0 * s.decay * s.character; break;
            case Type::cloud:   tail += cloudSeconds + s.decay; break;
            case Type::hall: case Type::room: case Type::plate: case Type::gated: break;
        }

        return tail;
    }

    double ReverbEngine::getCutsDb (const Settings& settings, double hz, bool post, double rate)
    {
        const double sr = rate;
        const float lo = (post ? settings.postLowCut : settings.preLowCut);
        const float hi = (post ? settings.postHighCut : settings.preHighCut);
        double db = 0.0;

        if (lo > lowCutOff + 0.5f)
            db += EqMath::design (EqMath::BandType::lowCut, 1, lo, 0.0, 0.7071, sr).magnitudeDb (hz, sr);
        if (hi < highCutOff - 1.0f)
            db += EqMath::design (EqMath::BandType::highCut, 1, hi, 0.0, 0.7071, sr).magnitudeDb (hz, sr);

        return db;
    }

    std::vector<ReverbEngine::Tap> ReverbEngine::getEarlyTaps (const Settings& settings)
    {
        std::vector<Tap> taps;
        const auto voicing = voicingOf (settings);
        if (voicing.earlyOut <= 0.0f || settings.type == Type::reverse)
            return taps;

        const auto& pattern = earlyPattern();
        const double scale = tapScale (settings.size) * voicing.earlyTime;
        const float level = settings.early * juce::jmin (1.0f, voicing.earlyOut);

        for (int side = 0; side < 2; ++side)
            for (int k = 0; k < numTaps; ++k)
            {
                const double moved = 1.0 - 0.35 * voicing.earlyIrregular + 0.7 * voicing.earlyIrregular * pattern.alternative[side][k];
                taps.push_back ({ pattern.ms[side][k] * moved * scale * 0.001, level * std::abs (pattern.gain[side][k]) / pattern.loudest });
            }

        return taps;
    }

    //==============================================================================
    void ReverbEngine::prepare (double sampleRate, int blockSize, const Settings& settings)
    {
        rate = sampleRate > 0.0 ? sampleRate : 48000.0;
        wetFifo->sampleRate = rate;
        const double k = rate / 48000.0;
        auto samples = [this] (double seconds) { return (int) std::ceil (seconds * rate); };

        for (auto& d : preDelayLine)
            d.setMaxSamples (samples ((maxPreDelayMs + maxTapMs) * 0.001) + 4);

        for (int side = 0; side < 2; ++side)
        {
            for (int i = 0; i < maxDiffusers; ++i)
                diffusers[side][i].setMaxSamples (samples (diffuserMs[side][i] * 0.001));

            for (int i = 0; i < 2; ++i)
                smear[side][i].setMaxSamples (samples (smearMs[side][i] * 0.001));

            reverseLine[side].setMaxSamples (samples (2.0 * maxReverseSeconds) + 8);
            bloomLine[side].setMaxSamples (samples (maxBloomSeconds * 1.1) + 8);
        }

        for (int i = 0; i < numLines; ++i)
        {
            lines[i].setMaxSamples ((int) std::ceil (baseLengths[i] * maxSizeScale * maxLengthScale * k + maxModulationSamples48k * k) + 8);
            loopAllpass[i].setMaxSamples (juce::jmax (8, juce::roundToInt (loopAllpassLengths[i] * k)));
        }

        shimmerLine.setMaxSamples (samples (shimmerWindowSeconds) + 8);
        cloudLine.setMaxSamples (samples (cloudSeconds) + 8);

        wetBuffer.setSize (2, juce::jmax (512, blockSize), false, true, false);
        inputBuffer.setSize (3, juce::jmax (512, blockSize), false, true, false);
        for (auto& c : designedCuts)
            c = -1.0f;

        const double scale = sizeScale (settings.size) * voicingOf (settings).lengthScale * k;
        for (int i = 0; i < numLines; ++i)
            lineLength[i] = baseLengths[i] * scale;

        reset();
    }

    void ReverbEngine::reset()
    {
        for (auto& d : preDelayLine) d.clear();
        for (auto& side : diffusers) for (auto& d : side) d.clear();
        for (auto& side : smear) for (auto& d : side) d.clear();
        for (auto& d : lines) d.clear();
        for (auto& d : loopAllpass) d.clear();
        for (auto& d : reverseLine) d.clear();
        for (auto& d : bloomLine) d.clear();
        shimmerLine.clear();
        cloudLine.clear();

        std::fill (std::begin (lowState), std::end (lowState), 0.0f);
        std::fill (std::begin (dampState), std::end (dampState), 0.0f);
        for (auto& side : earlyState)
            std::fill (std::begin (side), std::end (side), 0.0f);

        // Every line moves its own way, from its own starting point.
        for (int i = 0; i < numLines; ++i)
        {
            lineRandom[i].seed (0x1234567u + (uint32_t) i * 0x9e3779b9u);
            lfoPhase[i] = std::fmod (i * 0.6180339887, 1.0);
            walkFrom[i] = lineRandom[i].bipolar();
            walkTo[i] = lineRandom[i].bipolar();
            walkPos[i] = lineRandom[i].next();
        }

        lastTankPeak = 0.0f;
        silentSeconds = 1.0e9;      // nothing in the network yet

        shimmerPhase = 0.0;
        shimmerLow = shimmerHigh = shimmerFeed = 0.0f;
        reverseAge[0] = reverseAge[1] = 0;
        reverseLength[0] = reverseLength[1] = 0;
        bloomLast[0] = bloomLast[1] = 0.0f;
        gateEnvelope = gateGain = 0.0f;
        gateHold = 0;
        for (auto& g : grains)
            g = {};
        cloudRandom.seed (0x51ed270bu);
        nextGrain = 0;
        cloudFeed = 0.0f;
        duckEnvelope = 0.0f;

        for (auto* f : { &preLow, &preHigh, &postLow, &postHigh })
            for (auto& ch : f->z)
                ch[0] = ch[1] = 0.0;
    }

    void ReverbEngine::jumpTo (const Settings& settings)
    {
        const double k = rate / 48000.0;
        const double scale = sizeScale (settings.size) * voicingOf (settings).lengthScale * k;
        for (int i = 0; i < numLines; ++i)
            lineLength[i] = baseLengths[i] * scale;

        lastDry = std::cos (settings.mix * juce::MathConstants<float>::halfPi);
        lastWet = std::sin (settings.mix * juce::MathConstants<float>::halfPi);
    }

    void ReverbEngine::updateFilters (const Settings& settings)
    {
        const float now[4] { settings.preLowCut, settings.preHighCut, settings.postLowCut, settings.postHighCut };
        Filter* filters[4] { &preLow, &preHigh, &postLow, &postHigh };

        for (int i = 0; i < 4; ++i)
        {
            if (now[i] == designedCuts[i])
                continue;

            const bool lowCut = (i % 2) == 0;
            auto& f = *filters[i];
            const bool wasActive = f.active;
            f.active = lowCut ? now[i] > lowCutOff + 0.5f : now[i] < highCutOff - 1.0f;

            if (f.active)
                f.c = EqMath::design (lowCut ? EqMath::BandType::lowCut : EqMath::BandType::highCut, 1, now[i], 0.0, 0.7071, rate).sections[0];

            if (f.active && ! wasActive)
                for (auto& ch : f.z)
                    ch[0] = ch[1] = 0.0;

            designedCuts[i] = now[i];
        }
    }

    void ReverbEngine::startGrain (const Settings& s)
    {
        Grain* voice = nullptr;
        for (auto& g : grains)
            if (! g.active)
            {
                voice = &g;
                break;
            }

        // Every grain is drawn the same way whether or not a voice is free, so the pattern
        // doesn't depend on how the grains overlapped.
        const float scatter = juce::jlimit (0.0f, 1.0f, s.variation);
        const float lengthRandom = cloudRandom.bipolar(), pitchChoice = cloudRandom.next(), cents = cloudRandom.bipolar();
        const float when = cloudRandom.next(), pan = cloudRandom.bipolar();

        if (voice == nullptr)
            return;

        const int length = juce::jmax (16, (int) (grainSeconds (s) * rate * (1.0 + 0.3 * scatter * lengthRandom)));
        double ratio = (std::abs (s.pitch) > 0.01f && pitchChoice < 0.35f) ? std::pow (2.0, s.pitch / 12.0) : 1.0;
        ratio *= std::pow (2.0, scatter * 0.3 * cents / 12.0);

        const double step = ratio - 1.0;                        // the read position falls behind (or gains) this much per sample
        const double minimum = juce::jmax (0.0, step) * length + 4.0;
        const double room = cloudLine.size() - 8.0 - juce::jmax (0.0, -step) * length;
        const double delay = juce::jlimit (minimum, juce::jmax (minimum, room), minimum + rate * (0.01 + 0.8 * scatter * when));

        const double angle = (0.5 + 0.45 * pan * (0.4 + 0.6 * scatter)) * juce::MathConstants<double>::halfPi;

        voice->active = true;
        voice->age = 0;
        voice->length = length;
        voice->delay = delay;
        voice->step = step;
        voice->left = (float) std::cos (angle);
        voice->right = (float) std::sin (angle);
    }

    void ReverbEngine::process (juce::AudioBuffer<float>& buffer, int start, int n, const Settings& settings)
    {
        if (n <= 0 || lines[0].data.size() < 8)
            return;

        juce::ScopedNoDenormals noDenormals;
        const int numChannels = buffer.getNumChannels();

        if (numChannels == 0)
            return;

        // A block longer than the one prepared for (some offline renders): process it in pieces
        // rather than passing it through dry.
        if (n > wetBuffer.getNumSamples())
        {
            for (int done = 0; done < n;)
            {
                const int chunk = juce::jmin (n - done, wetBuffer.getNumSamples());
                process (buffer, start + done, chunk, settings);
                done += chunk;
            }

            return;
        }

        // Nothing has come in for longer than the longest possible tail: nothing to do. Skipping
        // also holds the modulation, so the reverb starts the same way whenever sound arrives,
        // however long the host ran it on silence before (otherwise two hosts give slightly
        // different tails).
        {
            float inputPeak = 0.0f;
            for (int ch = 0; ch < juce::jmin (2, numChannels); ++ch)
                inputPeak = juce::jmax (inputPeak, buffer.getMagnitude (ch, start, n));

            silentSeconds = inputPeak == 0.0f ? silentSeconds + n / rate : 0.0;

            if (silentSeconds > getTailSeconds (settings) && lastTankPeak < 1.0e-6f && ! settings.freeze)
            {
                lastDry = std::cos (settings.mix * juce::MathConstants<float>::halfPi);
                lastWet = std::sin (settings.mix * juce::MathConstants<float>::halfPi);
                dryPeak.push (0.0f);
                wetPeak.push (0.0f);
                return;
            }
        }

        updateFilters (settings);

        const auto voicing = voicingOf (settings);
        const auto type = settings.type;
        const double k = rate / 48000.0;
        const bool frozen = settings.freeze;
        const double rt = juce::jmax (0.05f, settings.decay);
        const double lowMult = settings.lowDecay, highMult = settings.highDecay;
        const double damp = juce::jlimit (500.0, juce::jmin (20000.0, rate * 0.45), settings.dampFreq * voicing.dampScale);
        const float lowCoeff = (float) (1.0 - std::exp (-2.0 * EqMath::pi * lowCrossover / rate));
        const float dampCoeff = (float) (1.0 - std::exp (-2.0 * EqMath::pi * damp / rate));
        const double targetScale = sizeScale (settings.size) * voicing.lengthScale * k;
        const double modDepth = settings.modulation * maxModulationSamples48k * k * voicing.modDepth;
        const int preDelaySamples = (int) (settings.preDelay * 0.001 * rate);
        const float widthValue = settings.width;
        constexpr float injection = 0.35f;
        constexpr float lineOutput = 1.47f;     // about as loud as the earlier engine: a little louder on noise, quieter on notes
        constexpr float feedbackTap = 0.495f;   // what Shimmer and Cloud take from the network (their loop gains are set for it)
        constexpr float stageOutput = lineOutput / feedbackTap;

        // Per-line loss for lows / mids / highs (one pass round the loop, its allpass included:
        // 60 dB in RT60 seconds).
        float gainLow[numLines], gainMid[numLines], gainHigh[numLines];
        for (int i = 0; i < numLines; ++i)
        {
            const double secondsPerPass = (lineLength[i] + loopAllpass[i].size()) / rate;
            gainMid[i]  = frozen ? 1.0f : (float) std::pow (10.0, -3.0 * secondsPerPass / rt);
            gainLow[i]  = frozen ? 1.0f : (float) std::pow (10.0, -3.0 * secondsPerPass / (rt * lowMult));
            gainHigh[i] = frozen ? 1.0f : (float) std::pow (10.0, -3.0 * secondsPerPass / (rt * highMult));
        }

        float* outL = buffer.getWritePointer (0, start);
        float* outR = numChannels > 1 ? buffer.getWritePointer (1, start) : nullptr;
        float* wetL = wetBuffer.getWritePointer (0);
        float* wetR = wetBuffer.getWritePointer (1);
        float* networkIn[2] { inputBuffer.getWritePointer (0), inputBuffer.getWritePointer (1) };
        float* detector = inputBuffer.getWritePointer (2);
        float dryMax = 0.0f;

        for (int s = 0; s < n; ++s)
            dryMax = juce::jmax (dryMax, std::abs (outL[s]), outR != nullptr ? std::abs (outR[s]) : 0.0f);

        // 1. Per channel over the block: pre cuts, pre-delay, early reflections (into the wet
        //    buffer and into the tail), Bloom's swell and diffusion (into the network's input).
        {
            const auto& pattern = earlyPattern();
            const bool earlyOn = voicing.earlyOut > 0.0f || voicing.earlyFeed > 0.0f;
            const float earlyOutGain = type == Type::reverse ? 0.0f : settings.early * 0.45f * voicing.earlyOut * pattern.norm;
            const float earlyFeedGain = voicing.earlyFeed * pattern.norm;
            const double tapSamples = tapScale (settings.size) * voicing.earlyTime * 0.001 * rate;

            float groupCoeff[3];
            for (int g = 0; g < 3; ++g)
                groupCoeff[g] = (float) (1.0 - std::exp (-2.0 * EqMath::pi * juce::jmin (rate * 0.45, 16000.0 * voicing.earlyBright / (1.0 + g)) / rate));

            const bool bloom = type == Type::bloom;
            const auto& swell = bloomPattern();
            const float bloomFeedback = frozen ? 0.0f : 0.7f * juce::jlimit (0.0f, 1.0f, settings.variation);
            const double bloomLength = juce::jmin (bloomSeconds (settings), maxBloomSeconds) * rate;

            for (int ch = 0; ch < 2; ++ch)
            {
                const float* dry = ch == 0 || outR == nullptr ? outL : outR;
                float* early = ch == 0 ? wetL : wetR;
                auto& preDelayed = preDelayLine[ch];

                int tapOffset[numTaps];
                for (int t = 0; t < numTaps; ++t)
                {
                    const double moved = 1.0 - 0.35 * voicing.earlyIrregular + 0.7 * voicing.earlyIrregular * pattern.alternative[ch][t];
                    tapOffset[t] = juce::jmin (preDelayed.size() - 2, preDelaySamples + (int) (pattern.ms[ch][t] * moved * tapSamples));
                }

                int bloomOffset[numBloomTaps];
                for (int t = 0; t < numBloomTaps; ++t)
                    bloomOffset[t] = juce::jlimit (0, bloomLine[ch].size() - 2, (int) (swell.position[ch][t] * bloomLength));

                for (int s = 0; s < n; ++s)
                {
                    float x = dry[s];
                    if (preLow.active)  x = preLow.process (ch, x);
                    if (preHigh.active) x = preHigh.process (ch, x);
                    preDelayed.push (frozen ? 0.0f : x);

                    float reflections = 0.0f;
                    if (earlyOn)
                    {
                        float groups[3] {};
                        for (int t = 0; t < numTaps; ++t)
                            groups[t / 8] += pattern.gain[ch][t] * preDelayed.read (tapOffset[t]);

                        for (int g = 0; g < 3; ++g)
                        {
                            earlyState[ch][g] += groupCoeff[g] * (groups[g] - earlyState[ch][g]);
                            reflections += earlyState[ch][g];
                        }
                    }
                    early[s] = reflections * earlyOutGain;

                    const float direct = preDelayed.read (preDelaySamples);
                    float y = direct + reflections * earlyFeedGain;

                    if (bloom)
                    {
                        auto& line = bloomLine[ch];
                        line.push (safety (y + bloomFeedback * bloomLast[ch]));

                        float sum = 0.0f;
                        for (int t = 0; t < numBloomTaps; ++t)
                            sum += swell.gain[t] * line.read (bloomOffset[t]);

                        bloomLast[ch] = sum * swell.sumNorm;
                        y = sum * swell.energyNorm;
                    }

                    // Diffusion: allpasses smear the input into a dense wash before the network.
                    for (int i = 0; i < voicing.numDiffusers; ++i)
                        y = diffusers[ch][i].allPass (y, voicing.diffusionGain);

                    networkIn[ch][s] = y;
                    detector[s] = ch == 0 ? std::abs (direct) : juce::jmax (detector[s], std::abs (direct));
                }
            }
        }

        // 2. The feedback delay network, sample by sample, with Shimmer's and Cloud's stages that
        //    feed back into it; its output is added to the early reflections.
        constexpr float hadamardScale = 0.25f;     // 1 / sqrt (16)
        const float injectionGain = frozen ? 0.0f : injection;
        const float loopGain = voicing.loopGain;
        const float modRandom = voicing.modRandom;
        float tankPeak = 0.0f;

        double walkStep[numLines], lfoStep[numLines];
        for (int i = 0; i < numLines; ++i)
        {
            lfoStep[i] = lfoRates[i] * voicing.modRate / rate;
            walkStep[i] = voicing.modRate / ((0.35 + 0.045 * i) * rate);
        }

        const bool shimmer = type == Type::shimmer, cloud = type == Type::cloud;

        // Only the type that's on feeds back (a value left from another type would drive the tank forever).
        if (! shimmer) shimmerFeed = 0.0f;
        if (! cloud)   cloudFeed = 0.0f;
        const double shimmerRatio = std::pow (2.0, juce::jlimit (-24.0f, 24.0f, settings.pitch) / 12.0);
        const double shimmerWindow = shimmerWindowSeconds * rate;
        const float shimmerAmount = juce::jlimit (0.0f, 1.0f, settings.character);
        const float longDecay = (float) std::sqrt (2.0 / juce::jmax (2.0, settings.decay * juce::jmax (1.0, (double) settings.lowDecay)));
        const float shimmerFeedback = shimmerAmount * 1.35f * longDecay;
        const float shimmerLowCoeff = (float) (1.0 - std::exp (-2.0 * EqMath::pi * juce::jmin (rate * 0.45, 1500.0 * std::pow (8.0, (double) settings.variation)) / rate));
        const float shimmerHighCoeff = (float) (1.0 - std::exp (-2.0 * EqMath::pi * 150.0 / rate));
        const int grainSpacing = juce::jmax (8, (int) (grainSeconds (settings) * rate / 3.0));
        const float grainGain = 1.6f * stageOutput, cloudFeedback = 0.2f * longDecay;

        for (int s = 0; s < n; ++s)
        {
            float v[numLines], f[numLines];

            for (int i = 0; i < numLines; ++i)
            {
                lineLength[i] += (baseLengths[i] * targetScale - lineLength[i]) * 0.0005;      // glide on size and type changes

                double offset = 0.0;
                if (modDepth > 0.0)
                {
                    lfoPhase[i] += lfoStep[i];
                    if (lfoPhase[i] >= 1.0)
                        lfoPhase[i] -= 1.0;

                    walkPos[i] += (float) walkStep[i];
                    if (walkPos[i] >= 1.0f)
                    {
                        walkPos[i] -= 1.0f;
                        walkFrom[i] = walkTo[i];
                        walkTo[i] = lineRandom[i].bipolar();
                    }

                    const float t = walkPos[i], smooth = t * t * (3.0f - 2.0f * t);
                    const float walk = walkFrom[i] + (walkTo[i] - walkFrom[i]) * smooth;
                    const float sine = (float) std::sin (2.0 * EqMath::pi * lfoPhase[i]);
                    offset = modDepth * (0.5 + 0.5 * ((1.0f - modRandom) * sine + modRandom * walk));
                }

                const double readPos = juce::jlimit (2.0, (double) lines[i].size() - 4.0, lineLength[i] + offset);
                v[i] = lines[i].readCubic (readPos);

                // Split into lows / mids / highs and give each its own decay.
                lowState[i] += lowCoeff * (v[i] - lowState[i]);
                dampState[i] += dampCoeff * (v[i] - dampState[i]);
                const float lowPart = lowState[i], highPart = v[i] - dampState[i], midPart = dampState[i] - lowState[i];
                const float filtered = frozen ? v[i] : gainLow[i] * lowPart + gainMid[i] * midPart + gainHigh[i] * highPart;

                // An allpass inside the loop: the echoes multiply quickly into a smooth tail.
                f[i] = loopAllpass[i].allPass (filtered, loopGain);
            }

            hadamard (f);

            const float fed = frozen ? 0.0f : (shimmerFeed + cloudFeed) * injection;
            for (int i = 0; i < numLines; ++i)
            {
                const float input = networkIn[i < 8 ? 0 : 1][s] * injectionGain + fed;
                lines[i].push (f[i] * hadamardScale + ((i & 1) ? -input : input));
            }

            float sumL = 0.0f, sumR = 0.0f;
            for (int i = 0; i < numLines; i += 2)
            {
                const float sign = ((i >> 1) & 1) ? -1.0f : 1.0f;
                sumL += sign * v[i];
                sumR += sign * v[i + 1];
            }
            const float tankL = sumL * lineOutput, tankR = sumR * lineOutput;
            const float tankMono = 0.5f * (sumL + sumR) * feedbackTap;
            tankPeak = juce::jmax (tankPeak, std::abs (tankL), std::abs (tankR));

            wetL[s] += tankL * voicing.tankOut;
            wetR[s] += tankR * voicing.tankOut;

            if (shimmer)
            {
                // Two read heads sliding through a short buffer, crossfaded: the tail an interval up
                // (or down), filtered, and fed back so it climbs on.
                shimmerLine.push (tankMono);
                shimmerPhase += (1.0 - shimmerRatio) / shimmerWindow;
                shimmerPhase -= std::floor (shimmerPhase);
                const double other = shimmerPhase + 0.5 - (shimmerPhase >= 0.5 ? 1.0 : 0.0);

                float y = hann (shimmerPhase) * shimmerLine.readFractional (2.0 + shimmerPhase * shimmerWindow)
                        + hann (other) * shimmerLine.readFractional (2.0 + other * shimmerWindow);

                shimmerLow += shimmerLowCoeff * (y - shimmerLow);
                shimmerHigh += shimmerHighCoeff * (shimmerLow - shimmerHigh);
                y = shimmerLow - shimmerHigh;

                shimmerFeed = frozen ? 0.0f : safety (y * shimmerFeedback);
                wetL[s] += y * 0.4f * shimmerAmount * stageOutput;
                wetR[s] += y * 0.4f * shimmerAmount * stageOutput;
            }

            if (cloud)
            {
                cloudLine.push (tankMono);

                if (--nextGrain <= 0)
                {
                    startGrain (settings);
                    nextGrain = juce::jmax (8, (int) (grainSpacing * (1.0f + 0.5f * settings.variation * cloudRandom.bipolar())));
                }

                float left = 0.0f, right = 0.0f;
                for (auto& g : grains)
                {
                    if (! g.active)
                        continue;

                    const float x = cloudLine.readFractional (juce::jlimit (1.0, cloudLine.size() - 3.0, g.delay)) * hann ((double) g.age / g.length);
                    left += x * g.left;
                    right += x * g.right;
                    g.delay -= g.step;
                    if (++g.age >= g.length)
                        g.active = false;
                }

                wetL[s] += left * grainGain;
                wetR[s] += right * grainGain;
                cloudFeed = frozen ? 0.0f : safety (0.5f * (left + right) / stageOutput * cloudFeedback);
            }
        }

        // 3. Reverse plays the reverb backwards in windows; Gated cuts it off.
        if (type == Type::reverse)
        {
            const int window = juce::jlimit (16, (reverseLine[0].size() - 8) / 2, (int) (reverseSeconds (settings) * rate));
            const float smearGain = 0.65f * juce::jlimit (0.0f, 1.0f, settings.variation);

            for (int s = 0; s < n; ++s)
            {
                reverseLine[0].push (wetL[s]);
                reverseLine[1].push (wetR[s]);
                float left = 0.0f, right = 0.0f;

                for (int h = 0; h < 2; ++h)
                {
                    if (reverseLength[h] <= 0)
                    {
                        reverseLength[h] = window;
                        reverseAge[h] = h == 0 ? 0 : window / 2;
                    }

                    // The window's start was `age` samples ago: read back from there, twice as far each sample.
                    const int age = reverseAge[h];
                    const float w = hann ((double) age / reverseLength[h]);
                    left += w * reverseLine[0].read (2 * age);
                    right += w * reverseLine[1].read (2 * age);

                    if (++reverseAge[h] >= reverseLength[h])
                    {
                        reverseAge[h] = 0;
                        reverseLength[h] = window;
                    }
                }

                if (smearGain > 0.0f)
                    for (int i = 0; i < 2; ++i)
                    {
                        left = smear[0][i].allPass (left, smearGain);
                        right = smear[1][i].allPass (right, smearGain);
                    }

                wetL[s] = left;
                wetR[s] = right;
            }
        }
        else if (type == Type::gated)
        {
            const int hold = (int) (gateHoldSeconds (settings) * rate);
            const float closing = (float) (1.0 / juce::jmax (1.0, gateReleaseSeconds (settings) * rate));
            const float opening = (float) (1.0 / (0.001 * rate));
            const float fall = (float) std::exp (-1.0 / (0.01 * rate));
            constexpr float threshold = 0.003f;     // about -50 dB

            for (int s = 0; s < n; ++s)
            {
                gateEnvelope = juce::jmax (detector[s], gateEnvelope * fall);
                if (gateEnvelope > threshold)
                    gateHold = hold;
                else if (gateHold > 0)
                    --gateHold;

                if (gateHold > 0 || frozen)
                    gateGain = juce::jmin (1.0f, gateGain + opening);
                else
                    gateGain = juce::jmax (0.0f, gateGain - closing);

                wetL[s] *= gateGain;
                wetR[s] *= gateGain;
            }
        }

        // 4. Over the block: width (mid / side), then the post cuts.
        for (int s = 0; s < n; ++s)
        {
            const float mid = 0.5f * (wetL[s] + wetR[s]), side = 0.5f * (wetL[s] - wetR[s]) * widthValue;
            wetL[s] = mid + side;
            wetR[s] = mid - side;
        }

        for (int ch = 0; ch < 2; ++ch)
        {
            float* wet = ch == 0 ? wetL : wetR;
            for (int s = 0; s < n; ++s)
            {
                if (postLow.active)  wet[s] = postLow.process (ch, wet[s]);
                if (postHigh.active) wet[s] = postHigh.process (ch, wet[s]);
            }
        }

        {
            juce::AudioBuffer<float> wetView (wetBuffer.getArrayOfWritePointers(), 2, 0, n);
            wetFifo->push (wetView);
        }

        // Equal-power mix, ramped across the block, with the reverb ducked under the input.
        const float m = settings.mix;
        const float dryGain = std::cos (m * juce::MathConstants<float>::halfPi);
        const float wetGain = std::sin (m * juce::MathConstants<float>::halfPi);
        const float duck = juce::jlimit (0.0f, 1.0f, settings.duck);
        const float duckAttack = (float) (1.0 - std::exp (-1.0 / (0.005 * rate)));
        const float duckRelease = (float) (1.0 - std::exp (-1.0 / (0.15 * rate)));
        float wetMax = 0.0f;

        for (int s = 0; s < n; ++s)
        {
            const float t = (float) (s + 1) / (float) n;
            const float dg = lastDry + (dryGain - lastDry) * t;
            float wg = lastWet + (wetGain - lastWet) * t;

            const float dryL = outL[s], dryR = outR != nullptr ? outR[s] : dryL;
            const float level = juce::jmax (std::abs (dryL), std::abs (dryR));
            duckEnvelope += (level > duckEnvelope ? duckAttack : duckRelease) * (level - duckEnvelope);

            if (duck > 0.0f)
            {
                // Full reduction (20 dB at the most) from -20 dBFS up, none below -40.
                const float db = juce::Decibels::gainToDecibels (duckEnvelope, -100.0f);
                const float reduction = duck * 20.0f * juce::jlimit (0.0f, 1.0f, (db + 40.0f) / 20.0f);
                wg *= juce::Decibels::decibelsToGain (-reduction);
            }

            wetMax = juce::jmax (wetMax, std::abs (wetL[s] * wg), std::abs (wetR[s] * wg));

            outL[s] = dryL * dg + wetL[s] * wg;
            if (outR != nullptr)
                outR[s] = dryR * dg + wetR[s] * wg;
            else
                outL[s] = dryL * dg + 0.5f * (wetL[s] + wetR[s]) * wg;
        }

        lastDry = dryGain;
        lastWet = wetGain;
        lastTankPeak = tankPeak;
        dryPeak.push (dryMax);
        wetPeak.push (wetMax);
    }
}
