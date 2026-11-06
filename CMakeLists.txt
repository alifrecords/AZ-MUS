cmake_minimum_required(VERSION 3.22)
project(AZMUS VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/bigobj /MP)
endif()
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "")
endif()

# Tracktion Engine (Personal litsenziya) + uning JUCE versiyasi
# ./te papkasida bo'lishi kerak (CI avtomatik yuklaydi, README ga qarang)
if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/te/modules/juce/CMakeLists.txt)
    message(FATAL_ERROR "Tracktion Engine topilmadi: ./te papkasiga klonlang (README)")
endif()
add_subdirectory(te/modules/juce ${CMAKE_BINARY_DIR}/juce)
add_subdirectory(te/modules ${CMAKE_BINARY_DIR}/tracktion)

juce_add_gui_app(AZMUS
    PRODUCT_NAME "AZ MUS"
    COMPANY_NAME "Azimxodjayev Music"
    BUNDLE_ID "com.azimxodjayevmusic.azmus"
    ICON_BIG ${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "AZ MUS mikrofon va audio kirishdan yozish uchun ruxsat so'raydi"
    NEEDS_CURL FALSE
    NEEDS_WEB_BROWSER FALSE)

juce_generate_juce_header(AZMUS)

juce_add_binary_data(AZMUSData SOURCES
    Resources/InterRegular.otf Resources/InterMedium.otf Resources/InterSemiBold.otf Resources/InterBold.otf
    Resources/icon.png)

target_sources(AZMUS PRIVATE Source/Main.cpp)
target_include_directories(AZMUS PRIVATE Source)

target_compile_definitions(AZMUS PRIVATE
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_AU=1
    JUCE_PLUGINHOST_LADSPA=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_APPLICATION_NAME_STRING="AZ MUS"
    JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}"
    TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH=1
    TRACKTION_AIR_WINDOWS=1)

# ASIO SDK (Windows): CI uni ./asiosdk ga yuklaydi
if(WIN32 AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/asiosdk/common)
    target_include_directories(AZMUS PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/asiosdk/common)
    target_compile_definitions(AZMUS PRIVATE JUCE_ASIO=1)
else()
    target_compile_definitions(AZMUS PRIVATE JUCE_ASIO=0)
endif()

target_link_libraries(AZMUS PRIVATE
    tracktion::tracktion_core
    tracktion::tracktion_engine
    tracktion::tracktion_graph
    juce::juce_audio_devices
    juce::juce_audio_processors
    juce::juce_audio_utils
    AZMUSData
    juce::juce_recommended_config_flags)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(AZMUS PRIVATE "-latomic")
endif()
if(APPLE)
    target_compile_options(AZMUS PRIVATE "-fno-aligned-allocation")
endif()
