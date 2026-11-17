cmake_minimum_required(VERSION 3.24)
project(goodmaxton LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# --- SDL3 : téléchargé et compilé en statique (aucun paquet système requis) ---
include(FetchContent)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-3.4.20
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(SDL3)

# --- Jeu ---
add_executable(goodmaxton
  src/main.c
  src/render.c
  src/draw.c
  src/game.c
)
target_include_directories(goodmaxton PRIVATE src)
target_compile_options(goodmaxton PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(goodmaxton PRIVATE SDL3::SDL3 m)
