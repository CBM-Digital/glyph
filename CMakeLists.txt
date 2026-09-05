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
if(CMAKE_SYSTEM_NAME STREQUAL "Android")
  set(GLYPH_TARGET_ANDROID ON)
else()
  set(GLYPH_TARGET_ANDROID OFF)
endif()

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(SDL2 QUIET sdl2)
  pkg_check_modules(SDL2_IMAGE QUIET SDL2_image)
  pkg_check_modules(SDL2_TTF QUIET SDL2_ttf)
  pkg_check_modules(SDL2_MIXER QUIET SDL2_mixer)
endif()

option(GLYPH_REQUIRE_MEDIA "Fail configuration if a production media backend is missing" OFF)
if(NOT GLYPH_TARGET_IOS AND NOT GLYPH_TARGET_ANDROID)
  # vcpkg and native Windows installations expose CMake targets, not pkg-config.
  foreach(component SDL2 SDL2_image SDL2_ttf SDL2_mixer)
    find_package(${component} CONFIG QUIET)
  endforeach()
  if(NOT SDL2_FOUND AND TARGET SDL2::SDL2)
    set(SDL2_FOUND TRUE)
    set(SDL2_LIBRARIES SDL2::SDL2)
  endif()
  foreach(pair "IMAGE|image" "TTF|ttf" "MIXER|mixer")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 upper)
    list(GET parts 1 lower)
    if(NOT SDL2_${upper}_FOUND AND TARGET SDL2_${lower}::SDL2_${lower})
      set(SDL2_${upper}_FOUND TRUE)
      set(SDL2_${upper}_LIBRARIES SDL2_${lower}::SDL2_${lower})
    endif()
  endforeach()
  # SDL2's config sets SDL2_FOUND itself; ensure it also supplies the link target.
  if(SDL2_FOUND AND NOT SDL2_LIBRARIES AND TARGET SDL2::SDL2)
    set(SDL2_LIBRARIES SDL2::SDL2)
  endif()
  if(GLYPH_REQUIRE_MEDIA AND NOT (SDL2_FOUND AND SDL2_IMAGE_FOUND AND SDL2_TTF_FOUND AND SDL2_MIXER_FOUND))
    message(FATAL_ERROR "Production builds require SDL2, SDL2_image, SDL2_ttf and SDL2_mixer.")
  endif()
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
  src/profile/ProfileStore.cpp
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

