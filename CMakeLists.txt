cmake_minimum_required(VERSION 4.3.1)

set(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_DIRECTORY_LABELS "SRAL_Root_Orchestrator")

project(SRAL_Orchestrator LANGUAGES NONE)

cmake_policy(SET CMP0077 NEW) # option() strictly respects normal scope over cache
cmake_policy(SET CMP0114 NEW) # ExternalProject step targets use modern concurrent hooks
cmake_policy(SET CMP0126 NEW) # set(CACHE) does not silently strip local scope variables
cmake_policy(SET CMP0139 NEW) # Enforce strict system validation loops on processing steps

if(NOT DEFINED CMAKE_BINARY_DIR OR "${CMAKE_BINARY_DIR}" STREQUAL "")
    message(FATAL_ERROR "[SRAL SECURITY REJECTION] Build framework destination is unmapped. Aborting execution.")
endif()

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_CURRENT_BINARY_DIR)
    message(FATAL_ERROR 
        "\n===============================================================================\n"
        "[SRAL CRITICAL ALERT] In-source builds are structurally prohibited!\n"
        "Compiling inside your source tree risks local repository metadata corruption.\n"
        "Please delete 'CMakeCache.txt' and 'CMakeFiles/' from your root repository folder,\n"
        "generate an independent build directory, and execute the configuration from there.\n"
        "===============================================================================\n"
    )
endif()

option(TARGET_DESKTOP   "Build native desktop workspace library" ON)
option(TARGET_CHROMEVOX "Build WebAssembly ChromeVox wrapper" OFF)
option(TARGET_ANDROID   "Build Android mobile NDK asset bundle" OFF)
option(TARGET_IOS       "Build Apple iOS device/simulator static bundle" OFF)

option(BUILD_SHARED_LIBS     "Build shared library instead of static" ON)
option(BUILD_TESTS           "Compile testing harness executables" OFF)
option(BUILD_BINDINGS        "Generate non-C++ language bridge bindings" OFF)

set(VERSION_TARGET_MACOS   "15.0" CACHE STRING "Minimum macOS Target Version Baseline")
set(VERSION_TARGET_WINDOWS "10.0" CACHE STRING "Minimum Windows Target Version Baseline")
set(VERSION_TARGET_ANDROID "21"   CACHE STRING "Minimum Android API Level Baseline")
set(VERSION_TARGET_IOS     "15.0" CACHE STRING "Minimum iOS Deployment Version Baseline")

set(SRAL_WITH_NVDA      OFF CACHE BOOL "Enable NVDA Screen Reader layer" FORCE)
set(SRAL_WITH_SPEECHD   OFF CACHE BOOL "Enable Speech Dispatcher layer" FORCE)
set(SRAL_WITH_CHROMEVOX OFF CACHE BOOL "Enable ChromeVox WebAssembly layer" FORCE)

set(PATH_NVDA_INSTALL    "" CACHE PATH "Custom installation path for NVDA Screen Reader SDK.")
set(PATH_SPEECHD_INSTALL "" CACHE PATH "Custom directory path to speech-dispatcher binary.")
set(PATH_PYTHON_INSTALL  "" CACHE FILEPATH "Custom absolute path to a Python 3.8+ interpreter.")
set(PATH_EMSDK_INSTALL   "" CACHE PATH "Custom local path to pre-installed Emscripten SDK.")
set(PATH_ANDROID_NDK     "" CACHE PATH "Custom local path to pre-installed Android NDK.")

if(NOT DEFINED CMAKE_BUILD_TYPE OR "${CMAKE_BUILD_TYPE}" STREQUAL "")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose build variant" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()


set(GLOBAL_HARDENING_ARGS
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    -DCMAKE_CXX_VISIBILITY_PRESET=hidden
    -DCMAKE_VISIBILITY_INLINES_HIDDEN=ON
    -DCMAKE_CXX_STANDARD=20
    -DCMAKE_CXX_STANDARD_REQUIRED=ON
    -DCMAKE_CXX_EXTENSIONS=OFF
    -DCMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE=ON
    -DCMAKE_CXX_FLAGS_RELEASE=-D_FORTIFY_SOURCE=3
)

