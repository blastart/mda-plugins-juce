cmake_minimum_required(VERSION 3.22)

project(MDAPlugins VERSION 2.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch JUCE automatically
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE
)
FetchContent_Declare(
    clap-juce-extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG e1f67893cc409a40c1154fa2e78c97046da24ce0  # main as of 2026-04-02
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE clap-juce-extensions)

# Common JUCE modules for all plugins
set(MDA_JUCE_MODULES
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_plugin_client
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
)

# Helper function to add an MDA plugin with consistent settings.
# Usage: mda_add_plugin(TARGET DIR PLUGIN_CODE PLUGIN_NAME PRODUCT_NAME CATEGORIES IS_SYNTH)
function(mda_add_plugin TARGET DIR PLUGIN_CODE PLUGIN_NAME PRODUCT_NAME CATEGORIES IS_SYNTH)
    if(IS_SYNTH)
        set(_MIDI_IN TRUE)
    else()
        set(_MIDI_IN FALSE)
    endif()

    juce_add_plugin(${TARGET}
        COMPANY_NAME "mda"
        PLUGIN_MANUFACTURER_CODE Mdav
        PLUGIN_CODE ${PLUGIN_CODE}
        FORMATS VST3 AU LV2
        PRODUCT_NAME "${PRODUCT_NAME}"
        PLUGIN_NAME "${PLUGIN_NAME}"
        IS_SYNTH ${IS_SYNTH}
        NEEDS_MIDI_INPUT ${_MIDI_IN}
        NEEDS_MIDI_OUTPUT FALSE
        VST3_CATEGORIES ${CATEGORIES}
        LV2URI "https://github.com/blastart/mda-plugins-juce#${TARGET}"
    )

    juce_generate_juce_header(${TARGET})

    target_sources(${TARGET} PRIVATE
        ${DIR}/Source/PluginProcessor.cpp
        ${DIR}/Source/PluginProcessor.h
    )

    target_compile_definitions(${TARGET} PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=1
    )

    target_link_libraries(${TARGET} PRIVATE
        ${MDA_JUCE_MODULES}
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
    )

    # Disable unused audio codecs
    target_compile_definitions(${TARGET} PUBLIC
        JUCE_USE_FLAC=0
        JUCE_USE_OGGVORBIS=0
    )

    # Add CLAP format support via clap-juce-extensions
    if(IS_SYNTH)
        set(_CLAP_FEATURES instrument)
    else()
        set(_CLAP_FEATURES audio-effect)
    endif()

    clap_juce_extensions_plugin(TARGET ${TARGET}
        CLAP_ID "com.github.blastart.mda.${TARGET}"
        CLAP_FEATURES ${_CLAP_FEATURES}
    )
endfunction()

# ============================================================================
# Effects
# ============================================================================

