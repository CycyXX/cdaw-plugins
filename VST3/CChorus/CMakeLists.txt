# C-Chorus as a VST3 plugin (see VST3/Common/CDAWEffect.cmake).
set(CDAW_SOURCE "${CDAW_PLUGINS_DIR}")

cdaw_add_effect_vst3(CChorus
    PRODUCT_NAME "C-Chorus"
    PLUGIN_CODE Cchr
    CATEGORY Modulation
    SOURCES
        CChorusProcessor.cpp
        "${CDAW_SOURCE}/Shared/DSP/ChorusEngine.cpp"
        "${CDAW_SOURCE}/Shared/Chorus/ChorusParameters.cpp"
        "${CDAW_SOURCE}/Shared/Chorus/ChorusView.cpp")
