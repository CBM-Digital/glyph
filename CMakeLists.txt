cmake_minimum_required(VERSION 3.20)

project(Glyph LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
)

target_include_directories(glyph_script PUBLIC src)

if(MSVC)
  target_compile_options(glyph_script PRIVATE /W4)
else()
  target_compile_options(glyph_script PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(glyph src/app/Main.cpp)
target_link_libraries(glyph PRIVATE glyph_script)

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

enable_testing()
add_test(NAME glyph_script_tests COMMAND glyph_script_tests)
add_test(NAME glyph_render_tests COMMAND glyph_render_tests)
add_test(NAME glyph_game_tests COMMAND glyph_game_tests)
