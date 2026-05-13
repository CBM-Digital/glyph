cmake_minimum_required(VERSION 3.20)

project(Glyph LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  set(GLYPH_TARGET_IOS ON)
else()
  set(GLYPH_TARGET_IOS OFF)
endif()

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
  src/assets/AssetSource.cpp
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

if(NOT GLYPH_TARGET_IOS)
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
endif()

if(GLYPH_TARGET_IOS)
  set(GLYPH_IOS_FRAMEWORK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ios/Frameworks" CACHE PATH
      "Directory containing SDL2 iOS .framework or .xcframework bundles.")
  set(GLYPH_IOS_SDL2_ROOT "" CACHE PATH
      "Optional SDL2 iOS distribution root. Checked before common repository locations.")
  set(GLYPH_IOS_SDL2_IMAGE_ROOT "" CACHE PATH
      "Optional SDL2_image iOS distribution root. Checked before common repository locations.")

  function(glyph_resolve_ios_framework out_var framework_name)
    set(search_roots)
    if(framework_name STREQUAL "SDL2_image" AND GLYPH_IOS_SDL2_IMAGE_ROOT)
      list(APPEND search_roots
        "${GLYPH_IOS_SDL2_IMAGE_ROOT}"
        "${GLYPH_IOS_SDL2_IMAGE_ROOT}/lib"
        "${GLYPH_IOS_SDL2_IMAGE_ROOT}/Frameworks"
      )
    elseif(GLYPH_IOS_SDL2_ROOT)
      list(APPEND search_roots
        "${GLYPH_IOS_SDL2_ROOT}"
        "${GLYPH_IOS_SDL2_ROOT}/lib"
        "${GLYPH_IOS_SDL2_ROOT}/Frameworks"
      )
    endif()
    list(APPEND search_roots
      "${GLYPH_IOS_FRAMEWORK_DIR}"
      "${CMAKE_CURRENT_SOURCE_DIR}/third_party"
      "${CMAKE_CURRENT_SOURCE_DIR}/vendor"
    )

    if(CMAKE_OSX_SYSROOT MATCHES "simulator")
      set(xc_slices ios-arm64_x86_64-simulator ios-arm64-simulator ios-x86_64-simulator)
    else()
      set(xc_slices ios-arm64)
    endif()

    foreach(root IN LISTS search_roots)
      if(EXISTS "${root}/${framework_name}.framework")
        set(${out_var} "${root}/${framework_name}.framework" PARENT_SCOPE)
        return()
      endif()

      foreach(xcframework
          "${root}/${framework_name}.xcframework"
          "${root}/${framework_name}/${framework_name}.xcframework")
        if(EXISTS "${xcframework}")
          foreach(slice IN LISTS xc_slices)
            if(EXISTS "${xcframework}/${slice}/${framework_name}.framework")
              set(${out_var} "${xcframework}/${slice}/${framework_name}.framework" PARENT_SCOPE)
              return()
            endif()
          endforeach()
        endif()
      endforeach()
    endforeach()

    set(${out_var} "" PARENT_SCOPE)
  endfunction()

  glyph_resolve_ios_framework(GLYPH_IOS_SDL2_FRAMEWORK SDL2)
  glyph_resolve_ios_framework(GLYPH_IOS_SDL2_IMAGE_FRAMEWORK SDL2_image)

  if(NOT GLYPH_IOS_SDL2_FRAMEWORK)
    message(FATAL_ERROR
      "SDL2 iOS framework not found. Put SDL2.framework or SDL2.xcframework under "
      "ios/Frameworks, or configure with -DGLYPH_IOS_SDL2_ROOT=/path/to/SDL2.")
  endif()
  if(NOT GLYPH_IOS_SDL2_IMAGE_FRAMEWORK)
    message(FATAL_ERROR
      "SDL2_image iOS framework not found. The bundled arcade sample uses PNG assets. Put "
      "SDL2_image.framework or SDL2_image.xcframework under ios/Frameworks, or configure "
      "with -DGLYPH_IOS_SDL2_IMAGE_ROOT=/path/to/SDL2_image.")
  endif()

  file(GLOB_RECURSE GLYPH_IOS_ARCADE_RESOURCE_CANDIDATES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/arcade/*"
  )
  set(GLYPH_IOS_ARCADE_RESOURCES)
  foreach(resource IN LISTS GLYPH_IOS_ARCADE_RESOURCE_CANDIDATES)
    if(IS_DIRECTORY "${resource}")
      continue()
    endif()
    get_filename_component(resource_name "${resource}" NAME)
    if(resource_name MATCHES "^\\.")
      continue()
    endif()

    file(RELATIVE_PATH resource_relative "${CMAKE_CURRENT_SOURCE_DIR}/examples/arcade" "${resource}")
    get_filename_component(resource_directory "${resource_relative}" DIRECTORY)
    if(resource_directory STREQUAL "")
      set(resource_location "Resources/examples/arcade")
    else()
      set(resource_location "Resources/examples/arcade/${resource_directory}")
    endif()
    set_source_files_properties("${resource}" PROPERTIES MACOSX_PACKAGE_LOCATION "${resource_location}")
    list(APPEND GLYPH_IOS_ARCADE_RESOURCES "${resource}")
  endforeach()

  add_executable(glyph_ios MACOSX_BUNDLE
    src/platform/IOSApp.cpp
    src/platform/SDLDesktopApp.cpp
    ${GLYPH_IOS_ARCADE_RESOURCES}
  )
  target_link_libraries(glyph_ios PRIVATE glyph_script)
  target_include_directories(glyph_ios PRIVATE
    "${GLYPH_IOS_SDL2_FRAMEWORK}/Headers"
    "${GLYPH_IOS_SDL2_IMAGE_FRAMEWORK}/Headers"
  )
  target_compile_definitions(glyph_ios PRIVATE
    GLYPH_HAS_SDL=1
    GLYPH_HAS_SDL_IMAGE=1
    GLYPH_HAS_SDL_TTF=0
    GLYPH_HAS_SDL_MIXER=0
  )
  target_link_libraries(glyph_ios PRIVATE
    "${GLYPH_IOS_SDL2_FRAMEWORK}"
    "${GLYPH_IOS_SDL2_IMAGE_FRAMEWORK}"
    "-framework AudioToolbox"
    "-framework AVFoundation"
    "-framework CoreAudio"
    "-framework CoreGraphics"
    "-framework CoreMotion"
    "-framework Foundation"
    "-framework GameController"
    "-framework ImageIO"
    "-framework Metal"
    "-framework OpenGLES"
    "-framework QuartzCore"
    "-framework UIKit"
  )
  set_target_properties(glyph_ios PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/ios/Info.plist.in"
    XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
    XCODE_ATTRIBUTE_ENABLE_BITCODE "NO"
    XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "13.0"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.glyph.runtime"
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
    XCODE_EMBED_FRAMEWORKS "${GLYPH_IOS_SDL2_FRAMEWORK};${GLYPH_IOS_SDL2_IMAGE_FRAMEWORK}"
    XCODE_EMBED_FRAMEWORKS_CODE_SIGN_ON_COPY TRUE
  )
endif()
