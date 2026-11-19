cmake_minimum_required(VERSION 3.20)
project(open_world_zombie_waves C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Fetch dependencies into ONE shared, persistent cache instead of per build dir.
# Every build dir used to grow its own ~1.6 GB _deps tree (SDL3, SDL_ttf,
# SDL_image, ENet); pointing every configure at the same base dir fetches and
# compiles them once and reuses them across every variant and worktree. It also
# keeps them out of /tmp, which on this machine is a RAM-backed tmpfs with a
# per-user quota that a CMake _deps tree can exhaust.
#
# An explicit -D FETCHCONTENT_BASE_DIR=... still wins; this only supplies the
# default for a fresh cache. The populated cache is bound to one generator, so
# configure with -G Ninja unless you point at a cache populated by another one.
option(BUILD_ASAN "Build with AddressSanitizer" OFF)
option(BUILD_UBSAN "Build with UndefinedBehaviorSanitizer" OFF)
# Sanitizer configurations must NOT share the dependency cache: ASan/UBSan
# recompile the FetchContent deps with different flags, which flag-flips that
# cache for every other worktree until a normal rebuild heals it (and has
# corrupted the SDL3 precompiled header). Give them a private base dir inside
# their own build tree, unless the user explicitly chose a non-default dir.
if(BUILD_ASAN OR BUILD_UBSAN)
    if(NOT FETCHCONTENT_BASE_DIR OR
       FETCHCONTENT_BASE_DIR STREQUAL "$ENV{HOME}/.cache/cmake-deps")
        set(FETCHCONTENT_BASE_DIR "${CMAKE_BINARY_DIR}/_deps" CACHE PATH
            "Private FetchContent tree for sanitizer builds" FORCE)
    endif()
elseif(NOT FETCHCONTENT_BASE_DIR AND DEFINED ENV{HOME})
    set(FETCHCONTENT_BASE_DIR "$ENV{HOME}/.cache/cmake-deps" CACHE PATH
        "Shared FetchContent cache (downloads + builds), reused across build dirs")
endif()

include(FetchContent)

FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
)
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SDL3)

FetchContent_Declare(
    SDL_ttf
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
)
set(SDLTTF_SAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SDL_ttf)

# ENet (v1.3.18): UDP transport for the multiplayer layer. Shared by both the
# game core (NetServer/NetClient) and the standalone spike. Pulled before the
# core library so its include dir can be propagated to every consumer.
FetchContent_Declare(
    ENet
    GIT_REPOSITORY https://github.com/lsalzman/enet.git
    GIT_TAG        v1.3.18
    GIT_SHALLOW    TRUE
)
set(ENET_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ENET_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ENET_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ENET_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(ENET_BUILTIN_NAT OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(ENet)
# SDL_image is used for exactly one thing: IMG_LoadTexture() on our own PNGs
# (src/assets/asset_manager.c:85). Left at its defaults it is wildly more than
# that, and on Windows it was fatal:
#
# FetchContent clones submodules by default, and SDL_image's submodule tree
# vendors aom, dav1d, libavif, libjxl (which itself pulls brotli, highway,
# lcms, libpng, sjpeg, skcms, zlib and a testdata repo), libtiff and libwebp.
# That is gigabytes of AV1 and JPEG-XL codec source for a game that loads flat
# RGBA sprites, and the nested paths (external/libjxl/third_party/...) blow
# past Windows' MAX_PATH, so the populate step died with MSB8066 / exit -1 and
# the Windows configure never completed at all.
#
# GIT_SUBMODULES "" fetches none of them, and the format options below keep
# every decoder that would need an external library switched off.
# SDLIMAGE_PNG_LIBPNG OFF selects SDL_image's built-in PNG decoder instead of
# vendored libpng, which is what makes the submodule-free build self-sufficient.
# This is not a Windows-only workaround: it cuts the same waste on Linux, and
# PNG decoding of our generated sprites is unchanged either way.
FetchContent_Declare(
    SDL_image
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
    GIT_SUBMODULES ""
)
set(SDLIMAGE_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_TESTS OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_INSTALL OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_VENDORED OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_PNG_LIBPNG OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_AVIF OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_JXL OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_TIF OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_WEBP OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SDL_image)

# The documented gate ("clean Release build with -Werror, zero warnings in
# project sources") was not actually enforced: no warning flags were enabled
# anywhere in this file, so the build had been "clean" while saying nothing.
# Applied to project targets only - SDL3 is fetched from a third-party repo and
# has its own (many) unused parameters and sign-compare warnings.
# W1: the flag set must be compiler-aware. MSVC does not understand
# "-Wextra" at all - cl reports "Command line error D8021: invalid numeric
# argument '/Wextra'" and exits before compiling a single file, so on Windows
# this line did not weaken the gate, it broke every project target. MSVC also
# maps "-Wall" to its own /Wall, which is the whole-system-header firehose
# rather than GCC's -Wall. Keep the GCC/Clang flags byte-for-byte identical so
# Linux behaviour cannot move, and give MSVC the equivalent intent: /W4 /WX,
# a real zero-warning gate.
if(MSVC)
  set(PROJECT_WARNINGS /W4 /WX
      # C4996: MSVC deprecates the ISO C names (snprintf/strdup/fopen) in
      # favour of its _s variants. The code is standard C; the "safe"
      # alternatives are MSVC-only and would be the non-portable choice.
      /wd4996
      # C4200: zero-sized array in a struct - a documented C99 flexible array
      # member, used deliberately in the wire structs.
      /wd4200)
  # The ISO names again, this time via the CRT's own deprecation macros, so
  # third-party headers stop pre-empting the diagnostic too.
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)
else()
  set(PROJECT_WARNINGS -Wall -Wextra -Werror)
endif()

# N2: sanitizer flags. They must be added BEFORE the project targets are
# created - add_compile_options/add_link_options only affect targets created
# afterwards - and AFTER the FetchContent deps, so only project code is
# instrumented. (Declared at the end of this file they applied to no target at
# all: BUILD_ASAN configured a build with no -fsanitize anywhere.)
if(BUILD_ASAN)
  add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address)
