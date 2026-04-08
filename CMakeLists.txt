cmake_minimum_required(VERSION 3.15)

if(APPLE)
    project(AudioCapture LANGUAGES C CXX OBJC OBJCXX)
else()
    project(AudioCapture LANGUAGES C CXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Release build by default
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(APPLE)
    # Default: build for current architecture only
    # Pass -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" for Universal Binary

    set_source_files_properties(src/main.cpp PROPERTIES LANGUAGE OBJCXX)

    add_executable(audio_capture
        src/main.cpp
        src/mac/system_audio_capture.mm
    )

    target_include_directories(audio_capture PRIVATE src src/mac)

    target_link_libraries(audio_capture
        "-framework ScreenCaptureKit"
        "-framework CoreMedia"
        "-framework AudioToolbox"
        "-framework CoreAudio"
        "-framework Foundation"
    )

    target_compile_options(audio_capture PRIVATE
        -Wall -Wextra -fobjc-arc
        $<$<CONFIG:Release>:-Os -flto>
    )

    target_link_options(audio_capture PRIVATE
        $<$<CONFIG:Release>:-flto -dead_strip>
    )

    set_target_properties(audio_capture PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )

    # Strip symbols in Release
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_custom_command(TARGET audio_capture POST_BUILD
            COMMAND ${CMAKE_STRIP} -x $<TARGET_FILE:audio_capture>
            COMMENT "Stripping symbols from audio_capture"
        )
    endif()

elseif(WIN32)
    add_executable(audio_capture
        src/main.cpp
        src/windows/wasapi_capture.cpp
    )

    target_include_directories(audio_capture PRIVATE src src/windows)

    target_link_libraries(audio_capture
        ole32 psapi mf mfplat mfreadwrite wmcodecdspuuid
    )

    if(MSVC)
        target_compile_options(audio_capture PRIVATE
            /W4
            $<$<CONFIG:Release>:/O2 /GL /GS->
        )
        target_link_options(audio_capture PRIVATE
            $<$<CONFIG:Release>:/LTCG /OPT:REF /OPT:ICF>
        )
        set_target_properties(audio_capture PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
    endif()

    set_target_properties(audio_capture PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )

    # Release: strip + UPX compress
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        find_program(UPX_EXECUTABLE upx)
        if(UPX_EXECUTABLE)
            add_custom_command(TARGET audio_capture POST_BUILD
                COMMAND ${UPX_EXECUTABLE} --best --lzma $<TARGET_FILE:audio_capture>
                COMMENT "UPX compressing audio_capture.exe"
            )
        else()
            message(STATUS "UPX not found - skipping compression. Install: choco install upx")
        endif()
    endif()
endif()
