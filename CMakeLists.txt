# Wacky Races - native Linux port build system
#
# This project is a native x86-64 Linux port of the 2000 PC game Wacky Races.
# It reads the original game assets directly (WGM, SND, AVI, BMP, TGAF, ...)
# using X11 and Vulkan 1.0. No Win32 calls, no PE execution, no Wine.

cmake_minimum_required(VERSION 3.16)
project(wackyraces LANGUAGES C CXX ASM)
include(CTest)

set(CMAKE_C_EXTENSIONS OFF)

# ----------------------------------------------------------------
# Platform selection
# ----------------------------------------------------------------
option(WPC_NATIVE_PORT "Build the native Linux port instead of the matching decompilation" ON)

if(WPC_NATIVE_PORT AND NOT UNIX)
    message(FATAL_ERROR "The native port currently targets Linux/Unix only.")
endif()

# ----------------------------------------------------------------
# C++ replacements for i386-only recovered routines (native port only)
# ----------------------------------------------------------------
set(WPC_PORT_CRT_SRC
    src/port/crt_memmove.cpp
    src/port/cxx_throw.cpp
    src/port/crt_entry.cpp
    src/port/wpc_affine_stubs.cpp
    src/port/wpc_vector_stubs.cpp
    src/port/wpc_empty_stubs.cpp
    src/port/wpc_field_readers.cpp
    src/port/wpc_game_heap_delete.cpp
    src/port/wpc_tga.cpp
    src/port/wpc_exception_metadata.cpp
    src/port/wpc_type_info.cpp
    src/port/wpc_vptr_stores.cpp
    src/port/wpc_vtable_stubs.cpp
)

# ----------------------------------------------------------------
# Native port sources (Linux platform layer)
# ----------------------------------------------------------------
set(WPC_PORT_SRC
    src/ima.cpp
    src/ima_tables.cpp
    src/port/game_assets.cpp
    src/port/game_scene.cpp
    src/port/game_mesh.cpp
    src/port/game_motion.cpp
    src/port/race_effects.cpp
    src/port/race_weapons.cpp
    src/port/race_robot.cpp
    src/port/game_material.cpp
    src/port/game_files.cpp
    src/port/game_world.cpp
    src/port/game_mode.cpp
    src/port/game_race.cpp
    src/port/game_ground.cpp
    src/port/game_drive.cpp
    src/port/race_track.cpp
    src/port/race_objects.cpp
    src/port/race_session.cpp
    src/port/race_camera.cpp
    src/port/race_view.cpp
    src/port/game_exe.cpp
    src/port/world_view.cpp
    src/port/game_avi.cpp
    src/port/movie_decoder.cpp
    src/port/movie_player.cpp
    src/port/game_sound.cpp
    src/port/audio_mixer.cpp
    src/port/audio_output.cpp
    src/port/game_audio.cpp
    src/port/voice_table.cpp
    src/port/race_audio.cpp
    src/port/game_config.cpp
    src/port/menu_animation.cpp
    src/port/menu_controller.cpp
    src/port/game_profile.cpp
    src/port/game_replay.cpp
    src/port/winner_scene.cpp
    src/port/menu_models.cpp
    src/port/menu_view.cpp
    src/port/linux_main.cpp
    src/port/linux_platform.cpp
    src/port/linux_joystick.cpp
    src/port/linux_platform.h
    src/port/wpc_platform.h
    src/port/wpc_iat_stubs.h
    src/port/wpc_iat_stubs.cpp
    src/port/wpc_stub_types.h
)

# ----------------------------------------------------------------
# Include directories
# ----------------------------------------------------------------
include_directories(src)
include_directories(src/port)
include_directories(src/port/vulkan/include)