endif()
if(BUILD_UBSAN)
  add_compile_options(-fsanitize=undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=undefined)
endif()

# Core game logic library (everything except the entry point)
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS
     "src/core/*.c" "src/ecs/*.c" "src/graphics/*.c"
     "src/world/*.c" "src/systems/*.c" "src/ui/*.c" "src/items/*.c"
     "src/events/*.c" "src/ai/*.c" "src/weapons/*.c" "src/net/*.c"
     "src/audio/*.c" "src/players.c" "src/assets/*.c" "src/effects/*.c")
add_library(zombie_core STATIC ${CORE_SOURCES})
target_compile_options(zombie_core PRIVATE ${PROJECT_WARNINGS})
target_include_directories(zombie_core PUBLIC src "${enet_SOURCE_DIR}/include")
target_link_libraries(zombie_core PUBLIC SDL3::SDL3 SDL3_ttf::SDL3_ttf enet SDL3_image::SDL3_image)

add_executable(${PROJECT_NAME} src/main.c)
target_compile_options(${PROJECT_NAME} PRIVATE ${PROJECT_WARNINGS})
target_link_libraries(${PROJECT_NAME} PRIVATE zombie_core)

# Ship assets beside the binary so the game finds textures and maps
# regardless of the working directory it is launched from.
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets)

# Static tests (QA)
add_executable(zombie_tests tests/tests.c)
target_compile_options(zombie_tests PRIVATE ${PROJECT_WARNINGS})
target_link_libraries(zombie_tests PRIVATE zombie_core)
# Test scratch goes under the build tree, never /tmp (this machine's /tmp is a
# RAM-backed tmpfs with a per-user quota). See tests/tests.c test_tmp_path().
target_compile_definitions(zombie_tests PRIVATE
    TEST_TMP_DIR="${CMAKE_BINARY_DIR}/test-tmp")

# ---------------------------------------------------------------------------
# Networking spike: standalone ENet loopback connectivity + packet codec proof
# (see docs/MULTIPLAYER_PLAN.md). Independent of the game code on purpose so
# the transport layer is validated before the real net code is written.
# ---------------------------------------------------------------------------
option(ENABLE_NET_SPIKE "Build the ENet loopback connectivity/codec spike" ON)
if(ENABLE_NET_SPIKE)
    add_executable(zombie_net_spike tests/net_spike.c)
    target_compile_options(zombie_net_spike PRIVATE ${PROJECT_WARNINGS})
    target_link_libraries(zombie_net_spike PRIVATE enet)
    target_include_directories(zombie_net_spike PRIVATE "${enet_SOURCE_DIR}/include")

    add_test(NAME net_spike COMMAND zombie_net_spike 127.0.0.1 50910 1.5 3)

    # Loopback handshake integration test: real NetServer + 3 NetClients in one
    # process (join, roster broadcast, version-mismatch reject, disconnect).
    add_executable(zombie_net_test tests/net_test.c)
    target_compile_options(zombie_net_test PRIVATE ${PROJECT_WARNINGS})
    target_link_libraries(zombie_net_test PRIVATE zombie_core)
    add_test(NAME net_loopback COMMAND zombie_net_test)
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(zombie_core PUBLIC m)
endif()

enable_testing()
add_test(NAME core_tests COMMAND zombie_tests)
set_tests_properties(core_tests PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
