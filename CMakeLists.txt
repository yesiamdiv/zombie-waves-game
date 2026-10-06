cmake_minimum_required(VERSION 3.20)
project(open_world_zombie_waves C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
FetchContent_Declare(
    SDL_image
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
)
set(SDLIMAGE_SAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SDL_image)

# The documented gate ("clean Release build with -Werror, zero warnings in
# project sources") was not actually enforced: no warning flags were enabled
# anywhere in this file, so the build had been "clean" while saying nothing.
# Applied to project targets only - SDL3 is fetched from a third-party repo and
# has its own (many) unused parameters and sign-compare warnings.
set(PROJECT_WARNINGS -Wall -Wextra -Werror)

# Core game logic library (everything except the entry point)
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS
     "src/core/*.c" "src/ecs/*.c" "src/graphics/*.c"
     "src/world/*.c" "src/systems/*.c" "src/ui/*.c" "src/items/*.c"
     "src/events/*.c" "src/ai/*.c" "src/weapons/*.c" "src/net/*.c"
     "src/players.c" "src/assets/*.c")
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