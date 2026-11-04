#pragma once

#include "../Common/ParameterSpec.h"
#include "../DSP/DistEngine.h"

namespace cdaw::DistParameters
{
    /** C-Dist's parameters (shared by CDAW's built-in plugin and the VST3). Stored by ID in
        projects and presets: never change one, only append. The defaults are the Singing Lead. */
    enum ID
    {
        voicing, input, drive, tight, character,
        bass, mid, midFreq, treble, smooth,
        level, blend,
        gateOn, gateThreshold, gateRelease,
        count
    };

    const ParameterSpec& get (int id);
    const ParameterSpecs& all();

    DistEngine::Settings toSettings (const std::function<float (int)>& valueOf);
}
