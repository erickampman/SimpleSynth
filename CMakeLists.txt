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
target_link_libraries(SimpleSynth PRIVATE clap)

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
target_link_libraries(simplesynth-smoke PRIVATE clap)

# --- PM framework port (Phase 1b) ------------------------------------------
# MIDI layer (spec-derived UMP) + CLAP decoders + vendored AU-free leaf files.
add_executable(pm-selftest test/pm_selftest.cpp src/pm/pm_midi.cpp)
target_include_directories(pm-selftest PRIVATE src/pm)
target_link_libraries(pm-selftest PRIVATE clap)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    foreach(t SimpleSynth simplesynth-smoke pm-selftest)
        target_compile_options(${t} PRIVATE -Wall -Wextra)
        if(SIMPLESYNTH_WERROR)
            target_compile_options(${t} PRIVATE -Werror)
        endif()
    endforeach()
endif()

enable_testing()
add_test(NAME smoke COMMAND simplesynth-smoke)
set_tests_properties(smoke PROPERTIES PASS_REGULAR_EXPRESSION "ALL PASS")

add_test(NAME pm COMMAND pm-selftest)
set_tests_properties(pm PROPERTIES PASS_REGULAR_EXPRESSION "ALL PASS")
