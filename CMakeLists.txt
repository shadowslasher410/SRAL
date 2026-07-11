cmake_minimum_required(VERSION 4.3.1)

set(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION ON)

set(CMAKE_DIRECTORY_LABELS "SRAL_Root_Orchestrator")

project(SRAL_Orchestrator LANGUAGES C CXX)

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
option(BUILD_TESTS           "Compile testing harness executables" ON)
option(BUILD_BINDINGS        "Generate non-C++ language bridge bindings" ON)

set(VERSION_MINTARGET_MACOS   "15" CACHE STRING "Minimum macOS Target Version Baseline")
set(VERSION_MINTARGET_WINDOWS "10" CACHE STRING "Minimum Windows Target Version Baseline")
set(VERSION_MINTARGET_ANDROID "21"   CACHE STRING "Minimum Android API Level Baseline")
set(VERSION_MINTARGET_IOS     "15" CACHE STRING "Minimum iOS Deployment Version Baseline")

set(SRAL_WITH_NVDA      OFF CACHE BOOL "Enable NVDA Screen Reader layer" FORCE)
set(SRAL_WITH_SPEECHD   OFF CACHE BOOL "Enable Speech Dispatcher layer" FORCE)
set(SRAL_WITH_CHROMEVOX OFF CACHE BOOL "Enable ChromeVox WebAssembly layer" FORCE)
set(SRAL_WITH_ACCESSKIT OFF CACHE BOOL "Enable AccessKit accessibility layer" FORCE)

set(PATH_NVDA_INSTALL      "" CACHE PATH "Custom installation path for NVDA Screen Reader SDK.")
set(PATH_SPEECHD_INSTALL   "" CACHE PATH "Custom directory path to speech-dispatcher binary.")
set(PATH_PYTHON_INSTALL    "" CACHE FILEPATH "Custom absolute path to a Python 3.8+ interpreter.")
set(PATH_EMSDK_INSTALL     "" CACHE PATH "Custom local path to pre-installed Emscripten SDK.")
set(PATH_ANDROID_NDK       "" CACHE PATH "Custom local path to pre-installed Android NDK.")
set(PATH_ACCESSKIT_INSTALL "" CACHE PATH "Custom directory path to pre-built AccessKit cross-platform C-bindings library.")
set(PATH_JDK_INSTALL "" CACHE PATH "Custom installation path for pre-installed JDK.")

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
    -DCMAKE_C_STANDARD=17
    -DCMAKE_C_STANDARD_REQUIRED=ON
    -DCMAKE_C_EXTENSIONS=OFF
    -DCMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE=ON
    -DCMAKE_CXX_FLAGS_RELEASE=-D_FORTIFY_SOURCE=3
)

include(ExternalProject)
if(TARGET_DESKTOP)
    set(DESKTOP_MINIMUM_REQUIRED_WINDOWS "10")
    set(DESKTOP_MINIMUM_REQUIRED_MACOS   "15")

    if(CMAKE_HOST_WIN32)
        if("${VERSION_MINTARGET_WINDOWS}" VERSION_LESS "${DESKTOP_MINIMUM_REQUIRED_WINDOWS}")
            message(FATAL_ERROR 
                "\n===============================================================================\n"
                "[SRAL ERROR] Newer Windows Version Required!\n"
                "Your Windows Version: ${VERSION_MINTARGET_WINDOWS}\n"
                "Minimum Required Version:  10\n"
                "===============================================================================\n"
            )
        endif()
    endif()

    if(CMAKE_HOST_APPLE)
        if("${VERSION_MINTARGET_MACOS}" VERSION_LESS "${DESKTOP_MINIMUM_REQUIRED_MACOS}")
            message(FATAL_ERROR 
                "\n===============================================================================\n"
                "[SRAL ERROR] Newer MacOS Version Required!\n"
                "Your macOS Version: ${VERSION_MINTARGET_MACOS}\n"
                "Minimum Required Version:  15.7.7\n"
                "===============================================================================\n"
            )
        endif()
    endif()

    set(DESKTOP_EXTRA_ARGS "")

    if(SRAL_WITH_ACCESSKIT)
        if(NOT EXISTS "${PATH_ACCESSKIT_INSTALL}" AND NOT DEFINED ENV{ACCESSKIT_DIR})
            message(FATAL_ERROR "\n[SRAL Build Error] AccessKit installation path tracking validation failed!")
        endif()

        if(EXISTS "${PATH_ACCESSKIT_INSTALL}")
            cmake_path(SET FINAL_ACCESSKIT_DIR NORMALIZE "${PATH_ACCESSKIT_INSTALL}")
        else()
            cmake_path(SET FINAL_ACCESSKIT_DIR NORMALIZE "$ENV{ACCESSKIT_DIR}")
        endif()
        
        list(APPEND DESKTOP_EXTRA_ARGS 
            "-DSRAL_WITH_ACCESSKIT=ON"
            "-DSRAL_ACCESSKIT_SDK_DIR=${FINAL_ACCESSKIT_DIR}"
        )
    endif()

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
        STEP_TARGETS configure
        CMAKE_ARGS
            -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
            -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
            ${GLOBAL_HARDENING_ARGS}
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DBUILD_SHARED_LIBS=${BUILD_SHARED_LIBS}
            -DBUILD_TESTS=${BUILD_TESTS}
            -DBUILD_BINDINGS=${BUILD_BINDINGS}
            -DCMAKE_OSX_DEPLOYMENT_TARGET=${VERSION_MINTARGET_MACOS}
            -DCMAKE_SYSTEM_VERSION=${VERSION_MINTARGET_WINDOWS}
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
        set(Python3_EXECUTABLE "${Python3_EXECUTABLE}" CACHE FILEPATH "Validated Python Install" FORCE)
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
        STEP_TARGETS configure
        CMAKE_ARGS
            ${GLOBAL_HARDENING_ARGS}
            -DCMAKE_TOOLCHAIN_FILE=${EMSDK_TOOLCHAIN}
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DBUILD_SHARED_LIBS=${BUILD_SHARED_LIBS}
            -DBUILD_TESTS=${BUILD_TESTS}
            -DBUILD_BINDINGS=${BUILD_BINDINGS}
            -DSRAL_COMPILE_AS_WASM=ON
        INSTALL_COMMAND ""
    )
