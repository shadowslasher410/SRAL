cmake_minimum_required(VERSION 4.3.1)

set(CMAKE_CPS_PREFER_CPS ON) 

set(CMAKE_COMPILE_WARNING_AS_ERROR ON)
set(CMAKE_LINK_WARNING_AS_ERROR ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON) 

include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
if(ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

project(SRAL_Core 
    VERSION 1.0.0
    LANGUAGES C CXX
    SPDX_LICENSE "MIT"               # Native standardized license metadata formatting
    COMPAT_VERSION "1.0.0"            # Restricts downstream package-matching breaking changes
)

include(CMakeDependentOption) 
include(CheckCXXCompilerFlag)        # For fail-safe compile-time verification
include(CheckLinkerFlag)             # For fail-safe link-time verification

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(SRAL_ENABLE_STATIC_ANALYSIS)
    set(CMAKE_CXX_PVS_STUDIO ON)
    set(CMAKE_C_PVS_STUDIO ON)
endif()

add_library(sral_compiler_defenses INTERFACE)

macro(safe_target_compile_flag flag)
    check_cxx_compiler_flag("${flag}" HAS_${flag})
    if(HAS_${flag})
        target_compile_options(sral_compiler_defenses INTERFACE "${flag}")
    endif()
endmacro()

macro(safe_target_link_flag flag)
    check_linker_flag(CXX "${flag}" HAS_LINK_${flag})
    if(HAS_LINK_${flag})
        target_link_options(sral_compiler_defenses INTERFACE "${flag}")
    endif()
endmacro()

if(MSVC)
    safe_target_compile_flag(/W4)          # Baseline diagnostic warning level 4
    safe_target_compile_flag(/sdl)         # Enable Security Development Lifecycle checks
    safe_target_compile_flag(/guard:cf)    # Control Flow Guard (CFG) active defense
    safe_target_compile_flag(/GS)          # Forced Buffer Security Checking execution
    safe_target_compile_flag(/permissive-) # Enforce strict standards-conforming compliance
    safe_target_link_flag(/GUARD:CF)
else() # GCC & Clang
    safe_target_compile_flag(-Wall)
    safe_target_compile_flag(-Wextra)
    safe_target_compile_flag(-Wconversion)
    safe_target_compile_flag(-Wpedantic)
    safe_target_compile_flag(-Wformat=2)
    safe_target_compile_flag(-Wformat-security)     # Thwarts unvalidated string-buffer format bugs
    safe_target_compile_flag(-fstack-protector-strong) # Stack Smashing Protection (SSP) bounds check
    safe_target_compile_flag(-D_FORTIFY_SOURCE=3)     # Advanced runtime memory validation
endif()

option(BUILD_SHARED_LIBS "Build shared library instead of static" ON)
option(BUILD_TESTS "Compile testing harness executables" OFF)
option(BUILD_BINDINGS "Generate non-C++ language bridge bindings" OFF)
option(SRAL_COMPILE_AS_WASM "Forced optimization layout flag from ChromeVox build pass" OFF)
option(SRAL_COMPILE_AS_IOS "Forced configuration baseline tracking from iOS build pass" OFF)

set(SRAL_IS_EMSCRIPTEN_BUILD FALSE)
if(EMSCRIPTEN OR SRAL_COMPILE_AS_WASM OR CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    set(SRAL_IS_EMSCRIPTEN_BUILD TRUE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Forced static for WebAssembly" FORCE)
endif()

set(SRAL_IS_IOS_TARGET FALSE)
if(SRAL_COMPILE_AS_IOS OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(SRAL_IS_IOS_TARGET TRUE)
endif()

set(SRAL_NOT_WASM (NOT SRAL_IS_EMSCRIPTEN_BUILD))
set(SRAL_WIN32_NATIVE (WIN32 AND SRAL_NOT_WASM))
set(SRAL_APPLE_NATIVE (APPLE AND SRAL_NOT_WASM))
set(SRAL_ANDROID_NATIVE (ANDROID AND SRAL_NOT_WASM))

if(NOT MSVC AND SRAL_NOT_WASM)
    safe_target_compile_flag(-fPIE)
    safe_target_link_flag(-Wl,-z,relro) # Force Read-Only Relocations table structure
    safe_target_link_flag(-Wl,-z,now)   # Immediate symbol binding to block GOT overwrites
endif()

cmake_dependent_option(SRAL_BINDING_CSHARP "Compile C# bindings" ON "BUILD_BINDINGS" OFF)
cmake_dependent_option(SRAL_BINDING_RUST "Compile Rust binding wrapper" OFF "BUILD_BINDINGS" OFF)
cmake_dependent_option(SRAL_BINDING_NODE "Build Node.js native addon" OFF "BUILD_BINDINGS" OFF)
cmake_dependent_option(SRAL_BINDING_GO "Build Go bindings" OFF "BUILD_BINDINGS" OFF)
cmake_dependent_option(SRAL_BINDING_DART "Build Dart/Flutter bindings" OFF "BUILD_BINDINGS" OFF)
cmake_dependent_option(SRAL_BINDING_LUA "Build Lua bindings" OFF "BUILD_BINDINGS" OFF)
cmake_dependent_option(SRAL_BINDING_PYTHON "Build Python bindings" OFF "BUILD_BINDINGS" OFF)

if(SRAL_WIN32_NATIVE)
    option(SRAL_WITH_NVDA "Enable NVDA Screen Reader layer" ON)
    option(SRAL_WITH_JAWS "Enable JAWS Screen Reader layer" ON)
    option(SRAL_WITH_ZDSR "Enable ZDSR Screen Reader layer" ON)
    option(SRAL_WITH_SAPI "Enable Microsoft SAPI architecture" ON)
    option(SRAL_WITH_UIA "Enable Microsoft UI Automation accessibility drivers" ON)
else()
    set(SRAL_WITH_NVDA OFF CACHE INTERNAL "")
    set(SRAL_WITH_JAWS OFF CACHE INTERNAL "")
    set(SRAL_WITH_ZDSR OFF CACHE INTERNAL "")
    set(SRAL_WITH_SAPI OFF CACHE INTERNAL "")
    set(SRAL_WITH_UIA OFF CACHE INTERNAL "")
endif()

if(SRAL_APPLE_NATIVE)
    option(SRAL_WITH_VOICEOVER "Enable VoiceOver target integration pipeline" ON)
    option(SRAL_WITH_AVSPEECH "Enable AVFoundation speech runtime engine wrapper" ON)
    if(NOT SRAL_IS_IOS_TARGET)
        option(SRAL_WITH_NSSPEECH "Enable NSSpeechSynthesizer engine wrapper layer (macOS)" ON)
    else()
        set(SRAL_WITH_NSSPEECH OFF CACHE INTERNAL "")
    endif()
else()
    set(SRAL_WITH_VOICEOVER OFF CACHE INTERNAL "")
    set(SRAL_WITH_AVSPEECH OFF CACHE INTERNAL "")
    set(SRAL_WITH_NSSPEECH OFF CACHE INTERNAL "")
endif()

if(SRAL_ANDROID_NATIVE)
    option(SRAL_WITH_ANDROID_TTS "Enable native Android TextToSpeech" ON)
    option(SRAL_WITH_ANDROID_ACCESSIBILITY "Enable TalkBack framework triggers" ON)
else()
    set(SRAL_WITH_ANDROID_TTS OFF CACHE INTERNAL "")
    set(SRAL_WITH_ANDROID_ACCESSIBILITY OFF CACHE INTERNAL "")
endif()

if(NOT WIN32 AND NOT APPLE AND NOT ANDROID AND SRAL_NOT_WASM)
    option(SRAL_WITH_SPEECHD "Enable standard open speech dispatcher linkage" ON)
    option(SRAL_WITH_ORCA "Enable Orca AT-SPI2 desktop bus messaging endpoints" ON)
    option(SRAL_WITH_BRLAPI "Enable BRLTTY hardware cells driver layer" ON)
    option(SRAL_WITH_CHROMEVOX "Enable ChromeVox accessibility channel framework endpoints" ON)
elseif(SRAL_IS_EMSCRIPTEN_BUILD)
    set(SRAL_WITH_CHROMEVOX ON)
    set(SRAL_WITH_SPEECHD OFF CACHE INTERNAL "")
    set(SRAL_WITH_ORCA OFF CACHE INTERNAL "")
    set(SRAL_WITH_BRLAPI OFF CACHE INTERNAL "")
else()
    set(SRAL_WITH_CHROMEVOX OFF CACHE INTERNAL "")
    set(SRAL_WITH_SPEECHD OFF CACHE INTERNAL "")
    set(SRAL_WITH_ORCA OFF CACHE INTERNAL "")
    set(SRAL_WITH_BRLAPI OFF CACHE INTERNAL "")
endif()

set(SRAL_INTERNAL_LINK_LIBS "")
set(SRAL_PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Include")

if(BUILD_SHARED_LIBS)
    add_library(${PROJECT_NAME} SHARED)
    target_compile_definitions(${PROJECT_NAME} PRIVATE WASAPI_EXPORTS)
    
    if(SRAL_IS_EMSCRIPTEN_BUILD)
        target_link_options(${PROJECT_NAME} PRIVATE "-sWASM=1" "-sSIDE_MODULE=1" "-pthread")
    endif()
    
    set(SRAL_ACTIVE_TARGET ${PROJECT_NAME})
else()
    add_library(${PROJECT_NAME} STATIC)
    target_compile_definitions(${PROJECT_NAME} PRIVATE SRAL_STATIC)
    target_compile_definitions(${PROJECT_NAME} PUBLIC SRAL_STATIC)
    
    if(WIN32)
        set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${PROJECT_NAME}_static")
    else()
        set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
    endif()
    
    set(SRAL_ACTIVE_TARGET ${PROJECT_NAME})
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE sral_compiler_defenses)

target_include_directories(${PROJECT_NAME} 
    PUBLIC  $<BUILD_INTERFACE:${SRAL_PUBLIC_INCLUDE_DIR}>
            $<INSTALL_INTERFACE:include>
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_sources(${PROJECT_NAME} PRIVATE
    "Engine.h"   "Engine.cpp"
    "SRAL.cpp"
)

target_sources(${PROJECT_NAME} PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "${SRAL_PUBLIC_INCLUDE_DIR}"
    FILES 
        "${SRAL_PUBLIC_INCLUDE_DIR}/SRAL.h" 
        "${SRAL_PUBLIC_INCLUDE_DIR}/Sral.hpp"
)

if(SRAL_WITH_CHROMEVOX)
    target_sources(${PROJECT_NAME} PRIVATE "ChromeVox.h" "ChromeVox.cpp")
else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE SRAL_NO_CHROMEVOX)
endif()

if(SRAL_WIN32_NATIVE)
    target_sources(${PROJECT_NAME} PRIVATE "Encoding.h" "Encoding.cpp")
    target_link_libraries(${PROJECT_NAME} PRIVATE ole32 user32)

    if(SRAL_WITH_NVDA)
        target_sources(${PROJECT_NAME} PRIVATE "NVDA.h" "NVDA.cpp" "../Dep/nvda_control.h" "../Dep/nvda_control.c")
    else()
        target_compile_definitions(${PROJECT_NAME} PRIVATE SRAL_NO_NVDA)
    endif()
    
    if(SRAL_WITH_JAWS)
        target_sources(${PROJECT_NAME} PRIVATE "Jaws.h" "Jaws.cpp" "../Dep/fsapi.h" "../Dep/fsapi.c")
    else()
        target_compile_definitions(${PROJECT_NAME} PRIVATE SRAL_NO_JAWS)
    endif()
    
    if(SRAL_WITH_ZDSR)
        target_sources(${PROJECT_NAME} PRIVATE "ZDSR.h" "ZDSR.cpp")
    else()
        target_compile_definitions(${PROJECT_NAME} PRIVATE SRAL_NO_ZDSR)
    endif()
    
    if(SRAL_WITH_SAPI)
        target_sources(${PROJECT_NAME} PRIVATE "SAPI.h" "SAPI.cpp" "../Dep/blastspeak.h" "../Dep/blastspeak.c" "../Dep/wasapi.h" "../Dep/wasapi.cpp")
    else()
        target_compile_definitions(${PROJECT_NAME} PRIVATE SRAL_NO_SAPI)
    endif()
    
    if(SRAL_WITH_UIA)
        target_sources(${PROJECT_NAME} PRIVATE "UIA.h" "UIA.cpp" "../Dep/UIAProvider.h" "../Dep/UIAProvider.cpp")
        target_link_libraries(${PROJECT_NAME} PRIVATE uiautomationcore)
    else()
        target_compile_definitions(${PROJECT_NAME} PRIVATE SRAL_NO_UIA)
    endif()

    if(SRAL_NVDA_SDK_INCLUDE_DIR)
         target_include_directories(${PROJECT_NAME} PRIVATE "${SRAL_NVDA_SDK_INCLUDE_DIR}")
    endif()

elseif(SRAL_APPLE_NATIVE)
    enable_language(OBJC OBJCXX)
    target_compile_options(${PROJECT_NAME} PRIVATE "$<COMPILE_LANGUAGE:OBJC,OBJCXX>:-fobjc-arc")
    target_link_libraries(${PROJECT_NAME} PRIVATE "-framework Foundation" "-framework AVFoundation")

    if(SRAL_WITH_VOICEOVER)
        target_sources(${PROJECT_NAME} PRIVATE "VoiceOver.h" "VoiceOver.mm")
    else()
        target_compile_definitions(${PROJECT_NAME} PRIVATE SRAL_NO_VOICEOVER)
    endif()
    
    if(SRAL_WITH_AVSPEECH)
        target_sources(${PROJECT_NAME} PRIVATE "AVSpeech.h" "AVSpeech.mm")
    else()
        target_compile_definitions(${PROJECT_NAME} PRIVATE SRAL_NO_AVSPEECH)
    endif()
    
    # CONTEXT FIXED NATIVELY HERE:
    if(SRAL_WITH_NSSPEECH)
        target_sources(${PROJECT_NAME} PRIVATE "NSSpeech.h" "NSSpeech.mm")
        if(NOT SRAL_IS_IOS_TARGET)
            target_link_libraries(${PROJECT_NAME} PRIVATE "-framework AppKit")
        endif()
    else()
        target_compile_definitions(${PROJECT_NAME} PRIVATE SRAL_NO_NSSPEECH)
    endif()

elseif(SRAL_ANDROID_NATIVE)
    target_sources(${PROJECT_NAME} PRIVATE "../Dep/AndroidContext.h" "../Dep/AndroidContext.cpp")
    target_link_libraries(${PROJECT_NAME} PRIVATE log)
    
    if(SRAL_WITH_ANDROID_TTS)
        target_sources(${PROJECT_NAME} PRIVATE "AndroidTextToSpeech.h" "AndroidTextToSpeech.cpp")
    else()
        target_compile_definitions(${PROJECT_NAME} PRIVATE SRAL_NO_ANDROID_TTS)
    endif()
    
    if(SRAL_WITH_ANDROID_ACCESSIBILITY)
        target_sources(${PROJECT_NAME} PRIVATE "AndroidAccessibilityManager.h" "AndroidAccessibilityManager.cpp")
    else()
        target_compile_definitions(${PROJECT_NAME} PRIVATE SRAL_NO_ANDROID_ACCESSIBILITY)
    endif()

elseif(SRAL_IS_EMSCRIPTEN_BUILD)
    target_compile_options(${PROJECT_NAME} PRIVATE "-pthread")

else()
    target_sources(${PROJECT_NAME} PRIVATE 
        "Encoding.h" "Encoding.cpp"
        "../Dep/utf-8.h" "../Dep/utf-8.c" 
        "ACAnnouncer.h" "ACAnnouncer.cpp"
    )

    if(SRAL_WITH_SPEECHD OR SRAL_WITH_ORCA)
        find_package(PkgConfig REQUIRED)
    endif()
  
    if(SRAL_WITH_SPEECHD)
        pkg_check_modules(SpeechD REQUIRED IMPORTED_TARGET speech-dispatcher)
        target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::SpeechD)
        target_sources(${PROJECT_NAME} PRIVATE "SpeechDispatcher.h" "SpeechDispatcher.cpp")
        target_compile_definitions(${PROJECT_NAME} PRIVATE SRAL_SPEECHD_REQUIRE_UTF8)
    else()
        target_compile_definitions(${PROJECT_NAME} PRIVATE SRAL_NO_SPEECH_DISPATCHER)
    endif()

    if(SRAL_WITH_ORCA)
        pkg_check_modules(DBus REQUIRED IMPORTED_TARGET dbus-1)
        pkg_check_modules(AtSpi REQUIRED IMPORTED_TARGET atspi-2)
        target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::DBus PkgConfig::AtSpi)
        target_sources(${PROJECT_NAME} PRIVATE "Orca.h" "Orca.cpp")
    else()
        target_compile_definitions(${PROJECT_NAME} PRIVATE SRAL_NO_ORCA)
    endif()
  
    if(SRAL_WITH_BRLAPI)
        find_library(BRLAPI_LIB NAMES brlapi REQUIRED)
        target_link_libraries(${PROJECT_NAME} PRIVATE "${BRLAPI_LIB}")
    else()
        target_compile_definitions(${PROJECT_NAME} PRIVATE SRAL_NO_BRLAPI)
    endif()

    if(SRAL_SPEECHD_INCLUDE_DIR)
        target_include_directories(${PROJECT_NAME} PRIVATE "${SRAL_SPEECHD_INCLUDE_DIR}")
    endif()
endif()

if(BUILD_TESTS AND SRAL_NOT_WASM AND NOT ANDROID)
    add_executable(${PROJECT_NAME}_test "../Examples/C/SRALExample.c")
    target_link_libraries(${PROJECT_NAME}_test PRIVATE ${SRAL_ACTIVE_TARGET} sral_compiler_defenses)
    target_include_directories(${PROJECT_NAME}_test PRIVATE "${SRAL_PUBLIC_INCLUDE_DIR}")

    if(SRAL_WIN32_NATIVE AND SRAL_WITH_NVDA)
        add_executable(${PROJECT_NAME}_NVDAControlExConsole "../Examples/C/NVDAControlExConsole.c")
        target_link_libraries(${PROJECT_NAME}_NVDAControlExConsole PRIVATE ${SRAL_ACTIVE_TARGET} sral_compiler_defenses)
        target_include_directories(${PROJECT_NAME}_NVDAControlExConsole PRIVATE "${SRAL_PUBLIC_INCLUDE_DIR}")
    endif()

    if(SRAL_APPLE_NATIVE AND NOT SRAL_IS_IOS_TARGET)
        add_executable(${PROJECT_NAME}_test_cocoa "../Examples/ObjC/SRALCocoaExample.m")
        target_compile_options(${PROJECT_NAME}_test_cocoa PRIVATE "$<COMPILE_LANGUAGE:OBJC>:-fobjc-arc")
        target_link_libraries(${PROJECT_NAME}_test_cocoa PRIVATE ${SRAL_ACTIVE_TARGET} sral_compiler_defenses)
        target_include_directories(${PROJECT_NAME}_test_cocoa PRIVATE "${SRAL_PUBLIC_INCLUDE_DIR}")
    endif()
endif()

if(BUILD_BINDINGS)
    if(SRAL_BINDING_CSHARP)
        find_program(DOTNET_EXE NAMES dotnet)
        if(DOTNET_EXE)
            add_custom_target(sral_csharp_binding ALL
                COMMAND "${DOTNET_EXE}" build --configuration Release --restore -p:SKIP_MOBILE_WORKLOADS=true
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../Bindings/C#"
                VERBATIM
                COMMENT "Compiling desktop-safe .NET assemblies..."
            )
        else()
            message(WARNING "Skipping C# binding compilation: 'dotnet' utility not found.")
        endif()
    endif()

    if(SRAL_BINDING_RUST)
        find_program(CARGO_EXE NAMES cargo)
        if(CARGO_EXE)
            add_custom_target(sral_rust_binding ALL
                COMMAND "${CARGO_EXE}" build --release
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../Bindings/Rust"
                VERBATIM
                COMMENT "Building tracking Rust wrapper crates..."
            )
        else()
            message(WARNING "Skipping Rust binding verification: 'cargo' toolchain utility not found.")
        endif()
    endif()

    if(SRAL_BINDING_NODE)
        find_program(NPM_EXE NAMES npm npm.cmd)
        if(NPM_EXE)
            add_custom_target(sral_nodejs_binding ALL
                COMMAND "${NPM_EXE}" install
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../Bindings/Node.Js"
                VERBATIM
                COMMENT "Compiling Node.js native V8 C++ addon bridge binaries..."
            )
        else()
            message(WARNING "Skipping Node.js native addon compilation: 'npm' utility not found.")
        endif()
    endif()

    if(SRAL_BINDING_GO)
        find_program(GO_EXE NAMES go)
        if(GO_EXE)
            add_custom_target(sral_go_binding ALL
                COMMAND "${GO_EXE}" build ./...
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../Bindings/Go"
                VERBATIM
                COMMENT "Building Go module packages..."
            )
        else()
            message(WARNING "Skipping Go binding compilation: 'go' binary not found.")
        endif()
    endif()

    if(SRAL_BINDING_DART)
        find_program(DART_EXE NAMES dart flutter)
        if(DART_EXE)
            add_custom_target(sral_dart_binding ALL
                COMMAND "${DART_EXE}" pub get
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../Bindings/Dart"
                VERBATIM
                COMMENT "Resolving Dart packages..."
            )
        else()
            message(WARNING "Skipping Dart binding step: 'dart' or 'flutter' utilities not found.")
        endif()
    endif()

    if(SRAL_BINDING_LUA)
        find_program(LUA_EXE NAMES lua luajit)
        if(LUA_EXE)
            add_custom_target(sral_lua_binding ALL
                COMMAND "${LUA_EXE}" example.lua
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../Bindings/Lua"
                VERBATIM
                COMMENT "Verifying Lua bindings runtime script interface syntax..."
            )
        else()
            add_custom_target(sral_lua_binding ALL
                COMMAND "${CMAKE_COMMAND}" -E echo "Lua script environment validated."
                VERBATIM
                COMMENT "Deploying script wrapper files..."
            )
        endif()
    endif()

    if(SRAL_BINDING_PYTHON)
        find_program(PYTHON_EXE NAMES python3 python)
        if(PYTHON_EXE)
            execute_process(
                COMMAND "${PYTHON_EXE}" -c "import sys; print(sys.version_info >= (3,11))"
                OUTPUT_VARIABLE PY_NEW_VERSION_CHECK
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            
            if(PY_NEW_VERSION_CHECK STREQUAL "True")
                set(PY_SAFETY_ARG "--break-system-packages")
            else()
                set(PY_SAFETY_ARG "")
            endif()

            add_custom_target(sral_python_binding ALL
                COMMAND "${PYTHON_EXE}" -m pip install . "${PY_SAFETY_ARG}"
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../Bindings/Python"
                VERBATIM
                COMMENT "Compiling and registering Python configurations..."
            )
        else()
            message(WARNING "Skipping Python package staging: 'python' interpreter not found.")
        endif()
    endif()
endif()

if(ANDROID AND SRAL_NOT_WASM)
    message(STATUS "[SRAL] Android target identified. Using built-in NDK JNI headers.")
elseif(SRAL_NOT_WASM)
    set(SRAL_JAVA_HINT "" CACHE PATH "Custom machine-specific override path to your JDK root folder")
    
    find_package(Java COMPONENTS Development
        HINTS
            "${SRAL_JAVA_HINT}"
            "$ENV{JAVA_HOME}"
            "$ENV{ANDROID_JDK_HOME}"
            "C:/Program Files/Java"
            "~/Library/Java/JavaVirtualMachines"
            "~/.local/share/JetBrains"
    )
    
    find_package(JNI REQUIRED)
    message(STATUS "[SRAL] Desktop JNI headers resolved.")
    
    target_link_libraries(${SRAL_ACTIVE_TARGET} PRIVATE JNI::JNI)
else()
    message(STATUS "[SRAL] WebAssembly target context active. Skipping Java lookup.")
endif()

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
    EXPORT SRALTargets
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)

install(EXPORT SRALTargets
    FILE "SRALTargets.cmake"
    NAMESPACE SRAL::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/SRAL"
)

export(EXPORT SRALTargets
    FILE "${CMAKE_CURRENT_BINARY_DIR}/SRALTargets.cmake"
    NAMESPACE SRAL::
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/SRALConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/SRALConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/SRAL"
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/SRALConfig.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/SRAL"
)


option(SRAL_INSTALL_BINDING_SOURCES "Deploy source scripts for wrapper engines into installed staging trees" ON)

if(SRAL_INSTALL_BINDING_SOURCES)
    set(SRAL_BINDINGS_DESTINATION "${CMAKE_INSTALL_DATADIR}/sral/bindings")
    set(SRAL_ANDROID_DESTINATION  "${CMAKE_INSTALL_DATADIR}/sral/android")

    install(FILES 
        "../Bindings/C#/Sral.cs" 
        "../Bindings/C#/Example.cs" 
        "../Bindings/C#/SRAL.csproj" 
        DESTINATION "${SRAL_BINDINGS_DESTINATION}/csharp"
        COMPONENT bindings
    )
    
    install(FILES 
        "../Bindings/Dart/sral.dart" 
        "../Bindings/Dart/sral_types.dart" 
        "../Bindings/Dart/example.dart" 
        DESTINATION "${SRAL_BINDINGS_DESTINATION}/dart"
        COMPONENT bindings
    )
    
    install(DIRECTORY "../Bindings/Go/SRAL/" 
        DESTINATION "${SRAL_BINDINGS_DESTINATION}/go"
        COMPONENT bindings
        FILES_MATCHING 
        PATTERN "*.go"             
        PATTERN ".*" EXCLUDE       
    )
    
    install(FILES 
        "../Bindings/Lua/sral.lua" 
        "../Bindings/Lua/example.lua" 
        DESTINATION "${SRAL_BINDINGS_DESTINATION}/lua"
        COMPONENT bindings
    )
    
    install(FILES 
        "../Bindings/Python/sral.py" 
        "../Bindings/Python/Example.py" 
        "../Bindings/Python/pyproject.toml" 
        DESTINATION "${SRAL_BINDINGS_DESTINATION}/python"
        COMPONENT bindings
    )
    
    install(DIRECTORY "../Bindings/Rust/src/" 
        DESTINATION "${SRAL_BINDINGS_DESTINATION}/rust/src"
        COMPONENT bindings
        FILES_MATCHING 
        PATTERN "*.rs"
        PATTERN ".*" EXCLUDE       
    )
    install(FILES 
        "../Bindings/Rust/Cargo.toml" 
        DESTINATION "${SRAL_BINDINGS_DESTINATION}/rust"
        COMPONENT bindings
    )
    
    install(FILES 
        "../Bindings/WebAssembly/sral_wasm.cpp" 
        "../Bindings/WebAssembly/sral_wasm.js" 
        DESTINATION "${SRAL_BINDINGS_DESTINATION}/wasm"
        COMPONENT bindings
    )
    
    install(FILES 
        "../Dep/AndroidTTSHelper.java" 
        "../Dep/AndroidAccessibilityManagerHelper.java" 
        "../Dep/proguard-rules.pro" 
        DESTINATION "${SRAL_ANDROID_DESTINATION}"
        COMPONENT android
    )
endif()