if(NOT GLYPH_TARGET_IOS AND NOT GLYPH_TARGET_ANDROID)
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

  add_executable(glyph_visual_snapshot_tests
    tests/integration/PixelSnapshotTests.cpp
  )
  target_link_libraries(glyph_visual_snapshot_tests PRIVATE glyph_script)
  if(SDL2_FOUND AND SDL2_IMAGE_FOUND)
    target_compile_definitions(glyph_visual_snapshot_tests PRIVATE GLYPH_VISUAL_SNAPSHOT_HAS_SDL=1)
    target_include_directories(glyph_visual_snapshot_tests PRIVATE
      ${SDL2_INCLUDE_DIRS}
      ${SDL2_IMAGE_INCLUDE_DIRS}
      ${SDL2_TTF_INCLUDE_DIRS}
    )
    target_link_directories(glyph_visual_snapshot_tests PRIVATE
      ${SDL2_LIBRARY_DIRS}
      ${SDL2_IMAGE_LIBRARY_DIRS}
      ${SDL2_TTF_LIBRARY_DIRS}
    )
    target_link_libraries(glyph_visual_snapshot_tests PRIVATE
      ${SDL2_LIBRARIES}
      ${SDL2_IMAGE_LIBRARIES}
      ${SDL2_TTF_LIBRARIES}
    )
    target_compile_options(glyph_visual_snapshot_tests PRIVATE
      ${SDL2_CFLAGS_OTHER}
      ${SDL2_IMAGE_CFLAGS_OTHER}
      ${SDL2_TTF_CFLAGS_OTHER}
    )
    if(SDL2_TTF_FOUND)
      target_compile_definitions(glyph_visual_snapshot_tests PRIVATE GLYPH_VISUAL_SNAPSHOT_HAS_TTF=1)
    else()
      target_compile_definitions(glyph_visual_snapshot_tests PRIVATE GLYPH_VISUAL_SNAPSHOT_HAS_TTF=0)
    endif()
  else()
    target_compile_definitions(glyph_visual_snapshot_tests PRIVATE GLYPH_VISUAL_SNAPSHOT_HAS_SDL=0)
    target_compile_definitions(glyph_visual_snapshot_tests PRIVATE GLYPH_VISUAL_SNAPSHOT_HAS_TTF=0)
  endif()

  add_executable(glyph_runtime_tests
    tests/runtime/RuntimeShellTests.cpp
  )
  target_link_libraries(glyph_runtime_tests PRIVATE glyph_script)

  add_executable(glyph_arcade_tests tests/game/ArcadePlayTests.cpp)
  target_link_libraries(glyph_arcade_tests PRIVATE glyph_script)
  target_compile_definitions(glyph_arcade_tests PRIVATE GLYPH_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
  if(SDL2_FOUND)
    add_executable(glyph_platform_tests tests/platform/SDLInputTests.cpp)
    target_link_libraries(glyph_platform_tests PRIVATE glyph_script ${SDL2_LIBRARIES} ${SDL2_IMAGE_LIBRARIES} ${SDL2_TTF_LIBRARIES} ${SDL2_MIXER_LIBRARIES})
    target_include_directories(glyph_platform_tests PRIVATE $<TARGET_PROPERTY:glyph,INCLUDE_DIRECTORIES>)
    target_link_directories(glyph_platform_tests PRIVATE $<TARGET_PROPERTY:glyph,LINK_DIRECTORIES>)
    target_compile_definitions(glyph_platform_tests PRIVATE $<TARGET_PROPERTY:glyph,COMPILE_DEFINITIONS> GLYPH_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME glyph_platform_tests COMMAND glyph_platform_tests)
  endif()
  enable_testing()
  add_test(NAME glyph_arcade_tests COMMAND glyph_arcade_tests)
  add_test(NAME glyph_script_tests COMMAND glyph_script_tests)
  add_test(NAME glyph_render_tests COMMAND glyph_render_tests)
  add_test(NAME glyph_game_tests COMMAND glyph_game_tests)
  add_test(NAME glyph_visual_snapshot_tests COMMAND glyph_visual_snapshot_tests)
  add_test(NAME glyph_runtime_tests COMMAND glyph_runtime_tests)
endif()

if(GLYPH_TARGET_ANDROID)
  set(GLYPH_ANDROID_SDL2_ROOT "" CACHE PATH
      "SDL2 source checkout or Android distribution root used for the Android host.")
  set(GLYPH_ANDROID_SDL2_IMAGE_ROOT "" CACHE PATH
      "Optional SDL2_image source checkout or Android distribution root used for PNG/JPG assets.")

  if(NOT GLYPH_ANDROID_SDL2_ROOT AND DEFINED ENV{SDL2_ANDROID_ROOT})
    set(GLYPH_ANDROID_SDL2_ROOT "$ENV{SDL2_ANDROID_ROOT}")
  endif()
  if(NOT GLYPH_ANDROID_SDL2_ROOT AND DEFINED ENV{SDL2_ROOT})
    set(GLYPH_ANDROID_SDL2_ROOT "$ENV{SDL2_ROOT}")
  endif()
  if(NOT GLYPH_ANDROID_SDL2_IMAGE_ROOT AND DEFINED ENV{SDL2_IMAGE_ANDROID_ROOT})
    set(GLYPH_ANDROID_SDL2_IMAGE_ROOT "$ENV{SDL2_IMAGE_ANDROID_ROOT}")
  endif()
  if(NOT GLYPH_ANDROID_SDL2_IMAGE_ROOT AND DEFINED ENV{SDL2_IMAGE_ROOT})
    set(GLYPH_ANDROID_SDL2_IMAGE_ROOT "$ENV{SDL2_IMAGE_ROOT}")
  endif()

  function(glyph_import_android_sdl out_target root library_name)
    if(NOT root)
      set(${out_target} "" PARENT_SCOPE)
      return()
    endif()

    set(include_candidates
      "${root}/include"
      "${root}/include/SDL2"
      "${root}/android-project/app/jni/${library_name}/include"
      "${root}/android-project/app/jni/SDL/include"
    )
    set(include_dir "")
    foreach(candidate IN LISTS include_candidates)
      if(EXISTS "${candidate}/SDL.h" OR EXISTS "${candidate}/SDL_image.h" OR EXISTS "${candidate}/SDL_ttf.h" OR EXISTS "${candidate}/SDL_mixer.h")
        set(include_dir "${candidate}")
        break()
      endif()
    endforeach()

    set(library_candidates
      "${root}/lib/${ANDROID_ABI}/lib${library_name}.so"
      "${root}/lib/android/${ANDROID_ABI}/lib${library_name}.so"
      "${root}/android-project/app/src/main/jniLibs/${ANDROID_ABI}/lib${library_name}.so"
    )
    set(library_path "")
    foreach(candidate IN LISTS library_candidates)
      if(EXISTS "${candidate}")
        set(library_path "${candidate}")
        break()
      endif()
    endforeach()

    if(include_dir AND library_path)
      set(imported_target "${library_name}_android_imported")
      add_library(${imported_target} SHARED IMPORTED)
      set_target_properties(${imported_target} PROPERTIES
        IMPORTED_LOCATION "${library_path}"
        INTERFACE_INCLUDE_DIRECTORIES "${include_dir}"
      )
      set(${out_target} "${imported_target}" PARENT_SCOPE)
    else()
      set(${out_target} "" PARENT_SCOPE)
    endif()
  endfunction()

  if(NOT GLYPH_ANDROID_SDL2_ROOT)
    message(FATAL_ERROR
      "SDL2 Android root not configured. Set glyph.sdl2Root in platform/android/local.properties, "
      "or pass -DGLYPH_ANDROID_SDL2_ROOT=/path/to/SDL2.")
  endif()

  if(EXISTS "${GLYPH_ANDROID_SDL2_ROOT}/CMakeLists.txt")
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    add_subdirectory("${GLYPH_ANDROID_SDL2_ROOT}" "${CMAKE_BINARY_DIR}/SDL2" EXCLUDE_FROM_ALL)
    if(TARGET SDL2::SDL2)
      set(GLYPH_ANDROID_SDL2_TARGET SDL2::SDL2)
    elseif(TARGET SDL2)
      set(GLYPH_ANDROID_SDL2_TARGET SDL2)
    endif()
  endif()
  if(NOT GLYPH_ANDROID_SDL2_TARGET)
    glyph_import_android_sdl(GLYPH_ANDROID_SDL2_TARGET "${GLYPH_ANDROID_SDL2_ROOT}" SDL2)
  endif()
  if(NOT GLYPH_ANDROID_SDL2_TARGET)
    message(FATAL_ERROR
      "Unable to find or build SDL2 for Android from ${GLYPH_ANDROID_SDL2_ROOT}. "
      "Use an SDL2 source checkout with CMake support or an Android distribution containing "
      "include/ and lib/<abi>/libSDL2.so.")
  endif()

  set(GLYPH_ANDROID_SDL2_IMAGE_OPTIONS
    SDL2IMAGE_INSTALL
    SDL2IMAGE_SAMPLES
    SDL2IMAGE_TESTS
    SDL2IMAGE_AVIF
    SDL2IMAGE_GIF
    SDL2IMAGE_JXL
    SDL2IMAGE_LBM
    SDL2IMAGE_PCX
    SDL2IMAGE_PNM
    SDL2IMAGE_QOI
    SDL2IMAGE_SVG
    SDL2IMAGE_TGA
    SDL2IMAGE_TIF
    SDL2IMAGE_WEBP
    SDL2IMAGE_XCF
    SDL2IMAGE_XPM
    SDL2IMAGE_XV
  )
  foreach(option IN LISTS GLYPH_ANDROID_SDL2_IMAGE_OPTIONS)
    set(${option} OFF CACHE BOOL "" FORCE)
  endforeach()
  set(SDL2IMAGE_BACKEND_STB ON CACHE BOOL "" FORCE)
  set(SDL2IMAGE_PNG ON CACHE BOOL "" FORCE)
  set(SDL2IMAGE_JPG ON CACHE BOOL "" FORCE)

  set(GLYPH_ANDROID_HAS_SDL_IMAGE OFF)
  if(GLYPH_ANDROID_SDL2_IMAGE_ROOT)
    if(EXISTS "${GLYPH_ANDROID_SDL2_IMAGE_ROOT}/CMakeLists.txt")
      add_subdirectory("${GLYPH_ANDROID_SDL2_IMAGE_ROOT}" "${CMAKE_BINARY_DIR}/SDL2_image" EXCLUDE_FROM_ALL)
      if(TARGET SDL2_image::SDL2_image)
        set(GLYPH_ANDROID_SDL2_IMAGE_TARGET SDL2_image::SDL2_image)
      elseif(TARGET SDL2_image)
        set(GLYPH_ANDROID_SDL2_IMAGE_TARGET SDL2_image)
      endif()
    endif()
    if(NOT GLYPH_ANDROID_SDL2_IMAGE_TARGET)
      glyph_import_android_sdl(GLYPH_ANDROID_SDL2_IMAGE_TARGET "${GLYPH_ANDROID_SDL2_IMAGE_ROOT}" SDL2_image)
    endif()
    if(GLYPH_ANDROID_SDL2_IMAGE_TARGET)
      set(GLYPH_ANDROID_HAS_SDL_IMAGE ON)
    else()
      message(WARNING
        "SDL2_image Android root was set but no usable SDL2_image target/library was found; "
        "PNG/JPG textures will use fallback sprites.")
    endif()
  endif()

  foreach(component TTF MIXER)
    string(TOLOWER "${component}" lower)
    set(GLYPH_ANDROID_SDL2_${component}_ROOT "" CACHE PATH "SDL2_${lower} Android source or prebuilt root")
    set(root "${GLYPH_ANDROID_SDL2_${component}_ROOT}")
    if(root AND EXISTS "${root}/CMakeLists.txt")
      add_subdirectory("${root}" "${CMAKE_BINARY_DIR}/SDL2_${lower}" EXCLUDE_FROM_ALL)
      if(TARGET SDL2_${lower}::SDL2_${lower})
        set(GLYPH_ANDROID_SDL2_${component}_TARGET SDL2_${lower}::SDL2_${lower})
      elseif(TARGET SDL2_${lower})
        set(GLYPH_ANDROID_SDL2_${component}_TARGET SDL2_${lower})
      endif()
    endif()
    if(NOT GLYPH_ANDROID_SDL2_${component}_TARGET)
      glyph_import_android_sdl(GLYPH_ANDROID_SDL2_${component}_TARGET "${root}" SDL2_${lower})
    endif()
    set(GLYPH_ANDROID_HAS_SDL_${component} OFF)
    if(GLYPH_ANDROID_SDL2_${component}_TARGET)
      set(GLYPH_ANDROID_HAS_SDL_${component} ON)
    endif()
  endforeach()
  if(GLYPH_REQUIRE_MEDIA AND NOT (GLYPH_ANDROID_HAS_SDL_IMAGE AND GLYPH_ANDROID_HAS_SDL_TTF AND GLYPH_ANDROID_HAS_SDL_MIXER))
    message(FATAL_ERROR "Android production builds require PNG, TTF and mixer targets; configure all three dependency roots.")
  endif()

  add_library(glyph SHARED
    src/platform/AndroidApp.cpp
    src/platform/SDLDesktopApp.cpp
  )
  target_link_libraries(glyph PRIVATE
    glyph_script
    ${GLYPH_ANDROID_SDL2_TARGET}
  )
  if(GLYPH_ANDROID_HAS_SDL_IMAGE)
    target_link_libraries(glyph PRIVATE ${GLYPH_ANDROID_SDL2_IMAGE_TARGET})
  endif()
  foreach(component TTF MIXER)
    if(GLYPH_ANDROID_SDL2_${component}_TARGET)
      target_link_libraries(glyph PRIVATE ${GLYPH_ANDROID_SDL2_${component}_TARGET})
    endif()
  endforeach()
  target_compile_definitions(glyph PRIVATE
    GLYPH_HAS_SDL=1
    GLYPH_HAS_SDL_IMAGE=$<BOOL:${GLYPH_ANDROID_HAS_SDL_IMAGE}>
    GLYPH_HAS_SDL_TTF=$<BOOL:${GLYPH_ANDROID_HAS_SDL_TTF}>
    GLYPH_HAS_SDL_MIXER=$<BOOL:${GLYPH_ANDROID_HAS_SDL_MIXER}>
  )
endif()

if(GLYPH_TARGET_IOS)
  set(GLYPH_IOS_FRAMEWORK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ios/Frameworks" CACHE PATH
      "Directory containing SDL2 iOS .framework or .xcframework bundles.")
  set(GLYPH_IOS_SDL2_ROOT "" CACHE PATH
      "Optional SDL2 iOS distribution root. Checked before common repository locations.")
  set(GLYPH_IOS_SDL2_IMAGE_ROOT "" CACHE PATH
      "Optional SDL2_image iOS distribution root. Checked before common repository locations.")

  if(GLYPH_IOS_SDL2_ROOT AND EXISTS "${GLYPH_IOS_SDL2_ROOT}/CMakeLists.txt")
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
    add_subdirectory("${GLYPH_IOS_SDL2_ROOT}" "${CMAKE_BINARY_DIR}/SDL2-ios" EXCLUDE_FROM_ALL)
    if(TARGET SDL2::SDL2)
      set(GLYPH_IOS_SDL2_TARGET SDL2::SDL2)
    elseif(TARGET SDL2-static)
      set(GLYPH_IOS_SDL2_TARGET SDL2-static)
    elseif(TARGET SDL2)
      set(GLYPH_IOS_SDL2_TARGET SDL2)
    endif()
  endif()

  if(GLYPH_IOS_SDL2_IMAGE_ROOT AND EXISTS "${GLYPH_IOS_SDL2_IMAGE_ROOT}/CMakeLists.txt")
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(SDL2IMAGE_INSTALL OFF CACHE BOOL "" FORCE)
    set(SDL2IMAGE_SAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL2IMAGE_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL2IMAGE_BACKEND_STB ON CACHE BOOL "" FORCE)
    set(SDL2IMAGE_BACKEND_IMAGEIO OFF CACHE BOOL "" FORCE)
    set(SDL2IMAGE_PNG ON CACHE BOOL "" FORCE)
    set(SDL2IMAGE_JPG ON CACHE BOOL "" FORCE)
    set(GLYPH_IOS_SDL2_IMAGE_OPTIONS
      SDL2IMAGE_AVIF
      SDL2IMAGE_GIF
      SDL2IMAGE_JXL
      SDL2IMAGE_LBM
      SDL2IMAGE_PCX
      SDL2IMAGE_PNM
      SDL2IMAGE_QOI
      SDL2IMAGE_SVG
      SDL2IMAGE_TGA
      SDL2IMAGE_TIF
      SDL2IMAGE_WEBP
      SDL2IMAGE_XCF
      SDL2IMAGE_XPM
      SDL2IMAGE_XV
    )
    foreach(option IN LISTS GLYPH_IOS_SDL2_IMAGE_OPTIONS)
      set(${option} OFF CACHE BOOL "" FORCE)
    endforeach()
    add_subdirectory("${GLYPH_IOS_SDL2_IMAGE_ROOT}" "${CMAKE_BINARY_DIR}/SDL2_image-ios" EXCLUDE_FROM_ALL)
    if(TARGET SDL2_image::SDL2_image)
      set(GLYPH_IOS_SDL2_IMAGE_TARGET SDL2_image::SDL2_image)
    elseif(TARGET SDL2_image)
      set(GLYPH_IOS_SDL2_IMAGE_TARGET SDL2_image)
    endif()
  endif()

  function(glyph_resolve_ios_framework out_var framework_name)
    set(search_roots)
    string(TOUPPER "${framework_name}" upper_name)
    if(GLYPH_IOS_${upper_name}_ROOT)
      list(APPEND search_roots "${GLYPH_IOS_${upper_name}_ROOT}")
    endif()
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

  if(NOT GLYPH_IOS_SDL2_TARGET)
    glyph_resolve_ios_framework(GLYPH_IOS_SDL2_FRAMEWORK SDL2)
  endif()
  if(NOT GLYPH_IOS_SDL2_IMAGE_TARGET)
    glyph_resolve_ios_framework(GLYPH_IOS_SDL2_IMAGE_FRAMEWORK SDL2_image)
  endif()

  if(NOT GLYPH_IOS_SDL2_TARGET AND NOT GLYPH_IOS_SDL2_FRAMEWORK)
    message(FATAL_ERROR
      "SDL2 iOS framework not found. Put SDL2.framework or SDL2.xcframework under "
      "ios/Frameworks, or configure with -DGLYPH_IOS_SDL2_ROOT=/path/to/SDL2.")
  endif()
  if(NOT GLYPH_IOS_SDL2_IMAGE_TARGET AND NOT GLYPH_IOS_SDL2_IMAGE_FRAMEWORK)
    message(FATAL_ERROR
      "SDL2_image iOS framework not found. The bundled arcade sample uses PNG assets. Put "
      "SDL2_image.framework or SDL2_image.xcframework under ios/Frameworks, or configure "
      "with -DGLYPH_IOS_SDL2_IMAGE_ROOT=/path/to/SDL2_image.")
  endif()

  foreach(component TTF MIXER)
    string(TOLOWER "${component}" lower)
    set(GLYPH_IOS_SDL2_${component}_ROOT "" CACHE PATH "SDL2_${lower} iOS framework root")
    glyph_resolve_ios_framework(GLYPH_IOS_SDL2_${component}_FRAMEWORK SDL2_${lower})
    set(GLYPH_IOS_HAS_SDL_${component} OFF)
    if(GLYPH_IOS_SDL2_${component}_FRAMEWORK)
      set(GLYPH_IOS_HAS_SDL_${component} ON)
    endif()
  endforeach()
  if(GLYPH_REQUIRE_MEDIA AND NOT (GLYPH_IOS_HAS_SDL_TTF AND GLYPH_IOS_HAS_SDL_MIXER))
    message(FATAL_ERROR "iOS production builds require SDL2_ttf and SDL2_mixer frameworks.")
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
  set(GLYPH_IOS_BUNDLE_DEVELOPMENT_LANGUAGE "en")
  set(GLYPH_IOS_BUNDLE_EXECUTABLE "glyph_ios")
  set(GLYPH_IOS_BUNDLE_IDENTIFIER "com.glyph.runtime")
  target_link_libraries(glyph_ios PRIVATE glyph_script)
  if(GLYPH_IOS_SDL2_TARGET)
    target_link_libraries(glyph_ios PRIVATE ${GLYPH_IOS_SDL2_TARGET})
  else()
    target_include_directories(glyph_ios PRIVATE "${GLYPH_IOS_SDL2_FRAMEWORK}/Headers")
    target_link_libraries(glyph_ios PRIVATE "${GLYPH_IOS_SDL2_FRAMEWORK}")
  endif()
  if(GLYPH_IOS_SDL2_IMAGE_TARGET)
    target_link_libraries(glyph_ios PRIVATE ${GLYPH_IOS_SDL2_IMAGE_TARGET})
  else()
    target_include_directories(glyph_ios PRIVATE "${GLYPH_IOS_SDL2_IMAGE_FRAMEWORK}/Headers")
    target_link_libraries(glyph_ios PRIVATE "${GLYPH_IOS_SDL2_IMAGE_FRAMEWORK}")
  endif()
  foreach(component TTF MIXER)
    if(GLYPH_IOS_SDL2_${component}_FRAMEWORK)
      target_include_directories(glyph_ios PRIVATE "${GLYPH_IOS_SDL2_${component}_FRAMEWORK}/Headers")
      target_link_libraries(glyph_ios PRIVATE "${GLYPH_IOS_SDL2_${component}_FRAMEWORK}")
    endif()
  endforeach()
  target_compile_definitions(glyph_ios PRIVATE
    GLYPH_HAS_SDL=1
    GLYPH_HAS_SDL_IMAGE=1
    GLYPH_HAS_SDL_TTF=$<BOOL:${GLYPH_IOS_HAS_SDL_TTF}>
    GLYPH_HAS_SDL_MIXER=$<BOOL:${GLYPH_IOS_HAS_SDL_MIXER}>
  )
  target_link_libraries(glyph_ios PRIVATE
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
    XCODE_EMBED_FRAMEWORKS_CODE_SIGN_ON_COPY TRUE
  )
  if(GLYPH_IOS_SDL2_FRAMEWORK OR GLYPH_IOS_SDL2_IMAGE_FRAMEWORK)
    set_target_properties(glyph_ios PROPERTIES
      XCODE_EMBED_FRAMEWORKS "${GLYPH_IOS_SDL2_FRAMEWORK};${GLYPH_IOS_SDL2_IMAGE_FRAMEWORK};${GLYPH_IOS_SDL2_TTF_FRAMEWORK};${GLYPH_IOS_SDL2_MIXER_FRAMEWORK}"
    )
  endif()
endif()

if(NOT GLYPH_TARGET_IOS AND NOT GLYPH_TARGET_ANDROID AND SDL2_FOUND)
  install(TARGETS glyph RUNTIME DESTINATION .)
  install(FILES examples/arcade/demo.glyph DESTINATION arcade)
  install(DIRECTORY examples/arcade/assets/original examples/arcade/assets/fonts examples/arcade/assets/audio DESTINATION arcade/assets)
  install(DIRECTORY examples/arcade/comet-courier examples/arcade/ski-slalom DESTINATION arcade
    PATTERN "prototype*" EXCLUDE
    PATTERN ".DS_Store" EXCLUDE)
endif()