endif()

if(TARGET_ANDROID)
    
    find_package(Java REQUIRED COMPONENTS Development)

    if(Java_VERSION VERSION_LESS "25.0")
        message(FATAL_ERROR 
            "\n===============================================================================\n"
            "[SRAL ERROR] Newer Java Development Kit Version Required!\n"
            "Your JDK Version: ${Java_VERSION}\n"
            "Minimum Required Version: JDK 25\n"
            "===============================================================================\n"
        )
    endif()

    set(NDK_MINIMUM_REQUIRED_VERSION "27.0.12077973")
    set(API_MINIMUM_REQUIRED_LEVEL 21)

    if(VERSION_MINTARGET_ANDROID LESS API_MINIMUM_REQUIRED_LEVEL)
        message(FATAL_ERROR 
            "\n===============================================================================\n"
            "[SRAL ERROR] Newer Android API Version Required!\n"
            "Your Android API Version: ${VERSION_MINTARGET_ANDROID}\n"
            "Minimum Required API Version: 21 (Android 5.0 Lollipop)\n"
            "===============================================================================\n"
        )
    endif()

    if(NOT EXISTS "${PATH_ANDROID_NDK}" AND NOT DEFINED ENV{ANDROID_NDK_HOME})
        message(FATAL_ERROR "\n[SRAL Build Error] Android NDK structural paths could not be matched!")
    endif()
      
    if(EXISTS "${PATH_ANDROID_NDK}")
        cmake_path(SET REAL_NDK_PATH NORMALIZE "${PATH_ANDROID_NDK}")
    else()
        cmake_path(SET REAL_NDK_PATH NORMALIZE "$ENV{ANDROID_NDK_HOME}")
    endif()
    
    set(NDK_PROPERTIES_FILE "${REAL_NDK_PATH}/source.properties")
    if(EXISTS "${NDK_PROPERTIES_FILE}")
        file(STRINGS "${NDK_PROPERTIES_FILE}" NDK_RELEASE_LINE REGEX "^Pkg.Revision[ \t]*=[ \t]*")
        string(REGEX REPLACE "^Pkg.Revision[ \t]*=[ \t]*([0-9.]+).*" "\\1" DETECTED_NDK_VERSION "${NDK_RELEASE_LINE}")
        
        if("${DETECTED_NDK_VERSION}" VERSION_LESS "${NDK_MINIMUM_REQUIRED_VERSION}")
            message(FATAL_ERROR 
                "\n===============================================================================\n"
                "[SRAL ERROR] Newer Android NDK Version Required!\n"
                "Your NDK Version: ${DETECTED_NDK_VERSION}\n"
                "Minimum Required Version: 27.0.12077973 (r27c)\n"
                "===============================================================================\n"
            )
        endif()
    else()
        message(WARNING "[SRAL Validation Notice] source.properties missing from NDK root. Skipping floor validation check.")
    endif()

    set(NDK_TOOLCHAIN_FILE "${REAL_NDK_PATH}/build/cmake/android.toolchain.cmake")

    if(NOT EXISTS "${NDK_TOOLCHAIN_FILE}")
        message(FATAL_ERROR "[SRAL Build Error] Target path maps NDK folders but lacks build/cmake/android.toolchain.cmake!")
    endif()

    set(ANDROID_EXTRA_ARGS "")
    if(SRAL_WITH_ACCESSKIT)
        if(NOT EXISTS "${PATH_ACCESSKIT_INSTALL}" AND NOT DEFINED ENV{ACCESSKIT_DIR})
            message(FATAL_ERROR "\n[SRAL Build Error] AccessKit installation path validation failed for Android platform!")
        endif()

        if(EXISTS "${PATH_ACCESSKIT_INSTALL}")
            cmake_path(SET FINAL_ACCESSKIT_DIR NORMALIZE "${PATH_ACCESSKIT_INSTALL}")
        else()
            cmake_path(SET FINAL_ACCESSKIT_DIR NORMALIZE "$ENV{ACCESSKIT_DIR}")
        endif()
        
        list(APPEND ANDROID_EXTRA_ARGS 
            "-DSRAL_WITH_ACCESSKIT=ON"
            "-DSRAL_ACCESSKIT_SDK_DIR=${FINAL_ACCESSKIT_DIR}"
        )
    endif()

    include(ExternalProject)

    ExternalProject_Add(sral_android_sandbox
        SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/SRC"
        BINARY_DIR "${CMAKE_BINARY_DIR}/sandbox-android-build"
        STEP_TARGETS configure
        CMAKE_ARGS
            ${GLOBAL_HARDENING_ARGS}
            -DCMAKE_TOOLCHAIN_FILE=${NDK_TOOLCHAIN_FILE}
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DCMAKE_SYSTEM_NAME=Android
            -DANDROID_ABI=arm64-v8a
            -DANDROID_PLATFORM=android-${VERSION_MINTARGET_ANDROID}
            -DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON 
            -DBUILD_SHARED_LIBS=${BUILD_SHARED_LIBS}
            -DBUILD_TESTS=${BUILD_TESTS}
            -DBUILD_BINDINGS=${BUILD_BINDINGS}
            ${ANDROID_EXTRA_ARGS}
        INSTALL_COMMAND ""
    )
