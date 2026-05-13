cmake_minimum_required(VERSION 3.20)

project(Glyph LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(SDL2 QUIET sdl2)
  pkg_check_modules(SDL2_IMAGE QUIET SDL2_image)
  pkg_check_modules(SDL2_TTF QUIET SDL2_ttf)
  pkg_check_modules(SDL2_MIXER QUIET SDL2_mixer)
endif()

add_library(glyph_script
  src/core/StringInterner.cpp
  src/script/Lexer.cpp
  src/script/Parser.cpp
  src/script/Value.cpp
  src/script/Env.cpp
  src/script/Native.cpp
  src/script/VM.cpp
  src/assets/AssetManager.cpp
  src/audio/AudioSystem.cpp
  src/input/InputSystem.cpp
  src/render/RenderCompiler.cpp
  src/render/Renderer.cpp
  src/game/GameHost.cpp
  src/runtime/RuntimeShell.cpp
)

target_include_directories(glyph_script PUBLIC src)

if(MSVC)
  target_compile_options(glyph_script PRIVATE /W4)
else()
  target_compile_options(glyph_script PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(glyph src/app/Main.cpp)
target_link_libraries(glyph PRIVATE glyph_script)

if(SDL2_FOUND)
  target_sources(glyph PRIVATE src/platform/SDLDesktopApp.cpp)
  target_compile_definitions(glyph PRIVATE GLYPH_HAS_SDL=1)
  target_include_directories(glyph PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIRS}
    ${SDL2_TTF_INCLUDE_DIRS}
    ${SDL2_MIXER_INCLUDE_DIRS}
  )
  target_link_directories(glyph PRIVATE
    ${SDL2_LIBRARY_DIRS}
    ${SDL2_IMAGE_LIBRARY_DIRS}
    ${SDL2_TTF_LIBRARY_DIRS}
    ${SDL2_MIXER_LIBRARY_DIRS}
  )
  target_link_libraries(glyph PRIVATE
    ${SDL2_LIBRARIES}
    ${SDL2_IMAGE_LIBRARIES}
    ${SDL2_TTF_LIBRARIES}
    ${SDL2_MIXER_LIBRARIES}
  )
  target_compile_options(glyph PRIVATE
    ${SDL2_CFLAGS_OTHER}
    ${SDL2_IMAGE_CFLAGS_OTHER}
    ${SDL2_TTF_CFLAGS_OTHER}
    ${SDL2_MIXER_CFLAGS_OTHER}
  )
  if(SDL2_IMAGE_FOUND)
    target_compile_definitions(glyph PRIVATE GLYPH_HAS_SDL_IMAGE=1)
  else()
    target_compile_definitions(glyph PRIVATE GLYPH_HAS_SDL_IMAGE=0)
  endif()
  if(SDL2_TTF_FOUND)
    target_compile_definitions(glyph PRIVATE GLYPH_HAS_SDL_TTF=1)
  else()
    target_compile_definitions(glyph PRIVATE GLYPH_HAS_SDL_TTF=0)
  endif()
  if(SDL2_MIXER_FOUND)
    target_compile_definitions(glyph PRIVATE GLYPH_HAS_SDL_MIXER=1)
  else()
    target_compile_definitions(glyph PRIVATE GLYPH_HAS_SDL_MIXER=0)
  endif()
else()
  target_compile_definitions(glyph PRIVATE GLYPH_HAS_SDL=0)
endif()

add_executable(glyph_script_tests
  tests/script/ScriptCoreTests.cpp
)
target_link_libraries(glyph_script_tests PRIVATE glyph_script)

add_executable(glyph_render_tests
  tests/render/RenderCompilerTests.cpp
)
target_link_libraries(glyph_render_tests PRIVATE glyph_script)

add_executable(glyph_game_tests
  tests/game/GameHostTests.cpp
)
target_link_libraries(glyph_game_tests PRIVATE glyph_script)

add_executable(glyph_runtime_tests
  tests/runtime/RuntimeShellTests.cpp
)
target_link_libraries(glyph_runtime_tests PRIVATE glyph_script)

enable_testing()
add_test(NAME glyph_script_tests COMMAND glyph_script_tests)
add_test(NAME glyph_render_tests COMMAND glyph_render_tests)
add_test(NAME glyph_game_tests COMMAND glyph_game_tests)
add_test(NAME glyph_runtime_tests COMMAND glyph_runtime_tests)
