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

enable_testing()
add_test(NAME glyph_script_tests COMMAND glyph_script_tests)