include(ExternalProject)
if(TARGET_DESKTOP)
    set(DESKTOP_EXTRA_ARGS "")

    if(CMAKE_HOST_WIN32 AND SRAL_WITH_NVDA)
        if(NOT EXISTS "${PATH_NVDA_INSTALL}" AND NOT DEFINED ENV{NVDA_DIR} AND NOT EXISTS "C:/Program Files (x86)/NVDA")
            message(FATAL_ERROR "\n[SRAL Build Error] NVDA Screen Reader installation path validation failed!")
        endif()

        if(EXISTS "${PATH_NVDA_INSTALL}")
            cmake_path(SET FINAL_NVDA_DIR NORMALIZE "${PATH_NVDA_INSTALL}")
        elseif(DEFINED ENV{NVDA_DIR} AND EXISTS "$ENV{NVDA_DIR}")
            cmake_path(SET FINAL_NVDA_DIR NORMALIZE "$ENV{NVDA_DIR}")
        else()
            set(FINAL_NVDA_DIR "C:/Program Files (x86)/NVDA")
        endif()
        list(APPEND DESKTOP_EXTRA_ARGS "-DSRAL_NVDA_SDK_INCLUDE_DIR=${FINAL_NVDA_DIR}/include")
    endif()
        
    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux" AND SRAL_WITH_SPEECHD)
        if(NOT EXISTS "${PATH_SPEECHD_INSTALL}" AND NOT EXISTS "/usr/include/speech-dispatcher")
            message(FATAL_ERROR "\n[SRAL Build Error] Speech Dispatcher development headers path validation failed!")
        endif()

        if(EXISTS "${PATH_SPEECHD_INSTALL}")
            cmake_path(SET FINAL_SPEECHD_DIR NORMALIZE "${PATH_SPEECHD_INSTALL}")
            list(APPEND DESKTOP_EXTRA_ARGS "-DSRAL_SPEECHD_INCLUDE_DIR=${FINAL_SPEECHD_DIR}/include")
        else()
            list(APPEND DESKTOP_EXTRA_ARGS "-DSRAL_SPEECHD_INCLUDE_DIR=/usr/include/speech-dispatcher")
        endif()
    endif()

    ExternalProject_Add(sral_desktop_sandbox
        SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/SRC"
        BINARY_DIR "${CMAKE_BINARY_DIR}/sandbox-desktop-build"
        CMAKE_ARGS
            ${GLOBAL_HARDENING_ARGS}
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DBUILD_SHARED_LIBS=${BUILD_SHARED_LIBS}
            -DBUILD_TESTS=${BUILD_TESTS}
            -DBUILD_BINDINGS=${BUILD_BINDINGS}
            -DCMAKE_OSX_DEPLOYMENT_TARGET=${VERSION_TARGET_MACOS}
            -DCMAKE_SYSTEM_VERSION=${VERSION_TARGET_WINDOWS}
            -DSRAL_WITH_NVDA=${SRAL_WITH_NVDA}
            -DSRAL_WITH_SPEECHD=${SRAL_WITH_SPEECHD}
            ${DESKTOP_EXTRA_ARGS}
        INSTALL_COMMAND ""
    )
    unset(DESKTOP_EXTRA_ARGS)
