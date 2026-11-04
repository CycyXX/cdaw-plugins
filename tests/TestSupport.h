#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <complex>
#include <functional>
#include <map>
#include <memory>
#include <vector>

namespace cdaw::test
{
    inline juce::File getTestDir()
    {
        auto d = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("cdaw_plugins_tests");
        d.createDirectory();
        return d;
    }

    inline juce::AudioBuffer<float> makeSine (double hz, float amplitude, double seconds, double rate = 48000.0, int channels = 2)
    {
        juce::AudioBuffer<float> b (channels, (int) (seconds * rate));
        for (int ch = 0; ch < channels; ++ch)
            for (int i = 0; i < b.getNumSamples(); ++i)
                b.setSample (ch, i, amplitude * (float) std::sin (juce::MathConstants<double>::twoPi * hz * i / rate));
        return b;
    }
}
