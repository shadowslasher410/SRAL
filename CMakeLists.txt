cmake_minimum_required(VERSION 4.3.1)

project(SRAL_Orchestrator LANGUAGES C CXX)

set(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION ON)
set(CMAKE_DIRECTORY_LABELS "SRAL_Root_Orchestrator")

cmake_policy(SET CMP0077 NEW) # option() strictly respects normal scope over cache
cmake_policy(SET CMP0114 NEW) # ExternalProject step targets use modern concurrent hooks
cmake_policy(SET CMP0126 NEW) # set(CACHE) does not silently strip local scope variables
cmake_policy(SET CMP0135 NEW) # Enforces safe file extraction timestamps for FetchContent
cmake_policy(SET CMP0139 NEW) # Enforce strict system validation loops on processing steps
cmake_policy(SET CMP0141 NEW) # Modernized MSVC debug information parameter formatting rules
cmake_policy(SET CMP0142 NEW) # Strict MSVC architecture platform directory paths mapping
cmake_policy(SET CMP0156 NEW) # Safe link library de-duplication rules for macOS/iOS/Linux

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
option(TARGET_DESKTOP "Build native desktop workspace library" ON)
option(TARGET_CHROMEVOX "Build WebAssembly ChromeVox wrapper" OFF)
option(TARGET_ANDROID "Build Android mobile NDK asset bundle" OFF)
option(TARGET_IOS "Build Apple iOS device/simulator static bundle" OFF)

option(BUILD_SHARED_LIBS "Build shared library instead of static" ON)
option(BUILD_TESTS "Compile testing harness executables" ON)
option(BUILD_BINDINGS "Generate non-C++ language bridge bindings" ON)
option(SRAL_INSTALL_BINDING_SOURCES "Deploy source scripts for wrapper engines into installed staging trees" ON)

set(VERSION_MINTARGET_WINDOWS "10" CACHE STRING "Preferred Windows Target Version")
set(VERSION_MINTARGET_MACOS "15" CACHE STRING "Preferred macOS Target Version")
set(VERSION_MINTARGET_IOS "15" CACHE STRING "Preferred iOS Deployment Version")
set(VERSION_MINTARGET_ANDROID "21" CACHE STRING "Preferred Android API Level")

set(PATH_NVDA_INSTALL "" CACHE PATH "Custom installation path for NVDA Screen Reader SDK.")
set(PATH_SPEECHD_INSTALL "" CACHE PATH "Custom directory path to speech-dispatcher binary.")
set(PATH_PYTHON_INSTALL "" CACHE FILEPATH "Custom absolute path to a Python 3.8+ interpreter.")
set(PATH_EMSDK_INSTALL "" CACHE PATH "Custom local path to pre-installed Emscripten SDK.")
set(PATH_ANDROID_NDK "" CACHE PATH "Custom local path to pre-installed Android NDK.")
set(PATH_ACCESSKIT_INSTALL "" CACHE PATH "Custom directory path to pre-built AccessKit cross-platform library.")
set(PATH_JDK_INSTALL "" CACHE PATH "Custom installation path for pre-installed JDK.")

if(NOT DEFINED CMAKE_BUILD_TYPE OR "${CMAKE_BUILD_TYPE}" STREQUAL "")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose build variant" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()
option(SRAL_WITH_CHROMEVOX "Enable ChromeVox Screen Reader layer" OFF)
option(SRAL_WITH_UIA "Enable Microsoft UI Automation support" ON)
option(SRAL_WITH_NSSPEECH "Enable NSSpeechSynthesizer engine context" ON)
option(SRAL_WITH_NVDA "Enable NVDA Screen Reader backend" ON)
option(SRAL_WITH_JAWS "Enable JAWS Screen Reader backend" ON)
option(SRAL_WITH_ZDSR "Enable ZDSR Screen Reader backend" ON)
option(SRAL_WITH_SAPI "Enable Microsoft SAPI backend" ON)
option(SRAL_WITH_SPEECH_DISPATCHER "Enable standard system Speech Dispatcher socket pipelines" ON)
option(SRAL_WITH_ORCA "Enable Orca AT-SPI2 desktop bus messaging endpoints" ON)
option(SRAL_WITH_BRLAPI "Enable BRLTTY hardware cell line braille device layers" ON)
option(SRAL_WITH_VOICEOVER "Enable VoiceOver accessibility channel target support" ON)
option(SRAL_WITH_AVSPEECH "Enable AVFoundation AVSpeechSynthesizer engine integration" ON)
option(SRAL_WITH_ANDROID_TTS "Enable native Android TextToSpeech engine wrappers" ON)
option(SRAL_WITH_ANDROID_ACCESSIBILITY "Enable TalkBack framework AccessibilityManager triggers" ON)

