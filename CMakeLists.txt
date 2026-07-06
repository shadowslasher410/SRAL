cmake_minimum_required(VERSION 4.3.1)
project(SRAL_Orchestrator NONE)

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

set(PATH_NVDA_INSTALL    "" CACHE PATH "Custom installation path for NVDA Screen Reader. Only required for Windows builds. If left empty, CMake will attempt to locate a compatible NVDA installation in the system PATH.")
set(PATH_SPEECHD_INSTALL "" CACHE PATH "Custom directory path to speech-dispatcher binary. Only required for Linux builds. If left empty, CMake will attempt to locate a compatible speech-dispatcher installation in the system PATH.")
set(PATH_PYTHON_INSTALL      "" CACHE FILEPATH "Custom absolute path to a Python 3.8+ interpretation. Only required for ChromeVox builds. If left empty, CMake will attempt to locate a compatible Python interpreter in the system PATH.")
set(PATH_EMSDK_INSTALL   "" CACHE PATH "Custom local path to pre-installed Emscripten SDK package. Only required for ChromeVox builds. If left empty, CMake will attempt to locate a compatible Emscripten installation in the system PATH.")
set(PATH_ANDROID_NDK     "" CACHE PATH "Custom local path to pre-installed Android NDK package. Only required for Android builds. If left empty, CMake will attempt to locate a compatible Android NDK installation in the system PATH.")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose build variant" FORCE)
endif()

include(ExternalProject)

if(TARGET_DESKTOP)
    set(DESKTOP_EXTRA_ARGS "")

    if(WIN32)
        if(NOT EXISTS "${PATH_NVDA_INSTALL}" AND NOT DEFINED ENV{NVDA_DIR} AND NOT EXISTS "C:/Program Files (x86)/NVDA")
            message(FATAL_ERROR 
                "\n===============================================================================\n"
                "[SRAL Build Error] NVDA Screen Reader installation could not be found!\n"
                "Download the SDK package manually from: https://download.nvaccess.org/releases/stable/\n"
                "Update your custom installation path inside your CMakeUserPresets.json file, or pass manually via CLI: -DPATH_NVDA_INSTALL=\"C:/path/to/NVDA\"\n"
                "===============================================================================\n"
            )
        endif()

         if(EXISTS "${PATH_NVDA_INSTALL}")
            set(FINAL_NVDA_DIR "${PATH_NVDA_INSTALL}")
        elseif(DEFINED ENV{NVDA_DIR})
            set(FINAL_NVDA_DIR "$ENV{NVDA_DIR}")
        else()
            set(FINAL_NVDA_DIR "C:/Program Files (x86)/NVDA")
        endif()
        list(APPEND DESKTOP_EXTRA_ARGS "-DSRAL_NVDA_SDK_INCLUDE_DIR=${FINAL_NVDA_DIR}/include")
    endif()
        
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
      if(NOT EXISTS "${PATH_SPEECHD_INSTALL}" AND NOT EXISTS "/usr/include/speech-dispatcher")
         message(FATAL_ERROR 
                "\n===============================================================================\n"
                "[SRAL Build Error] Speech Dispatcher development headers could not be found!\n"
                "Install it via your system package manager (e.g., 'sudo apt install libspeechd-dev'), or download and build the source manually from: https://github.com/brailcom/speechd/releases/latest\n"
                "Update your custom installation path inside your CMakeUserPresets.json file, or pass manually via CLI: -DPATH_SPEECHD_INSTALL=\"/path/to/speechd/installation/root\"\n"
                "===============================================================================\n"
            )
      endif()

      if(EXISTS "${PATH_SPEECHD_INSTALL}")
        list(APPEND DESKTOP_EXTRA_ARGS "-DSRAL_SPEECHD_INCLUDE_DIR=${PATH_SPEECHD_INSTALL}/include")
      else()
          list(APPEND DESKTOP_EXTRA_ARGS "-DSRAL_SPEECHD_INCLUDE_DIR=/usr/include/speech-dispatcher")
      endif()
    endif()

    ExternalProject_Add(sral_desktop_sandbox
        SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/SRC"
        BINARY_DIR "${CMAKE_BINARY_DIR}/sandbox-desktop-build"
        CMAKE_ARGS
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DBUILD_SHARED_LIBS=${BUILD_SHARED_LIBS}
            -DBUILD_TESTS=${BUILD_TESTS}
            -DBUILD_BINDINGS=${BUILD_BINDINGS}
            -DCMAKE_OSX_DEPLOYMENT_TARGET=${VERSION_TARGET_MACOS}
            -DCMAKE_SYSTEM_VERSION=${VERSION_TARGET_WINDOWS}
            ${DESKTOP_EXTRA_ARGS}
        INSTALL_COMMAND ""
    )
