# The plugins' tests: a console app with no host, running the engines and views directly.
juce_add_console_app(cdaw_plugins_tests PRODUCT_NAME "cdaw_plugins_tests")

file(GLOB CDAW_PLUGINS_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
file(GLOB_RECURSE CDAW_PLUGINS_SHARED_SOURCES CONFIGURE_DEPENDS "${CDAW_PLUGINS_DIR}/Shared/*.cpp")
target_sources(cdaw_plugins_tests PRIVATE ${CDAW_PLUGINS_TEST_SOURCES} ${CDAW_PLUGINS_SHARED_SOURCES})
target_include_directories(cdaw_plugins_tests PRIVATE "${CDAW_PLUGINS_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")

target_compile_definitions(cdaw_plugins_tests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_STANDALONE_APPLICATION=1
    CDAW_PLUGINS_SOURCE_DIR="${CDAW_PLUGINS_DIR}")

target_link_libraries(cdaw_plugins_tests PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

if(MSVC)
    target_compile_options(cdaw_plugins_tests PRIVATE /bigobj /utf-8)
endif()

add_test(NAME cdaw_plugins_tests COMMAND cdaw_plugins_tests)
set_tests_properties(cdaw_plugins_tests PROPERTIES TIMEOUT 900)