endif()
if(TARGET_CHROMEVOX)
    if(EXISTS "${PATH_PYTHON_INSTALL}")
        cmake_path(SET Python3_EXECUTABLE NORMALIZE "${PATH_PYTHON_INSTALL}")
        set(Python3_EXECUTABLE "${Python3_EXECUTABLE}" CACHE FILEPATH "Validated Python Overwrite" FORCE)
    endif()

    find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)

    cmake_path(SET EMSDK_ROOT NORMALIZE "${PATH_EMSDK_INSTALL}")
    set(EMSDK_TOOLCHAIN "${EMSDK_ROOT}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake")

    if(NOT EXISTS "${PATH_EMSDK_INSTALL}" OR NOT EXISTS "${EMSDK_TOOLCHAIN}")
        message(FATAL_ERROR "\n[SRAL Build Error] Emscripten SDK Toolchain path tracing failed!")
    endif()

    ExternalProject_Add(sral_chromevox_sandbox
        SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/SRC"
        BINARY_DIR "${CMAKE_BINARY_DIR}/sandbox-chromevox-build"
        CMAKE_ARGS
            ${GLOBAL_HARDENING_ARGS}
            -DCMAKE_TOOLCHAIN_FILE=${EMSDK_TOOLCHAIN}
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DBUILD_SHARED_LIBS=OFF 
            -DBUILD_TESTS=${BUILD_TESTS}
            -DBUILD_BINDINGS=${BUILD_BINDINGS}
            -DSRAL_COMPILE_AS_WASM=ON
        INSTALL_COMMAND ""
    )
endif()
if(TARGET_ANDROID)
    set(NDK_VER "r25c")
    
    if(NOT EXISTS "${PATH_ANDROID_NDK}" AND NOT DEFINED ENV{ANDROID_NDK_HOME})
        message(FATAL_ERROR "\n[SRAL Build Error] Android NDK structural paths could not be matched!")
    endif()
      
    if(EXISTS "${PATH_ANDROID_NDK}")
        cmake_path(SET REAL_NDK_PATH NORMALIZE "${PATH_ANDROID_NDK}")
    else()
        cmake_path(SET REAL_NDK_PATH NORMALIZE "$ENV{ANDROID_NDK_HOME}")
    endif()
    
    set(NDK_TOOLCHAIN_FILE "${REAL_NDK_PATH}/build/cmake/android.toolchain.cmake")

    if(NOT EXISTS "${NDK_TOOLCHAIN_FILE}")
        message(FATAL_ERROR "[SRAL Build Error] Target path maps NDK folders but lacks build/cmake/android.toolchain.cmake!")
    endif()

    ExternalProject_Add(sral_android_sandbox
        SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/SRC"
        BINARY_DIR "${CMAKE_BINARY_DIR}/sandbox-android-build"
        CMAKE_ARGS
            ${GLOBAL_HARDENING_ARGS}
            -DCMAKE_TOOLCHAIN_FILE=${NDK_TOOLCHAIN_FILE}
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DCMAKE_SYSTEM_NAME=Android
            -DANDROID_ABI=arm64-v8a
            -DANDROID_PLATFORM=android-${VERSION_TARGET_ANDROID}
            -DBUILD_SHARED_LIBS=${BUILD_SHARED_LIBS}
            -DBUILD_TESTS=${BUILD_TESTS}
            -DBUILD_BINDINGS=${BUILD_BINDINGS}
        INSTALL_COMMAND ""
    )
endif()
if(TARGET_IOS)
    if(NOT APPLE)
        message(FATAL_ERROR "[SRAL Sandbox Error] Cross-compilation framework strictly requires a macOS host executing Xcode properties.")
    endif()
    
    if(NOT DEFINED CMAKE_OSX_SYSROOT)
        set(CMAKE_OSX_SYSROOT "iphoneos")
    endif()
    
    if(NOT DEFINED CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64")
    endif()

    ExternalProject_Add(sral_ios_sandbox
        SOURCE_DIR        "${CMAKE_CURRENT_SOURCE_DIR}/SRC"
        BINARY_DIR        "${CMAKE_BINARY_DIR}/sandbox-ios-build"
        CMAKE_ARGS
            ${GLOBAL_HARDENING_ARGS}
            -DCMAKE_SYSTEM_NAME=iOS
            -DCMAKE_OSX_SYSROOT=${CMAKE_OSX_SYSROOT}
            -DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}
            -DCMAKE_OSX_DEPLOYMENT_TARGET=${VERSION_TARGET_IOS}
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DBUILD_SHARED_LIBS=OFF 
            -DBUILD_TESTS=${BUILD_TESTS}
            -DBUILD_BINDINGS=${BUILD_BINDINGS}
            -DSRAL_COMPILE_AS_IOS=ON
        INSTALL_COMMAND   ""
    )
endif()