endif()

if(TARGET_CHROMEVOX)
    if(EXISTS "${PATH_PYTHON_INSTALL}")
        set(Python3_EXECUTABLE "${PATH_PYTHON_INSTALL}" CACHE FILEPATH "Validated Python Overwrite" FORCE)
    endif()

    find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)
    message(STATUS "Found compatible Python interpreter: ${Python3_EXECUTABLE} (Version: ${Python3_VERSION})")

    set(EMSDK_LOCAL_DIR "${CMAKE_BINARY_DIR}/sandbox-emsdk")
    set(EMSDK_TOOLCHAIN "${PATH_EMSDK_INSTALL}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake")

    if(NOT EXISTS "${PATH_EMSDK_INSTALL}" OR NOT EXISTS "${EMSDK_TOOLCHAIN}")
        message(FATAL_ERROR 
            "\n===============================================================================\n"
            "[SRAL Build Error] Emscripten SDK Toolchain could not be found!\n"
            "Please download and install the standard EMSDK manually from: https://github.com/emscripten-core/emsdk/tags\n"
            "And activate it using the Python commands:\n"
            "   '${Python3_EXECUTABLE} emsdk.py install latest'\n"
            "   '${Python3_EXECUTABLE} emsdk.py activate latest'\n"
            "Or provide a custom installation directory via CLI: -DPATH_EMSDK_INSTALL=\"/path/to/emsdk\"\n"
            "===============================================================================\n"
        )
    endif()

    ExternalProject_Add(sral_chromevox_sandbox
        SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/SRC"
        BINARY_DIR "${CMAKE_BINARY_DIR}/sandbox-chromevox-build"
        CMAKE_ARGS
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
    if(NOT EXISTS "${PATH_ANDROID_NDK}" AND NOT DEFINED ENV{ANDROID_NDK_HOME})
        set(NDK_VER "r25c")
        
        if(WIN32)
          set(NDK_PLATFORM_URL "https://dl.google.com/android/repository/android-ndk-${NDK_VER}-windows.zip")
        elseif(APPLE)
          set(NDK_PLATFORM_URL "https://dl.google.com/android/repository/android-ndk-${NDK_VER}-darwin.dmg")
        else()
          set(NDK_PLATFORM_URL "https://dl.google.com/android/repository/android-ndk-${NDK_VER}-linux.zip")
        endif()

        message(FATAL_ERROR 
            "\n===============================================================================\n"
            "[SRAL Build Error] Android NDK installation could not be found!\n"
            "NOTE: A minimum Android NDK version of ${NDK_VER} or higher is required!\n"
            "Download the standalone NDK archive manually from Google: ${NDK_PLATFORM_URL}\n"
            "Extract it, and define your path using your CMakeUserPresets.json file or via CLI: -DPATH_ANDROID_NDK=\"/path/to/android-ndk-${NDK_VER}\"\n"
            "===============================================================================\n"
        )
    endif()
      
    if(EXISTS "${PATH_ANDROID_NDK}")
        set(REAL_NDK_PATH "${PATH_ANDROID_NDK}")
    else()
        set(REAL_NDK_PATH "$ENV{ANDROID_NDK_HOME}")
    endif()
    
    set(NDK_TOOLCHAIN_FILE "${REAL_NDK_PATH}/build/cmake/android.toolchain.cmake")

    if(NOT EXISTS "${NDK_TOOLCHAIN_FILE}")
        message(FATAL_ERROR "[SRAL Build Error] The target matches NDK paths but lacks 'build/cmake/android.toolchain.cmake'!")
    endif()

    ExternalProject_Add(sral_android_sandbox
        SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/SRC"
        BINARY_DIR "${CMAKE_BINARY_DIR}/sandbox-android-build"
        CMAKE_ARGS
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
        message(FATAL_ERROR "[SRAL Sandbox] Apple iOS builds require a macOS host environment running Xcode.")
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