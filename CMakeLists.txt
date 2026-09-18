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

FetchContent_Declare(
    SDL_image
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
)
set(SDLIMAGE_SAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SDL_image)

# Core game logic library (everything except the entry point)
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS
     "src/core/*.c" "src/ecs/*.c" "src/graphics/*.c"
     "src/world/*.c" "src/systems/*.c" "src/ui/*.c" "src/items/*.c"
     "src/events/*.c" "src/ai/*.c" "src/weapons/*.c" "src/assets/*.c")
add_library(zombie_core STATIC ${CORE_SOURCES})
target_include_directories(zombie_core PUBLIC src)
target_link_libraries(zombie_core PUBLIC SDL3::SDL3 SDL3_ttf::SDL3_ttf SDL3_image::SDL3_image)

add_executable(${PROJECT_NAME} src/main.c)
target_link_libraries(${PROJECT_NAME} PRIVATE zombie_core)

# Ship assets beside the binary so the game finds textures and maps
# regardless of the working directory it is launched from.
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets)

# Static tests (QA)
add_executable(zombie_tests tests/tests.c)
target_link_libraries(zombie_tests PRIVATE zombie_core)

if(UNIX AND NOT APPLE)
    target_link_libraries(zombie_core PUBLIC m)
endif()

enable_testing()
add_test(NAME core_tests COMMAND zombie_tests)
set_tests_properties(core_tests PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")