endif()

if(TARGET_IOS)
    set(IOS_MINIMUM_REQUIRED_VERSION "15")

    if("${VERSION_MINTARGET_IOS}" VERSION_LESS "${IOS_MINIMUM_REQUIRED_VERSION}")
        message(FATAL_ERROR 
            "\n===============================================================================\n"
            "[SRAL ERROR] Newer iOS Version Required!\n"
            "Your iOS Version: ${VERSION_MINTARGET_IOS}\n"
            "Minimum Required Version:  15\n"
            "===============================================================================\n"
        )
    endif()
    
    if(NOT APPLE)
        message(FATAL_ERROR "[SRAL Sandbox Error] Cross-compilation framework strictly requires a macOS host executing Xcode properties.")
    endif()
    
    if(NOT DEFINED CMAKE_OSX_SYSROOT)
        set(CMAKE_OSX_SYSROOT "iphoneos")
    endif()
    
    if(NOT DEFINED CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64")
    endif()
    
    set(IOS_EXTRA_ARGS "")
    if(SRAL_WITH_ACCESSKIT)
        if(NOT EXISTS "${PATH_ACCESSKIT_INSTALL}" AND NOT DEFINED ENV{ACCESSKIT_DIR})
            message(FATAL_ERROR "\n[SRAL Build Error] AccessKit installation path validation failed for iOS platform!")
        endif()

        if(EXISTS "${PATH_ACCESSKIT_INSTALL}")
            cmake_path(SET FINAL_ACCESSKIT_DIR NORMALIZE "${PATH_ACCESSKIT_INSTALL}")
        else()
            cmake_path(SET FINAL_ACCESSKIT_DIR NORMALIZE "$ENV{ACCESSKIT_DIR}")
        endif()
        
        list(APPEND IOS_EXTRA_ARGS 
            "-DSRAL_WITH_ACCESSKIT=ON"
            "-DSRAL_ACCESSKIT_SDK_DIR=${FINAL_ACCESSKIT_DIR}"
        )
    endif()

    ExternalProject_Add(sral_ios_sandbox
        SOURCE_DIR        "${CMAKE_CURRENT_SOURCE_DIR}/SRC"
        BINARY_DIR        "${CMAKE_BINARY_DIR}/sandbox-ios-build"
        STEP_TARGETS configure
        CMAKE_ARGS
            ${GLOBAL_HARDENING_ARGS}
            -DCMAKE_SYSTEM_NAME=iOS
            -DCMAKE_OSX_SYSROOT=${CMAKE_OSX_SYSROOT}
            -DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}
            -DCMAKE_OSX_DEPLOYMENT_TARGET=${VERSION_MINTARGET_IOS}
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DBUILD_SHARED_LIBS=OFF
            -DBUILD_TESTS=${BUILD_TESTS}
            -DBUILD_BINDINGS=${BUILD_BINDINGS}
            -DSRAL_COMPILE_AS_IOS=ON
            ${IOS_EXTRA_ARGS}
        INSTALL_COMMAND   ""
    )
endif()
message(STATUS "[SRAL Orchestrator] Configuration phase completed successfully.")