#                    TARGET          DIR        CODE  PLUGIN_NAME        PRODUCT_NAME     CATEGORIES     SYNTH
mda_add_plugin(MDAAmbience   Ambience    mdaA "mda Ambience"    "mda_Ambience"   Reverb         FALSE)
mda_add_plugin(MDABandisto   Bandisto    mdaD "mda Bandisto"    "mda_Bandisto"   Distortion     FALSE)
mda_add_plugin(MDABeatBox    BeatBox     mdaG "mda BeatBox"     "mda_BeatBox"    Fx             FALSE)
mda_add_plugin(MDADegrade    Degrade     mdaC "mda Degrade"     "mda_Degrade"    Distortion     FALSE)
mda_add_plugin(MDADelay      Delay       mday "mda Delay"       "mda_Delay"      Delay          FALSE)
mda_add_plugin(MDADetune     Detune      mdat "mda Detune"      "mda_Detune"     Modulation     FALSE)
mda_add_plugin(MDADither     Dither      mdad "mda Dither"      "mda_Dither"     Fx             FALSE)
mda_add_plugin(MDADynamics   Dynamics    mdaN "mda Dynamics"    "mda_Dynamics"   Dynamics       FALSE)
mda_add_plugin(MDAEnvelope   Envelope    mdae "mda Envelope"    "mda_Envelope"   Fx             FALSE)
mda_add_plugin(MDAImage      Image       mdaI "mda Image"       "mda_Image"      Spatial        FALSE)
mda_add_plugin(MDALeslie     Leslie      mdaH "mda Leslie"      "mda_Leslie"     Modulation     FALSE)
mda_add_plugin(MDALimiter    Limiter     mdaL "mda Limiter"     "mda_Limiter"    Dynamics       FALSE)
mda_add_plugin(MDALoudness   Loudness    mdal "mda Loudness"    "mda_Loudness"   Fx             FALSE)
mda_add_plugin(MDAOverdrive  Overdrive   mdaO "mda Overdrive"   "mda_Overdrive"  Distortion     FALSE)
mda_add_plugin(MDARezFilter  RezFilter   mdaF "mda RezFilter"   "mda_RezFilter"  Filter         FALSE)
mda_add_plugin(MDARingMod    RingMod     mdaR "mda RingMod"     "mda_RingMod"    Modulation     FALSE)
mda_add_plugin(MDAShepard    Shepard     mdah "mda Shepard"     "mda_Shepard"    Modulation     FALSE)
mda_add_plugin(MDASplitter   Splitter    mda7 "mda Splitter"    "mda_Splitter"   Fx             FALSE)
mda_add_plugin(MDAStereo     Stereo      mdaS "mda Stereo"      "mda_Stereo"     Spatial        FALSE)
mda_add_plugin(MDASubSynth   SubSynth    mdaB "mda SubSynth"    "mda_SubSynth"   Fx             FALSE)
mda_add_plugin(MDATestTone   TestTone    mdaT "mda TestTone"    "mda_TestTone"   Generator      FALSE)
mda_add_plugin(MDACombo      Combo       mdaX "mda Combo"       "mda_Combo"      Distortion     FALSE)
mda_add_plugin(MDADeEss      DeEss       mdas "mda De-ess"      "mda_De-ess"     Dynamics       FALSE)
mda_add_plugin(MDADubDelay   DubDelay    mdab "mda DubDelay"    "mda_DubDelay"   Delay          FALSE)
mda_add_plugin(MDAMultiBand  MultiBand   mdaM "mda MultiBand"   "mda_MultiBand"  Dynamics       FALSE)
mda_add_plugin(MDARePsycho   RePsycho    mdaY "mda RePsycho!"   "mda_RePsycho"   Fx             FALSE)
mda_add_plugin(MDARoundPan   RoundPan    mdaP "mda RoundPan"    "mda_RoundPan"   Spatial        FALSE)
mda_add_plugin(MDASpecMeter  SpecMeter  "mda?" "mda SpecMeter"  "mda_SpecMeter"  Analyzer       FALSE)
mda_add_plugin(MDATalkBox    TalkBox    "mda&" "mda TalkBox"    "mda_TalkBox"    Fx             FALSE)
mda_add_plugin(MDAThruZero   ThruZero    mdaZ "mda ThruZero"    "mda_ThruZero"   Modulation     FALSE)
mda_add_plugin(MDATracker    Tracker     mdaJ "mda Tracker"     "mda_Tracker"    Fx             FALSE)
mda_add_plugin(MDATransient  Transient   mdaK "mda Transient"   "mda_Transient"  Dynamics       FALSE)
mda_add_plugin(MDAVocInput   VocInput    mda3 "mda VocInput"    "mda_VocInput"   Fx             FALSE)
mda_add_plugin(MDAVocoder    Vocoder     mdav "mda Vocoder"     "mda_Vocoder"    Fx             FALSE)

# ============================================================================
# Instruments (synths — need MIDI input)
# ============================================================================

mda_add_plugin(MDADX10       DX10        MDAx "mda DX10"        "mda_DX10"       Instrument     TRUE)
mda_add_plugin(MDAEPiano     EPiano      MDAe "mda ePiano"      "mda_ePiano"     Instrument     TRUE)
mda_add_plugin(MDAJX10       JX10        MDAj "mda JX10"        "mda_JX10"       Instrument     TRUE)
mda_add_plugin(MDALooplex    Looplex    "MDA~" "mda Looplex"    "mda_Looplex"    Fx             TRUE)
mda_add_plugin(MDAPiano      Piano       MDAp "mda Piano"       "mda_Piano"      Instrument     TRUE)
