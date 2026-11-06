# C-Verb as a VST3 plugin (for Cubase and other hosts).
# Its DSP, parameters and editor are the same source files CDAW's built-in C-Verb compiles
# (Shared), so a change to the reverb lands in both. Only CVerbProcessor.* is VST3-specific.

juce_add_plugin(CVerb
    PRODUCT_NAME "C-Verb"
    COMPANY_NAME "Cyrill Durrer"
    BUNDLE_ID "com.CyrillDurrer.CVerb"   # no space allowed, see VST3/Common/CDAWEffect.cmake
    VERSION ${PROJECT_VERSION}
    PLUGIN_MANUFACTURER_CODE Cdaw
    PLUGIN_CODE Cvrb
    FORMATS ${CDAW_PLUGIN_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Reverb
    COPY_PLUGIN_AFTER_BUILD FALSE)

set(CDAW_SOURCE "${CDAW_PLUGINS_DIR}")

# The shared code: the only CDAW sources the plugin needs (none of them use Tracktion).
set(CVERB_SHARED_SOURCES
    "${CDAW_SOURCE}/Shared/DSP/AnalyserFifo.cpp"
    "${CDAW_SOURCE}/Shared/DSP/ReverbEngine.cpp"
    "${CDAW_SOURCE}/Shared/CVerb/CVerbParameters.cpp"
    "${CDAW_SOURCE}/Shared/CVerb/CVerbPresets.cpp"
    "${CDAW_SOURCE}/Shared/CVerb/CVerbView.cpp"
    "${CDAW_SOURCE}/Shared/UI/EffectWidgets.cpp"
    "${CDAW_SOURCE}/Shared/UI/ParamKnob.cpp"
    "${CDAW_SOURCE}/Shared/UI/PresetBar.cpp"
    "${CDAW_SOURCE}/Shared/LookAndFeel/CDAWLookAndFeel.cpp")

target_sources(CVerb PRIVATE
    CVerbProcessor.cpp
    CVerbProcessor.h
    ${CVERB_SHARED_SOURCES})

target_include_directories(CVerb PRIVATE "${CDAW_SOURCE}")

target_compile_definitions(CVerb PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(CVerb
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

set_target_properties(CVerb PROPERTIES FOLDER "Plugins")