# ----------------------------------------------------------------
# Target: wackyraces (native port)
# ----------------------------------------------------------------
if(WPC_NATIVE_PORT)
    add_executable(wackyraces
        ${WPC_PORT_CRT_SRC}
        ${WPC_PORT_SRC}
    )

    target_compile_definitions(wackyraces PRIVATE
        WPC_NATIVE_PORT=1
        WPC_PLATFORM_LINUX=1
    )

    target_compile_options(wackyraces PRIVATE
        -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers
        -Wno-narrowing -Wno-error=narrowing
        -O2 -g
    )

    target_link_libraries(wackyraces PRIVATE
        X11
        Xext
        vulkan
        m
        dl
        pthread
    )

    # Optional: libavcodec decodes the Indeo 5 (IV50) movie bitstream. Without
    # it the startup and intro movies are skipped with a log message.
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(WPC_LIBAVCODEC QUIET IMPORTED_TARGET libavcodec libavutil)
    endif()
    # Optional: PulseAudio's simple API (also served by PipeWire) for sound
    # output. Without it the mixer runs silently.
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(WPC_PULSE QUIET IMPORTED_TARGET libpulse-simple)
    endif()
    if(WPC_PULSE_FOUND)
        target_compile_definitions(wackyraces PRIVATE WPC_HAVE_PULSE=1)
        target_link_libraries(wackyraces PRIVATE PkgConfig::WPC_PULSE)
        message(STATUS "Audio: PulseAudio ${WPC_PULSE_VERSION}")
    else()
        message(STATUS "Audio: libpulse-simple not found; sound will be silent")
    endif()
    if(WPC_LIBAVCODEC_FOUND)
        target_compile_definitions(wackyraces PRIVATE WPC_HAVE_LIBAVCODEC=1)
        target_link_libraries(wackyraces PRIVATE PkgConfig::WPC_LIBAVCODEC)
        message(STATUS "Movies: Indeo 5 through libavcodec ${WPC_LIBAVCODEC_libavcodec_VERSION}")
    else()
        message(STATUS "Movies: libavcodec not found; Indeo 5 movies will be skipped")
    endif()

    if(BUILD_TESTING)
        add_executable(native_assets_test tests/native_assets.cpp src/port/game_assets.cpp)
        add_test(NAME native-assets COMMAND native_assets_test)
        add_executable(native_scene_test tests/native_scene.cpp src/port/game_scene.cpp src/port/game_assets.cpp)
        add_test(NAME native-scene COMMAND native_scene_test)
        add_executable(native_menu_test tests/native_menu.cpp src/port/menu_animation.cpp src/port/menu_controller.cpp src/port/game_profile.cpp src/port/game_exe.cpp src/port/game_replay.cpp src/port/game_drive.cpp src/port/game_files.cpp src/port/game_scene.cpp src/port/game_assets.cpp)
        add_test(NAME native-menu COMMAND native_menu_test)
        add_executable(native_mesh_test tests/native_mesh.cpp src/port/game_mesh.cpp src/port/game_material.cpp src/port/game_assets.cpp)
        add_test(NAME native-mesh COMMAND native_mesh_test)
        add_executable(native_motion_test tests/native_motion.cpp src/port/game_motion.cpp src/port/game_mesh.cpp
            src/port/game_material.cpp src/port/game_scene.cpp src/port/game_assets.cpp)
        add_test(NAME native-motion COMMAND native_motion_test)
        add_executable(native_material_test tests/native_material.cpp src/port/game_material.cpp src/port/game_assets.cpp)
        add_test(NAME native-material COMMAND native_material_test)
        add_executable(native_world_test tests/native_world.cpp src/port/game_world.cpp src/port/game_files.cpp
            src/port/game_mesh.cpp src/port/game_material.cpp src/port/game_scene.cpp src/port/game_assets.cpp)
        add_test(NAME native-world COMMAND native_world_test)
        add_executable(native_mode_test tests/native_mode.cpp src/port/game_mode.cpp src/port/game_scene.cpp src/port/game_assets.cpp)
        add_test(NAME native-mode COMMAND native_mode_test)
        add_executable(native_race_test tests/native_race.cpp src/port/game_race.cpp src/port/game_scene.cpp src/port/game_assets.cpp)
        add_test(NAME native-race COMMAND native_race_test)
        add_executable(native_drive_test tests/native_drive.cpp src/port/game_drive.cpp src/port/race_session.cpp src/port/race_camera.cpp src/port/race_effects.cpp src/port/race_weapons.cpp src/port/race_robot.cpp
            src/port/race_track.cpp src/port/race_objects.cpp src/port/game_race.cpp src/port/game_ground.cpp src/port/game_mode.cpp
            src/port/game_scene.cpp src/port/game_assets.cpp src/port/game_mesh.cpp src/port/game_material.cpp src/port/game_motion.cpp)
        add_test(NAME native-drive COMMAND native_drive_test)
        add_executable(native_config_test tests/native_config.cpp src/port/game_config.cpp src/port/game_exe.cpp src/port/linux_joystick.cpp)
        add_test(NAME native-config COMMAND native_config_test)
        add_executable(native_movie_test tests/native_movie.cpp src/port/game_avi.cpp src/port/movie_decoder.cpp)
        if(WPC_LIBAVCODEC_FOUND)
            target_compile_definitions(native_movie_test PRIVATE WPC_HAVE_LIBAVCODEC=1)
            target_link_libraries(native_movie_test PRIVATE PkgConfig::WPC_LIBAVCODEC)
        endif()
        add_test(NAME native-movie COMMAND native_movie_test)
        add_executable(native_audio_test tests/native_audio.cpp src/port/audio_mixer.cpp src/port/audio_output.cpp
            src/port/game_sound.cpp src/port/game_audio.cpp src/port/voice_table.cpp src/port/game_avi.cpp
            src/port/game_files.cpp src/port/game_scene.cpp src/port/game_assets.cpp src/ima.cpp src/ima_tables.cpp)
        target_link_libraries(native_audio_test PRIVATE pthread)
        add_test(NAME native-audio COMMAND native_audio_test)
        add_executable(native_vehicle_test tests/native_vehicle.cpp src/port/menu_models.cpp src/port/game_motion.cpp
            src/port/game_world.cpp src/port/game_mesh.cpp src/port/game_material.cpp src/port/game_scene.cpp
            src/port/game_assets.cpp src/port/linux_platform.cpp src/port/menu_controller.cpp src/port/game_profile.cpp
            src/port/game_exe.cpp src/port/game_replay.cpp src/port/game_drive.cpp src/port/game_files.cpp)
        target_link_libraries(native_vehicle_test PRIVATE X11 vulkan pthread)
        add_test(NAME native-vehicle COMMAND native_vehicle_test)
        add_executable(native_render_test tests/native_render.cpp src/port/linux_platform.cpp)
        target_link_libraries(native_render_test PRIVATE X11 vulkan pthread)
        target_compile_options(native_render_test PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
        option(WPC_RENDER_TESTS "Enable presentation tests (requires an X11 display and Vulkan)" OFF)
        if(WPC_RENDER_TESTS)
            add_test(NAME native-render COMMAND native_render_test)
        endif()
    endif()
endif()
