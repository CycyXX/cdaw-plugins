cmake_minimum_required(VERSION 3.25)

project(cdaw_plugins VERSION 0.1.0 LANGUAGES C CXX)

# Builds on its own (with the JUCE in third_party/JUCE), or as a part of CDAW
# (add_subdirectory, using the JUCE CDAW already has).
set(CDAW_PLUGINS_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)

    if(MSVC)
        # The same as CDAW: the static runtime, so a plugin needs nothing installed beside it;
        # full inlining; debug info in the objects; a link that can drop and fold code.
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>")
        set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/O2 /Ob2 /DNDEBUG")
        set(CMAKE_C_FLAGS_RELWITHDEBINFO "/O2 /Ob2 /DNDEBUG")
        set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "/DEBUG /INCREMENTAL:NO /OPT:REF /OPT:ICF")
        set(CMAKE_MODULE_LINKER_FLAGS_RELWITHDEBINFO "/DEBUG /INCREMENTAL:NO /OPT:REF /OPT:ICF")
        set(CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO "/DEBUG /INCREMENTAL:NO /OPT:REF /OPT:ICF")
    endif()
endif()

if(NOT TARGET juce::juce_core)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/JUCE/CMakeLists.txt")
        message(FATAL_ERROR "JUCE is missing. Run: git submodule update --init --depth 1")
    endif()
    add_subdirectory(third_party/JUCE EXCLUDE_FROM_ALL)
endif()

# VST3 everywhere, and Audio Units on macOS too.
set(CDAW_PLUGIN_FORMATS VST3)
if(APPLE)
    list(APPEND CDAW_PLUGIN_FORMATS AU)
endif()

include(VST3/Common/CDAWEffect.cmake)

add_subdirectory(VST3/CVerb)
add_subdirectory(VST3/CMulticomp)
add_subdirectory(VST3/CComp)
add_subdirectory(VST3/CDelay)
add_subdirectory(VST3/CChorus)
add_subdirectory(VST3/CPhaser)
add_subdirectory(VST3/CFilter)
add_subdirectory(VST3/CDist)
add_subdirectory(VST3/CEsser)
add_subdirectory(VST3/CSynth)
add_subdirectory(VST3/CLead)

option(CDAW_PLUGINS_BUILD_TESTS "Build the plugins' tests" ${PROJECT_IS_TOP_LEVEL})
if(CDAW_PLUGINS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
