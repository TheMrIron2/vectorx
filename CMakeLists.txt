cmake_minimum_required(VERSION 3.24)
project(vectorx VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(VECTORX_BUILD_GAME "Build the SDL3/OpenGL application" ON)
include(CTest)

add_library(vectorx_core
    src/core/vector_frame.c
    src/game/flight.c
    src/game/weapons.c
    src/game/scene.c
)
target_include_directories(vectorx_core PUBLIC src)
if(NOT WIN32)
    target_link_libraries(vectorx_core PUBLIC m)
endif()
if(MSVC)
    target_compile_options(vectorx_core PRIVATE /W4)
else()
    target_compile_options(vectorx_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(VECTORX_BUILD_GAME)
    find_package(SDL3 3.2 QUIET CONFIG)
    if(NOT SDL3_FOUND)
        include(FetchContent)
        set(SDL_SHARED ON CACHE BOOL "" FORCE)
        set(SDL_STATIC OFF CACHE BOOL "" FORCE)
        set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
        set(SDL_TESTS OFF CACHE BOOL "" FORCE)
        set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(SDL3
            URL https://www.libsdl.org/release/SDL3-3.4.6.tar.gz
            URL_HASH SHA256=96bdd3f0c442105c87c287d213858b2df8207c87d54991dd589078c3746cb386
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        FetchContent_MakeAvailable(SDL3)
    endif()
    find_package(OpenGL REQUIRED)
    add_library(vectorx_vorbis STATIC third_party/stb/stb_vorbis.c)
    target_include_directories(vectorx_vorbis SYSTEM PUBLIC third_party/stb)
    target_compile_definitions(vectorx_vorbis PUBLIC STB_VORBIS_NO_PUSHDATA_API STB_VORBIS_NO_INTEGER_CONVERSION)
    if(NOT WIN32)
        target_link_libraries(vectorx_vorbis PUBLIC m)
    endif()
    add_executable(vectorx src/main.c src/platform/gl_renderer.c src/platform/music.c src/platform/track_menu.c)
    target_link_libraries(vectorx PRIVATE vectorx_core vectorx_vorbis SDL3::SDL3 OpenGL::GL)
    add_custom_target(vectorx_assets
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/bgm" "$<TARGET_FILE_DIR:vectorx>/bgm"
        VERBATIM)
    add_dependencies(vectorx vectorx_assets)
    if(MSVC)
        target_compile_options(vectorx PRIVATE /W4)
    else()
        target_compile_options(vectorx PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    if(WIN32 AND TARGET SDL3::SDL3-shared)
        # Keep DLLs beside the executable so it can be launched from Explorer.
        add_custom_command(TARGET vectorx POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:SDL3::SDL3-shared> $<TARGET_FILE_DIR:vectorx>
            VERBATIM
        )
        if(MINGW)
            target_link_options(vectorx PRIVATE -static-libgcc)
            # SDL's Windows support can depend on MinGW runtime DLLs even though
            # the application itself is C. Make launching independent of PATH.
            get_filename_component(VECTORX_COMPILER_DIRECTORY "${CMAKE_C_COMPILER}" DIRECTORY)
            foreach(VECTORX_RUNTIME libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll)
                execute_process(COMMAND ${CMAKE_C_COMPILER} -print-file-name=${VECTORX_RUNTIME}
                    OUTPUT_VARIABLE VECTORX_RUNTIME_PATH OUTPUT_STRIP_TRAILING_WHITESPACE)
                if(NOT EXISTS "${VECTORX_RUNTIME_PATH}")
                    set(VECTORX_RUNTIME_PATH "${VECTORX_COMPILER_DIRECTORY}/${VECTORX_RUNTIME}")
                endif()
                if(EXISTS "${VECTORX_RUNTIME_PATH}")
                    add_custom_command(TARGET vectorx POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                            "${VECTORX_RUNTIME_PATH}" $<TARGET_FILE_DIR:vectorx> VERBATIM)
                endif()
            endforeach()
        endif()
    endif()
endif()

if(BUILD_TESTING)
    add_executable(vectorx_core_tests tests/core_tests.c)
    target_link_libraries(vectorx_core_tests PRIVATE vectorx_core)
    if(MINGW)
        target_link_options(vectorx_core_tests PRIVATE -static-libgcc -static)
    endif()
    add_test(NAME core COMMAND vectorx_core_tests)
    if(VECTORX_BUILD_GAME)
        add_executable(vectorx_music_tests tests/music_tests.c src/platform/music.c src/platform/track_menu.c)
        target_include_directories(vectorx_music_tests PRIVATE src)
        target_link_libraries(vectorx_music_tests PRIVATE vectorx_core vectorx_vorbis SDL3::SDL3)
        add_dependencies(vectorx_music_tests vectorx)
        if(MINGW)
            target_link_options(vectorx_music_tests PRIVATE -static-libgcc)
        endif()
        add_test(NAME music COMMAND vectorx_music_tests)
    endif()
endif()