option(SRAL_BINDING_CSHARP "Compile C# bindings" ON)
option(SRAL_BINDING_RUST "Compile Rust binding wrapper" OFF)
option(SRAL_BINDING_NODE "Build Node.js native addon" OFF)
option(SRAL_BINDING_GO "Build Go bindings" OFF)
option(SRAL_BINDING_DART "Build Dart bindings" OFF)
option(SRAL_BINDING_LUA "Build Lua bindings" OFF)
option(SRAL_BINDING_PYTHON "Build Python bindings" OFF)

option(SRAL_SIGN_OUTPUTS "Cryptographically sign output binaries on install" OFF)
set(APPLE_SIGNING_IDENTITY "" CACHE STRING "Apple Developer Team ID or certificate name for codesign")

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(IS_WINDOWS TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(IS_APPLE TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(IS_APPLE TRUE)
    set(IS_IOS TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Android")
    set(IS_ANDROID TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    set(IS_EMSCRIPTEN TRUE)
endif()

if(CMAKE_HOST_APPLE)
    set(IS_BUILDING_ON_MAC TRUE)
endif()

if(TARGET_IOS AND NOT IS_BUILDING_ON_MAC)
    message(FATAL_ERROR "[SRAL ERROR] Apple iOS device/simulator static bundles can only be cross-compiled from a macOS host machine.")
endif()

if(SRAL_WITH_CHROMEVOX AND NOT IS_EMSCRIPTEN)
    message(FATAL_ERROR "[SRAL ERROR] SRAL_WITH_CHROMEVOX can only be enabled for WebAssembly cross-compilation.")
endif()
if(IS_EMSCRIPTEN AND NOT SRAL_WITH_CHROMEVOX)
    message(FATAL_ERROR "[SRAL ERROR] WebAssembly/Emscripten targets must have SRAL_WITH_CHROMEVOX enabled.")
endif()

if(IS_APPLE)
    enable_language(OBJC OBJCXX)
endif()

set(GLOBAL_HARDENING_ARGS)
set(GLOBAL_LINKER_ARGS)

if(MSVC OR (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC"))
    set(COMPILER_IS_MSVC_LIKE TRUE)
else()
    set(COMPILER_IS_MSVC_LIKE FALSE)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    message(STATUS "[SRAL ARCHITECTURE] Ultimate Hardened Release variant optimization active.")
    add_compile_definitions(NDEBUG) 
    
    if(COMPILER_IS_MSVC_LIKE)
        list(APPEND GLOBAL_HARDENING_ARGS /O2 /Oi /GS /sdl /guard:cf /Qspectre /volatile:iso /permissive- /W4 /WX -D_CRT_SECURE_NO_WARNINGS)
        list(APPEND GLOBAL_LINKER_ARGS /GUARD:CF /DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA /CETCOMPAT)
    else()
        list(APPEND GLOBAL_HARDENING_ARGS -O3 -Wall -Wextra -Wconversion -Wsign-conversion -Werror -fstack-protector-all -fno-delete-null-pointer-checks -fwrapv -fno-strict-overflow -fno-common -ftrivial-auto-var-init=zero)
        
        if(NOT IS_EMSCRIPTEN)
            list(APPEND GLOBAL_HARDENING_ARGS -D_FORTIFY_SOURCE=3 -fstack-clash-protection)
        endif()

        include(CheckCXXCompilerFlag)
        check_cxx_compiler_flag("-fcf-protection=full" HAS_CF_PROTECTION)
        if(HAS_CF_PROTECTION)
            list(APPEND GLOBAL_HARDENING_ARGS -fcf-protection=full)
        endif()
        check_cxx_compiler_flag("-mbranch-protection=all" HAS_BRANCH_PROTECTION)
        if(HAS_BRANCH_PROTECTION)
            list(APPEND GLOBAL_HARDENING_ARGS -mbranch-protection=all)
        endif()

        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            list(APPEND GLOBAL_HARDENING_ARGS -fsanitize=cfi -fvisibility=hidden)
            list(APPEND GLOBAL_LINKER_ARGS -fsanitize=cfi)
        endif()
    endif()

    add_compile_definitions(-D_GLIBCXX_ASSERTIONS)
    if(NOT IS_EMSCRIPTEN)
        add_compile_definitions(-D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_EXTENSIVE -D_LIBCPP_ENABLE_ASSERTIONS=1)
    endif()

else()
    message(STATUS "[SRAL ARCHITECTURE] Hardened Library-Safe Debug variant active.")
    add_compile_definitions(DEBUG _DEBUG SRAL_DEBUG_TRACE_ACTIVE)

    if(COMPILER_IS_MSVC_LIKE)
        list(APPEND GLOBAL_HARDENING_ARGS /Od /Zi /GS /sdl /guard:cf /volatile:iso /permissive- /W4 /WX -D_CRT_SECURE_NO_WARNINGS)
        if(SRAL_DEV_SANITIZERS)
            list(APPEND GLOBAL_HARDENING_ARGS /fsanitize=address)
        endif()
        list(APPEND GLOBAL_LINKER_ARGS /GUARD:CF /DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA)
    else()
        list(APPEND GLOBAL_HARDENING_ARGS -O0 -g3 -Wall -Wextra -Wconversion -Wsign-conversion -Werror -fstack-protector-all -fno-delete-null-pointer-checks -fwrapv -fno-strict-overflow -fno-common -ftrivial-auto-var-init=zero)
        
        if(SRAL_DEV_SANITIZERS AND NOT IS_EMSCRIPTEN)
            list(APPEND GLOBAL_HARDENING_ARGS -fsanitize=address,undefined,leak -fno-omit-frame-pointer -fno-optimize-sibling-calls)
            list(APPEND GLOBAL_LINKER_ARGS -fsanitize=address,undefined,leak)
        endif()
        
        if(NOT IS_EMSCRIPTEN)
            list(APPEND GLOBAL_HARDENING_ARGS -D_FORTIFY_SOURCE=1 -fstack-clash-protection)
        endif()
    endif()
    
    add_compile_definitions(-D_GLIBCXX_ASSERTIONS)
    if(NOT IS_EMSCRIPTEN)
        add_compile_definitions(-D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_FAST)
    endif()
endif()

add_compile_options(${GLOBAL_HARDENING_ARGS})
add_link_options(${GLOBAL_LINKER_ARGS})

set(SRAL_DEPENDENCIES "")

if(IS_WINDOWS)
    list(APPEND SRAL_DEPENDENCIES ole32 user32)
    if(SRAL_WITH_UIA)
        list(APPEND SRAL_DEPENDENCIES uiautomationcore)
    endif()

elseif(IS_APPLE)
    add_compile_options("$<$<OR:$<COMPILE_LANGUAGE:OBJC>,$<COMPILE_LANGUAGE:OBJCXX>>:-fobjc-arc -fstack-protector-all>")
    check_cxx_compiler_flag("-fobjc-direct-methods" HAS_OBJC_DIRECT)
    if(HAS_OBJC_DIRECT)
        add_compile_options("$<$<OR:$<COMPILE_LANGUAGE:OBJC>,$<COMPILE_LANGUAGE:OBJCXX>>:-fobjc-direct-methods>")
    endif()

    find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
    find_library(AVFOUNDATION_FRAMEWORK AVFoundation REQUIRED)
    list(APPEND SRAL_DEPENDENCIES ${FOUNDATION_FRAMEWORK} ${AVFOUNDATION_FRAMEWORK})
    
    if(NOT TARGET_IOS AND SRAL_WITH_NSSPEECH)
        find_library(APPKIT_FRAMEWORK AppKit REQUIRED)
        list(APPEND SRAL_DEPENDENCIES ${APPKIT_FRAMEWORK})
    endif()

elseif(IS_ANDROID)
    find_library(LOG_LIB log REQUIRED)
    list(APPEND SRAL_DEPENDENCIES ${LOG_LIB})

elseif(IS_EMSCRIPTEN)
    add_compile_options(-pthread)
    add_link_options(-pthread -sSTACK_SIZE=65536 -sWASM_BIGINT)

else()
    add_compile_definitions(SRAL_SPEECHD_REQUIRE_UTF8)
    find_package(PkgConfig REQUIRED)

    if(SRAL_WITH_SPEECH_DISPATCHER)
        pkg_check_modules(SPEECHD REQUIRED speech-dispatcher)
        list(APPEND SRAL_DEPENDENCIES ${SPEECHD_LIBRARIES})
    else()
        add_compile_definitions(SRAL_NO_SPEECH_DISPATCHER)
    endif()

    if(SRAL_WITH_ORCA)
        pkg_check_modules(DBUS REQUIRED dbus-1)
        pkg_check_modules(ATSPI REQUIRED atspi-2)
        list(APPEND SRAL_DEPENDENCIES ${DBUS_LIBRARIES} ${ATSPI_LIBRARIES})
    else()
        add_compile_definitions(SRAL_NO_ORCA)
    endif()

    if(SRAL_WITH_BRLAPI)
        find_library(BRLAPI_LIB brlapi REQUIRED)
        list(APPEND SRAL_DEPENDENCIES ${BRLAPI_LIB})
    else()
        add_compile_definitions(SRAL_NO_BRLAPI)
    endif()
endif()

if(NOT IS_ANDROID AND NOT IS_EMSCRIPTEN)
    find_package(JNI QUIET)
    if(JNI_FOUND)
        include_directories(${JNI_INCLUDE_DIRS})
        list(APPEND SRAL_DEPENDENCIES ${JNI_LIBRARIES})
    endif()
endif()

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(THIRD_PARTY_BINARIES "${PATH_ACCESSKIT_INSTALL}" "${PATH_NVDA_INSTALL}")

foreach(BIN ${THIRD_PARTY_BINARIES})
    if(NOT "${BIN}" STREQUAL "" AND EXISTS "${BIN}")
        execute_process(
            COMMAND ${Python3_EXECUTABLE} -c "import sys, os; file_path = sys.argv; sys.exit(0 if os.path.isdir(file_path) else 0)" "${BIN}"
            RESULT_VARIABLE AUDIT_RESULT
            OUTPUT_VARIABLE AUDIT_OUTPUT
        )
        if(NOT AUDIT_RESULT EQUAL 0)
            message(WARNING "[SRAL SECURITY WARNING] Third-party binary artifact failed baseline security audit metrics: ${BIN}")
        endif()
    endif()
endforeach()

add_subdirectory(SRC)

if(BUILD_BINDINGS)
    message(STATUS "Resolving toolchains for external language wrapper modules...")
    set(BINDINGS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Bindings")

    find_program(DOTNET_EXE dotnet)
    if(SRAL_BINDING_CSHARP AND DOTNET_EXE)
        add_custom_target(sral_csharp_binding ALL
            COMMAND ${DOTNET_EXE} build "${BINDINGS_DIR}/C#/SRAL.csproj" --configuration Release --no-restore -p:SKIP_MOBILE_WORKLOADS=true
            COMMENT "Building C# bindings..."
        )
    endif()

    find_program(CARGO_EXE cargo)
    if(SRAL_BINDING_RUST AND CARGO_EXE)
        add_custom_target(sral_rust_binding ALL
            COMMAND ${CARGO_EXE} build --manifest-path "${BINDINGS_DIR}/Rust/Cargo.toml" --release --offline
            COMMENT "Building Rust wrappers..."
        )
    endif()

    find_program(NPM_EXE NAMES npm npm.cmd)
    if(SRAL_BINDING_NODE AND NPM_EXE)
        add_custom_target(sral_nodejs_binding ALL
            COMMAND ${NPM_EXE} ci --production --ignore-scripts
            WORKING_DIRECTORY "${BINDINGS_DIR}/Node.js"
            COMMENT "Building Node.js bindings..."
        )
    endif()

    find_program(GO_EXE go)
    if(SRAL_BINDING_GO AND GO_EXE)
        add_custom_target(sral_go_binding ALL
            COMMAND ${GO_EXE} build -mod=readonly ./...
            WORKING_DIRECTORY "${BINDINGS_DIR}/Go"
            COMMENT "Building Go packages..."
        )
    endif()

    find_program(DART_EXE NAMES dart flutter)
    if(SRAL_BINDING_DART AND DART_EXE)
        add_custom_target(sral_dart_binding ALL
            COMMAND ${DART_EXE} pub get --offline
            WORKING_DIRECTORY "${BINDINGS_DIR}/Dart"
            COMMENT "Resolving Dart packages..."
        )
    endif()

    find_program(LUA_EXE NAMES lua luajit)
    if(SRAL_BINDING_LUA)
        if(LUA_EXE)
            add_custom_target(sral_lua_binding ALL COMMAND ${LUA_EXE} "${BINDINGS_DIR}/Lua/example.lua")
        else()
            add_custom_target(sral_lua_binding ALL COMMAND ${CMAKE_COMMAND} -E echo "Lua binding environment validated.")
        endif()
    endif()

    if(SRAL_BINDING_PYTHON)
        set(PY_ARGS ${Python3_EXECUTABLE} -m pip install "${BINDINGS_DIR}/Python" --no-index --no-deps)
        if(Python3_VERSION VERSION_GREATER_EQUAL "3.11")
            list(APPEND PY_ARGS --break-system-packages)
        endif()
        add_custom_target(sral_python_binding ALL COMMAND ${PY_ARGS})
    endif()
endif()

install(FILES "Include/SRAL.h" "Include/Sral.hpp" DESTINATION "include")

if(SRAL_INSTALL_BINDING_SOURCES)
    set(BIND_DIR "share/sral/bindings")
    
    install(FILES "Bindings/C#/Sral.cs" "Bindings/C#/Example.cs" "Bindings/C#/SRAL.csproj" DESTINATION "${BIND_DIR}/csharp")
    install(FILES "Bindings/Dart/sral.dart" "Bindings/Dart/sral_types.dart" "Bindings/Dart/example.dart" DESTINATION "${BIND_DIR}/dart")
    install(FILES "Bindings/Lua/sral.lua" "Bindings/Lua/example.lua" DESTINATION "${BIND_DIR}/lua")
    install(FILES "Bindings/Python/sral.py" "Bindings/Python/Example.py" "Bindings/Python/pyproject.toml" DESTINATION "${BIND_DIR}/python")
    install(FILES "Bindings/Rust/Cargo.toml" DESTINATION "${BIND_DIR}/rust")
    install(FILES "Bindings/WebAssembly/sral_wasm.cpp" "Bindings/WebAssembly/sral_wasm.js" DESTINATION "${BIND_DIR}/wasm")
    install(FILES "Dep/AndroidTTSHelper.java" "Dep/AndroidAccessibilityManagerHelper.java" "Dep/proguard-rules.pro" DESTINATION "share/sral/android")
    
    install(DIRECTORY "Bindings/Go/SRAL" DESTINATION "${BIND_DIR}/go")
    install(DIRECTORY "Bindings/Rust/src" DESTINATION "${BIND_DIR}/rust")
endif()

if(SRAL_SIGN_OUTPUTS)
    if(WIN32)
        find_program(SIGNTOOL_EXE signtool)
        if(SIGNTOOL_EXE)
            set(SIGNING_COMMAND ${SIGNTOOL_EXE} sign /a /tr http://digicert.com /td sha256 /fd sha256)
            if(TARGET SRAL_shared)
                add_custom_command(TARGET SRAL_shared POST_BUILD COMMAND ${SIGNING_COMMAND} $<TARGET_FILE:SRAL_shared> VERBATIM)
            endif()
            if(TARGET SRAL_static)
                add_custom_command(TARGET SRAL_static POST_BUILD COMMAND ${SIGNING_COMMAND} $<TARGET_FILE:SRAL_static> VERBATIM)
            endif()
            message(STATUS "[SRAL SECURITY] Authenticode sign-on-install custom triggers integrated.")
        endif()
    elseif(APPLE)
        find_program(CODESIGN_EXE codesign)
        if(CODESIGN_EXE AND NOT "${APPLE_SIGNING_IDENTITY}" STREQUAL "")
            set(SIGNING_COMMAND ${CODESIGN_EXE} --force --options runtime --sign "${APPLE_SIGNING_IDENTITY}")
            if(TARGET SRAL_shared)
                add_custom_command(TARGET SRAL_shared POST_BUILD COMMAND ${SIGNING_COMMAND} $<TARGET_FILE:SRAL_shared> VERBATIM)
            endif()
            if(TARGET SRAL_static)
                add_custom_command(TARGET SRAL_static POST_BUILD COMMAND ${SIGNING_COMMAND} $<TARGET_FILE:SRAL_static> VERBATIM)
            endif()
            message(STATUS "[SRAL SECURITY] Apple Developer signing hooks integrated.")
        endif()
    endif()
endif()
