cmake_minimum_required(VERSION 3.17)

project(SimpleSynth VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)          # PM framework uses std::numbers / std::span
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(SIMPLESYNTH_WERROR "Treat warnings as errors (for CI)" OFF)

# CLAP headers (vendored submodule; falls back to an installed package).
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/clap/CMakeLists.txt")
    add_subdirectory(external/clap EXCLUDE_FROM_ALL)
else()
    find_package(clap REQUIRED)
endif()

# --- the .clap instrument module -------------------------------------------
add_library(SimpleSynth MODULE src/SimpleSynth.cpp)
target_include_directories(SimpleSynth PRIVATE src)
target_link_libraries(SimpleSynth PRIVATE clap pm_framework)

if(APPLE)
    set_target_properties(SimpleSynth PROPERTIES
        BUNDLE TRUE
        BUNDLE_EXTENSION clap
        MACOSX_BUNDLE_GUI_IDENTIFIER com.example.simplesynth
        MACOSX_BUNDLE_BUNDLE_NAME SimpleSynth
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Info.plist.in")
    target_link_options(SimpleSynth PRIVATE
        -exported_symbols_list "${CMAKE_CURRENT_SOURCE_DIR}/cmake/simplesynth-macos.symbols")
elseif(UNIX)
    set_target_properties(SimpleSynth PROPERTIES PREFIX "" SUFFIX ".clap")
    target_link_options(SimpleSynth PRIVATE
        "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/cmake/simplesynth-linux.version"
        "-Wl,-z,defs")
elseif(WIN32)
    set_target_properties(SimpleSynth PROPERTIES PREFIX "" SUFFIX ".clap")
endif()

# --- lifecycle self-test ---------------------------------------------------
add_executable(simplesynth-smoke test/host_smoke.cpp src/SimpleSynth.cpp)
target_include_directories(simplesynth-smoke PRIVATE src)
target_link_libraries(simplesynth-smoke PRIVATE clap pm_framework)

# --- PM framework port (Phase 1b) ------------------------------------------
# Vendored PolygraphModular framework + our shim/MIDI layer, as a static lib. Compiled with
# relaxed warnings (vendored code, its own style); our glue and tests stay strict.
add_library(pm_framework STATIC
    src/Common/pm_midi.cpp
    src/Common/pm_param_data.cpp
    src/Module/SinOscillatorModule.cpp
    src/Module/ADSRHelper.cpp
    src/Module/ADSRModule.cpp
    src/Module/CombineModule.cpp
    src/Module/FinalModule.cpp
    src/Module/SlewModule.cpp
    src/Module/PitchConverterModule.cpp
    src/Module/ModuleFactory.cpp
    src/Voice/ModuleMap.cpp
    src/Voice/pm_module_specs.cpp
    src/Voice/VoiceStateManager.cpp
    src/Voice/Voice.cpp
    src/Voice/VoiceManager.cpp)
# All framework folders on the include path so the vendored `#include "X.hpp"` resolve
# across Module/ Voice/ Common/ (mirrors PolygraphModular's layout).
target_include_directories(pm_framework PUBLIC src/Common src/Module src/Voice)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(pm_framework PRIVATE -Wall -Wno-unknown-pragmas -Wno-unused-parameter)
endif()

add_executable(pm-selftest test/pm_selftest.cpp)
target_link_libraries(pm-selftest PRIVATE pm_framework clap)

add_executable(pm-module-test test/pm_module_test.cpp)
target_link_libraries(pm-module-test PRIVATE pm_framework)

add_executable(pm-sinosc-test test/pm_sinosc_test.cpp)
target_link_libraries(pm-sinosc-test PRIVATE pm_framework)

add_executable(pm-adsr-test test/pm_adsr_test.cpp)
target_link_libraries(pm-adsr-test PRIVATE pm_framework)

add_executable(pm-modules-test test/pm_modules_test.cpp)
target_link_libraries(pm-modules-test PRIVATE pm_framework)

add_executable(pm-factory-test test/pm_factory_test.cpp)
target_link_libraries(pm-factory-test PRIVATE pm_framework)

add_executable(pm-voice-test test/pm_voice_test.cpp)
target_link_libraries(pm-voice-test PRIVATE pm_framework)

add_executable(pm-voicemanager-test test/pm_voicemanager_test.cpp)
target_link_libraries(pm-voicemanager-test PRIVATE pm_framework)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    # pm-selftest only pulls in light headers → fully strict.
    # SimpleSynth + smoke are our code but include vendored framework headers → strict on our
    # logic, but tolerate the vendored style (unknown pragmas / unused params).
    target_compile_options(pm-selftest PRIVATE -Wall -Wextra)
    set(PM_VENDOR_WARN_SUPPRESS
        -Wno-unknown-pragmas -Wno-unused-parameter -Wno-sign-compare -Wno-unused-function)
    foreach(t SimpleSynth simplesynth-smoke)
        target_compile_options(${t} PRIVATE -Wall -Wextra ${PM_VENDOR_WARN_SUPPRESS})
    endforeach()
    foreach(t SimpleSynth simplesynth-smoke pm-selftest)
        if(SIMPLESYNTH_WERROR)
            target_compile_options(${t} PRIVATE -Werror)
        endif()
    endforeach()
    # tests that pull in vendored headers → relaxed (unused params in inline methods).
    foreach(t pm-module-test pm-sinosc-test pm-adsr-test pm-modules-test pm-factory-test
              pm-voice-test pm-voicemanager-test)
        target_compile_options(${t} PRIVATE -Wall -Wno-unknown-pragmas -Wno-unused-parameter)
    endforeach()
endif()

enable_testing()
add_test(NAME smoke COMMAND simplesynth-smoke)
set_tests_properties(smoke PROPERTIES PASS_REGULAR_EXPRESSION "ALL PASS")

add_test(NAME pm COMMAND pm-selftest)
set_tests_properties(pm PROPERTIES PASS_REGULAR_EXPRESSION "ALL PASS")

add_test(NAME pm-module COMMAND pm-module-test)
set_tests_properties(pm-module PROPERTIES PASS_REGULAR_EXPRESSION "ALL PASS")

add_test(NAME pm-sinosc COMMAND pm-sinosc-test)
set_tests_properties(pm-sinosc PROPERTIES PASS_REGULAR_EXPRESSION "ALL PASS")

add_test(NAME pm-adsr COMMAND pm-adsr-test)
set_tests_properties(pm-adsr PROPERTIES PASS_REGULAR_EXPRESSION "ALL PASS")

add_test(NAME pm-modules COMMAND pm-modules-test)
set_tests_properties(pm-modules PROPERTIES PASS_REGULAR_EXPRESSION "ALL PASS")

add_test(NAME pm-factory COMMAND pm-factory-test)
set_tests_properties(pm-factory PROPERTIES PASS_REGULAR_EXPRESSION "ALL PASS")

add_test(NAME pm-voice COMMAND pm-voice-test)
set_tests_properties(pm-voice PROPERTIES PASS_REGULAR_EXPRESSION "ALL PASS")

add_test(NAME pm-voicemanager COMMAND pm-voicemanager-test)
set_tests_properties(pm-voicemanager PROPERTIES PASS_REGULAR_EXPRESSION "ALL PASS")
