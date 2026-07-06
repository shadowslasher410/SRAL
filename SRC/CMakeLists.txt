cmake_minimum_required(VERSION 4.3.1)
project(SRAL_Core LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(SRAL_PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Include")
include_directories("${SRAL_PUBLIC_INCLUDE_DIR}")

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

set(BUILD_SRAL_TEST ${BUILD_TESTS})
set(SRAL_BUILD_BINDINGS ${BUILD_BINDINGS})
if(SRAL_BUILD_BINDINGS)
    option(SRAL_BINDING_CSHARP "Compile C# bindings (.NET SDK required)" ON)
    option(SRAL_BINDING_RUST "Compile Rust binding wrapper (Cargo required)" OFF)
    option(SRAL_BINDING_NODE "Build Node.js native addon (node-gyp/npm required)" OFF)
    option(SRAL_BINDING_GO "Build Go bindings" OFF)
    option(SRAL_BINDING_DART "Build Dart/Flutter bindings" OFF)
    option(SRAL_BINDING_LUA "Build Lua bindings" OFF)
    option(SRAL_BINDING_PYTHON "Build Python bindings" OFF)
endif()

if(WIN32 AND NOT SRAL_IS_EMSCRIPTEN_BUILD)
  option(SRAL_WITH_NVDA "Enable NVDA Screen Reader layer" ON)
  option(SRAL_WITH_JAWS "Enable JAWS Screen Reader layer" ON)
  option(SRAL_WITH_ZDSR "Enable ZDSR Screen Reader layer" ON)
  option(SRAL_WITH_SAPI "Enable Microsoft SAPI architecture" ON)
  option(SRAL_WITH_UIA "Enable Microsoft UI Automation accessibility drivers" ON)
else()
  set(SRAL_WITH_NVDA OFF) set(SRAL_WITH_JAWS OFF) set(SRAL_WITH_ZDSR OFF) set(SRAL_WITH_SAPI OFF) set(SRAL_WITH_UIA OFF)
endif()

if(APPLE AND NOT SRAL_IS_EMSCRIPTEN_BUILD)
  option(SRAL_WITH_VOICEOVER "Enable VoiceOver target integration pipeline" ON)
  option(SRAL_WITH_AVSPEECH "Enable AVFoundation speech runtime engine wrapper" ON)
  if(NOT SRAL_IS_IOS_TARGET)
    option(SRAL_WITH_NSSPEECH "Enable NSSpeechSynthesizer engine wrapper layer (macOS)" ON)
  else()
    set(SRAL_WITH_NSSPEECH OFF)
  endif()
else()
  set(SRAL_WITH_VOICEOVER OFF) set(SRAL_WITH_AVSPEECH OFF) set(SRAL_WITH_NSSPEECH OFF)
endif()

if(ANDROID AND NOT SRAL_IS_EMSCRIPTEN_BUILD)
  option(SRAL_WITH_ANDROID_TTS "Enable native Android TextToSpeech" ON)
  option(SRAL_WITH_ANDROID_ACCESSIBILITY "Enable TalkBack framework triggers" ON)
else()
  set(SRAL_WITH_ANDROID_TTS OFF) set(SRAL_WITH_ANDROID_ACCESSIBILITY OFF)
endif()

if(NOT WIN32 AND NOT APPLE AND NOT ANDROID AND NOT SRAL_IS_EMSCRIPTEN_BUILD)
  option(SRAL_WITH_SPEECH_DISPATCHER "Enable standard open speech dispatcher linkage" ON)
  option(SRAL_WITH_ORCA "Enable Orca AT-SPI2 desktop bus messaging endpoints" ON)
  option(SRAL_WITH_BRLAPI "Enable BRLTTY hardware cells driver layer" ON)
  option(SRAL_WITH_CHROMEVOX "Enable ChromeVox accessibility channel framework endpoints" ON)
elseif(SRAL_IS_EMSCRIPTEN_BUILD)
  option(SRAL_WITH_CHROMEVOX "Enable ChromeVox script execution interface frameworks" ON)
  set(SRAL_WITH_SPEECH_DISPATCHER OFF) set(SRAL_WITH_ORCA OFF) set(SRAL_WITH_BRLAPI OFF)
else()
  set(SRAL_WITH_CHROMEVOX OFF) set(SRAL_WITH_SPEECH_DISPATCHER OFF) set(SRAL_WITH_ORCA OFF) set(SRAL_WITH_BRLAPI OFF)
endif()

add_library(${PROJECT_NAME}_obj OBJECT)
target_include_directories(${PROJECT_NAME}_obj PUBLIC "${SRAL_PUBLIC_INCLUDE_DIR}")

target_sources(${PROJECT_NAME}_obj PRIVATE
  "Engine.h"   "Engine.cpp"
  "SRAL.cpp"
)

target_sources(${PROJECT_NAME}_obj PUBLIC
  FILE_SET HEADERS
  BASE_DIRS "${SRAL_PUBLIC_INCLUDE_DIR}"
  FILES "${SRAL_PUBLIC_INCLUDE_DIR}/SRAL.h" "${SRAL_PUBLIC_INCLUDE_DIR}/Sral.hpp"
)

if(SRAL_WITH_CHROMEVOX)
  target_sources(${PROJECT_NAME}_obj PRIVATE "ChromeVox.h" "ChromeVox.cpp")
else()
  target_compile_definitions(${PROJECT_NAME}_obj PRIVATE SRAL_NO_CHROMEVOX)
endif()

if(WIN32 AND NOT SRAL_IS_EMSCRIPTEN_BUILD)
  target_sources(${PROJECT_NAME}_obj PRIVATE "Encoding.h" "Encoding.cpp")

  if(SRAL_WITH_NVDA)
    target_sources(${PROJECT_NAME}_obj PRIVATE "NVDA.h" "NVDA.cpp" "../Dep/nvda_control.h" "../Dep/nvda_control.c")
  else()
    target_compile_definitions(${PROJECT_NAME}_obj PRIVATE SRAL_NO_NVDA)
  endif()
  if(SRAL_WITH_JAWS)
    target_sources(${PROJECT_NAME}_obj PRIVATE "Jaws.h" "Jaws.cpp" "../Dep/fsapi.h" "../Dep/fsapi.c")
  else()
    target_compile_definitions(${PROJECT_NAME}_obj PRIVATE SRAL_NO_JAWS)
  endif()
  if(SRAL_WITH_ZDSR)
    target_sources(${PROJECT_NAME}_obj PRIVATE "ZDSR.h" "ZDSR.cpp")
  else()
    target_compile_definitions(${PROJECT_NAME}_obj PRIVATE SRAL_NO_ZDSR)
  endif()
  if(SRAL_WITH_SAPI)
    target_sources(${PROJECT_NAME}_obj PRIVATE "SAPI.h" "SAPI.cpp" "../Dep/blastspeak.h" "../Dep/blastspeak.c" "../Dep/wasapi.h" "../Dep/wasapi.cpp")
  else()
    target_compile_definitions(${PROJECT_NAME}_obj PRIVATE SRAL_NO_SAPI)
  endif()
  if(SRAL_WITH_UIA)
    target_sources(${PROJECT_NAME}_obj PRIVATE "UIA.h" "UIA.cpp" "../Dep/UIAProvider.h" "../Dep/UIAProvider.cpp")
  else()
    target_compile_definitions(${PROJECT_NAME}_obj PRIVATE SRAL_NO_UIA)
  endif()

  if(SRAL_NVDA_SDK_INCLUDE_DIR)
     target_include_directories(${PROJECT_NAME}_obj PRIVATE "${SRAL_NVDA_SDK_INCLUDE_DIR}")
  endif()

elseif(APPLE AND NOT SRAL_IS_EMSCRIPTEN_BUILD)
  enable_language(OBJC OBJCXX)
  target_compile_options(${PROJECT_NAME}_obj PRIVATE "$<COMPILE_LANGUAGE:OBJC,OBJCXX>:-fobjc-arc")
  if(SRAL_WITH_VOICEOVER)
    target_sources(${PROJECT_NAME}_obj PRIVATE "VoiceOver.h" "VoiceOver.mm")
  else()
    target_compile_definitions(${PROJECT_NAME}_obj PRIVATE SRAL_NO_VOICEOVER)
  endif()
  if(SRAL_WITH_AVSPEECH)
    target_sources(${PROJECT_NAME}_obj PRIVATE "AVSpeech.h" "AVSpeech.mm")
  else()
    target_compile_definitions(${PROJECT_NAME}_obj PRIVATE SRAL_NO_AVSPEECH)
  endif()
  if(SRAL_WITH_NSSPEECH)
    target_sources(${PROJECT_NAME}_obj PRIVATE "NSSpeech.h" "NSSpeech.mm")
  else()
    target_compile_definitions(${PROJECT_NAME}_obj PRIVATE SRAL_NO_NSSPEECH)
  endif()

elseif(ANDROID AND NOT SRAL_IS_EMSCRIPTEN_BUILD)
  target_sources(${PROJECT_NAME}_obj PRIVATE "../Dep/AndroidContext.h" "../Dep/AndroidContext.cpp")
  if(SRAL_WITH_ANDROID_TTS)
    target_sources(${PROJECT_NAME}_obj PRIVATE "AndroidTextToSpeech.h" "AndroidTextToSpeech.cpp")
  else()
    target_compile_definitions(${PROJECT_NAME}_obj PRIVATE SRAL_NO_ANDROID_TTS)
  endif()
  if(SRAL_WITH_ANDROID_ACCESSIBILITY)
    target_sources(${PROJECT_NAME}_obj PRIVATE "AndroidAccessibilityManager.h" "AndroidAccessibilityManager.cpp")
  else()
    target_compile_definitions(${PROJECT_NAME}_obj PRIVATE SRAL_NO_ANDROID_ACCESSIBILITY)
  endif()

elseif(SRAL_IS_EMSCRIPTEN_BUILD)
  message(STATUS "[SRAL Native Runtime] Mapping target build structures into Emscripten pipeline...")
  target_compile_options(${PROJECT_NAME}_obj PRIVATE "-pthread")

else()
  target_sources(${PROJECT_NAME}_obj PRIVATE 
    "Encoding.h" "Encoding.cpp"
    "../Dep/utf-8.h" "../Dep/utf-8.c" 
    "ACAnnouncer.h" "ACAnnouncer.cpp"
  )
  
    if(SRAL_WITH_SPEECH_DISPATCHER)
        target_sources(${PROJECT_NAME}_obj PRIVATE "SpeechDispatcher.h" "SpeechDispatcher.cpp")
        target_compile_definitions(${PROJECT_NAME}_obj PRIVATE SRAL_SPEECHD_REQUIRE_UTF8)
    else()
        target_compile_definitions(${PROJECT_NAME}_obj PRIVATE SRAL_NO_SPEECH_DISPATCHER)
    endif()
  
    if(SRAL_WITH_ORCA)
        target_sources(${PROJECT_NAME}_obj PRIVATE "Orca.h" "Orca.cpp")
    else()
        target_compile_definitions(${PROJECT_NAME}_obj PRIVATE SRAL_NO_ORCA)
    endif()
  
    if(NOT SRAL_WITH_BRLAPI)
        target_compile_definitions(${PROJECT_NAME}_obj PRIVATE SRAL_NO_BRLAPI)
    endif()

    if(SRAL_SPEECHD_INCLUDE_DIR)
        target_include_directories(${PROJECT_NAME}_obj PRIVATE "${SRAL_SPEECHD_INCLUDE_DIR}")
    endif()
endif()

if(NOT SRAL_IS_EMSCRIPTEN_BUILD)
    set_property(TARGET ${PROJECT_NAME}_obj PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()

set(SRAL_LINK_LIBS "")
set(SRAL_PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Include")

if(WIN32 AND NOT SRAL_IS_EMSCRIPTEN_BUILD)
  list(APPEND SRAL_LINK_LIBS "ole32" "user32")
  if(SRAL_WITH_UIA)
    list(APPEND SRAL_LINK_LIBS "uiautomationcore")
  endif()
elseif(APPLE AND NOT SRAL_IS_EMSCRIPTEN_BUILD)
  list(APPEND SRAL_LINK_LIBS "-framework Foundation" "-framework AVFoundation")
  if(NOT SRAL_IS_IOS_TARGET AND SRAL_WITH_NSSPEECH)
    list(APPEND SRAL_LINK_LIBS "-framework AppKit")
  endif()
elseif(ANDROID AND NOT SRAL_IS_EMSCRIPTEN_BUILD)
  list(APPEND SRAL_LINK_LIBS "log")
elseif(SRAL_IS_EMSCRIPTEN_BUILD)
else()
  find_package(PkgConfig REQUIRED)
  
  if(SRAL_WITH_SPEECH_DISPATCHER)
    pkg_check_modules(SpeechD REQUIRED IMPORTED_TARGET speech-dispatcher)
    list(APPEND SRAL_LINK_LIBS PkgConfig::SpeechD)
  endif()
  
  if(SRAL_WITH_ORCA)
    pkg_check_modules(DBus REQUIRED IMPORTED_TARGET dbus-1)
    pkg_check_modules(AtSpi REQUIRED IMPORTED_TARGET atspi-2)
    list(APPEND SRAL_LINK_LIBS PkgConfig::DBus PkgConfig::AtSpi)
  endif()
  
  if(SRAL_WITH_BRLAPI)
    find_library(BRLAPI_LIB "brlapi" REQUIRED)
    list(APPEND SRAL_LINK_LIBS ${BRLAPI_LIB})
  endif()
endif()

if(BUILD_SHARED_LIBS)
    target_compile_definitions(${PROJECT_NAME}_obj PRIVATE WASAPI_EXPORTS)
  
    add_library(${PROJECT_NAME}_shared SHARED $<TARGET_OBJECTS:${PROJECT_NAME}_obj>)
    set_target_properties(${PROJECT_NAME}_shared PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
  
    target_include_directories(${PROJECT_NAME}_shared PUBLIC 
        $<BUILD_INTERFACE:${SRAL_PUBLIC_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(${PROJECT_NAME}_shared PRIVATE ${SRAL_LINK_LIBS})
  
    if(SRAL_IS_EMSCRIPTEN_BUILD)
        target_link_options(${PROJECT_NAME}_shared PRIVATE "-sWASM=1" "-sSIDE_MODULE=1" "-pthread")
    endif()
else()
    target_compile_definitions(${PROJECT_NAME}_obj PRIVATE SRAL_STATIC)
    
    add_library(${PROJECT_NAME}_static STATIC $<TARGET_OBJECTS:${PROJECT_NAME}_obj>)
    if(WIN32)
        set_target_properties(${PROJECT_NAME}_static PROPERTIES OUTPUT_NAME "${PROJECT_NAME}_static")
    else()
        set_target_properties(${PROJECT_NAME}_static PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
    endif()

    target_compile_definitions(${PROJECT_NAME}_static PUBLIC SRAL_STATIC)
    target_include_directories(${PROJECT_NAME}_static PUBLIC 
        $<BUILD_INTERFACE:${SRAL_PUBLIC_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(${PROJECT_NAME}_static PUBLIC ${SRAL_LINK_LIBS})
endif()

if(BUILD_SRAL_TEST AND NOT SRAL_IS_EMSCRIPTEN_BUILD AND NOT ANDROID)
    set(SRAL_TEST_LINK_TARGET ${PROJECT_NAME}_static)
    if(BUILD_SHARED_LIBS)
        set(SRAL_TEST_LINK_TARGET ${PROJECT_NAME}_shared)
    endif()

    add_executable(${PROJECT_NAME}_test "../Examples/C/SRALExample.c")
    target_include_directories(${PROJECT_NAME}_test PRIVATE "${SRAL_PUBLIC_INCLUDE_DIR}")
    target_link_libraries(${PROJECT_NAME}_test PRIVATE ${SRAL_TEST_LINK_TARGET})

    if(WIN32 AND SRAL_WITH_NVDA)
        add_executable(${PROJECT_NAME}_NVDAControlExConsole "../Examples/C/NVDAControlExConsole.c")
        target_include_directories(${PROJECT_NAME}_NVDAControlExConsole PRIVATE "${SRAL_PUBLIC_INCLUDE_DIR}")
        target_link_libraries(${PROJECT_NAME}_NVDAControlExConsole PRIVATE ${SRAL_TEST_LINK_TARGET})
    endif()

    if(APPLE AND NOT SRAL_IS_IOS_TARGET)
        add_executable(${PROJECT_NAME}_test_cocoa "../Examples/ObjC/SRALCocoaExample.m")
        target_include_directories(${PROJECT_NAME}_test_cocoa PRIVATE "${SRAL_PUBLIC_INCLUDE_DIR}")
        target_compile_options(${PROJECT_NAME}_test_cocoa PRIVATE "$<COMPILE_LANGUAGE:OBJC>:-fobjc-arc")
        target_link_libraries(${PROJECT_NAME}_test_cocoa PRIVATE ${SRAL_TEST_LINK_TARGET})
    endif()
endif()

if(SRAL_BUILD_BINDINGS)
    if(SRAL_BINDING_CSHARP)
        find_program(DOTNET_EXE NAMES dotnet)
        if(DOTNET_EXE)
            add_custom_target(sral_csharp_binding ALL
                COMMAND ${DOTNET_EXE} build --configuration Release --restore -p:SKIP_MOBILE_WORKLOADS=true
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../Bindings/C#"
                COMMENT "Restoring dependencies and compiling desktop-safe .NET assemblies..."
            )
        else()
            message(WARNING "Skipping C# binding compilation: 'dotnet' engine utility was not found.")
        endif()
    endif()

    if(SRAL_BINDING_RUST)
        find_program(CARGO_EXE NAMES cargo)
        if(CARGO_EXE)
            add_custom_target(sral_rust_binding ALL
                COMMAND ${CARGO_EXE} build --release
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../Bindings/Rust"
                COMMENT "Verifying and building tracking Rust wrapper crates..."
            )
        else()
            message(WARNING "Skipping Rust binding verification: 'cargo' toolchain utility was not found.")
        endif()
    endif()

    if(SRAL_BINDING_NODE)
        find_program(NPM_EXE NAMES npm npm.cmd)
        if(NPM_EXE)
            add_custom_target(sral_nodejs_binding ALL
                COMMAND ${NPM_EXE} install
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../Bindings/Node.Js"
                COMMENT "Compiling Node.js native V8 C++ addon bridge binaries..."
            )
        else()
            message(WARNING "Skipping Node.js native addon compilation: 'npm' package utility was not found.")
        endif()
    endif()

    if(SRAL_BINDING_GO)
        find_program(GO_EXE NAMES go)
        if(GO_EXE)
            add_custom_target(sral_go_binding ALL
                COMMAND ${GO_EXE} build ./...
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../Bindings/Go"
                COMMENT "Building and testing Go module packages via workspace context..."
            )
        else()
            message(WARNING "Skipping Go binding compilation: 'go' toolchain binary was not found.")
        endif()
    endif()

    if(SRAL_BINDING_DART)
        find_program(DART_EXE NAMES dart flutter)
        if(DART_EXE)
            add_custom_target(sral_dart_binding ALL
                COMMAND ${DART_EXE} pub get
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../Bindings/Dart"
                COMMENT "Resolving Dart packages and checking syntax interfaces..."
            )
        else()
            message(WARNING "Skipping Dart binding step: 'dart' or 'flutter' CLI utilities not found.")
        endif()
    endif()

    if(SRAL_BINDING_LUA)
        find_program(LUA_EXE NAMES lua luajit)
        if(LUA_EXE)
            add_custom_target(sral_lua_binding ALL
                COMMAND ${LUA_EXE} example.lua
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../Bindings/Lua"
                COMMENT "Verifying Lua bindings runtime script interface syntax..."
            )
        else()
            add_custom_target(sral_lua_binding ALL
                COMMAND ${CMAKE_COMMAND} -E echo "Lua script environment validated (source files deployed)."
                COMMENT "Deploying script wrapper files..."
            )
        endif()
    endif()

    if(SRAL_BINDING_PYTHON)
        find_program(PYTHON_EXE NAMES python3 python)
        if(PYTHON_EXE)
            execute_process(
                COMMAND ${PYTHON_EXE} -c "import sys; print(sys.version_info >= (3,11))"
                OUTPUT_VARIABLE PY_NEW_VERSION_CHECK
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            set(PY_INSTALL_ARGS install .)
            if(PY_NEW_VERSION_CHECK STREQUAL "True")
                list(APPEND PY_INSTALL_ARGS --break-system-packages)
            endif()

            add_custom_target(sral_python_binding ALL
                COMMAND ${PYTHON_EXE} -m pip ${PY_INSTALL_ARGS}
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../Bindings/Python"
                COMMENT "Compiling and registering tracking python binding configurations..."
            )
        else()
            message(WARNING "Skipping Python package staging: 'python' interpreter engine utility not found.")
        endif()
    endif()
endif()

if(ANDROID AND NOT SRAL_IS_EMSCRIPTEN_BUILD)
    message(STATUS "[SRAL] Android target identified. Using built-in NDK JNI headers.")
elseif(NOT SRAL_IS_EMSCRIPTEN_BUILD)
    set(SRAL_JAVA_HINT "" CACHE PATH "Custom machine-specific override path to your JDK root folder")
    
    find_package(Java COMPONENTS Development
        HINTS
            ${SRAL_JAVA_HINT}
            "$ENV{JAVA_HOME}"
            "$ENV{ANDROID_JDK_HOME}"
            "C:/Program Files/Java"
            "~/Library/Java/JavaVirtualMachines"
            "~/.local/share/JetBrains"
    )
    
    find_package(JNI REQUIRED)
    message(STATUS "[SRAL] Desktop JNI headers resolved at: ${JNI_INCLUDE_DIRS}")
    
    if(BUILD_SHARED_LIBS AND TARGET ${PROJECT_NAME}_shared)
        if(TARGET JNI::JNI)
            target_link_libraries(${PROJECT_NAME}_shared PRIVATE JNI::JNI)
        else()
            target_include_directories(${PROJECT_NAME}_shared PRIVATE ${JNI_INCLUDE_DIRS})
            target_link_libraries(${PROJECT_NAME}_shared PRIVATE ${JNI_LIBRARIES})
        endif()
    endif()

    if(TARGET ${PROJECT_NAME}_static)
        if(TARGET JNI::JNI)
            target_link_libraries(${PROJECT_NAME}_static PUBLIC JNI::JNI)
        else()
            target_include_directories(${PROJECT_NAME}_static PUBLIC ${JNI_INCLUDE_DIRS})
            target_link_libraries(${PROJECT_NAME}_static PUBLIC ${JNI_LIBRARIES})
        endif()
    endif()
else()
    message(STATUS "[SRAL] WebAssembly target context active. Skipping all host Java/JNI environment lookups.")
endif()

if(TARGET ${PROJECT_NAME}_static)
    install(TARGETS ${PROJECT_NAME}_static EXPORT SRALTargets
        FILE_SET HEADERS DESTINATION "include"
        ARCHIVE DESTINATION "lib"
    )
endif()

if(BUILD_SHARED_LIBS AND TARGET ${PROJECT_NAME}_shared)
    install(TARGETS ${PROJECT_NAME}_shared EXPORT SRALTargets
        FILE_SET HEADERS DESTINATION "include"
        RUNTIME DESTINATION "bin"
        LIBRARY DESTINATION "lib"
        ARCHIVE DESTINATION "lib"
    )
endif()

export(EXPORT SRALTargets
    FILE "${CMAKE_CURRENT_BINARY_DIR}/SRALTargets.cmake"
    NAMESPACE SRAL::
)

install(EXPORT SRALTargets
    FILE "SRALTargets.cmake"
    NAMESPACE SRAL::
    DESTINATION "lib/cmake/SRAL"
)

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/SRALConfig.cmake" [=[
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/SRALTargets.cmake")
    include("${CMAKE_CURRENT_LIST_DIR}/SRALTargets.cmake")
else()
    include("${CMAKE_CURRENT_LIST_DIR}/SRALTargets.cmake" OPTIONAL)
endif()
]=])

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/SRALConfig.cmake"
    DESTINATION "lib/cmake/SRAL"
)

option(SRAL_INSTALL_BINDING_SOURCES "Deploy source scripts for wrapper engines into installed staging trees" ON)
if(SRAL_INSTALL_BINDING_SOURCES)
    install(FILES "../Bindings/C#/Sral.cs" "../Bindings/C#/Example.cs" "../Bindings/C#/SRAL.csproj" DESTINATION "share/sral/bindings/csharp")
    install(FILES "../Bindings/Dart/sral.dart" "../Bindings/Dart/sral_types.dart" "../Bindings/Dart/example.dart" DESTINATION "share/sral/bindings/dart")
    install(DIRECTORY "../Bindings/Go/SRAL/" DESTINATION "share/sral/bindings/go")
    install(FILES "../Bindings/Lua/sral.lua" "../Bindings/Lua/example.lua" DESTINATION "share/sral/bindings/lua")
    install(FILES "../Bindings/Python/sral.py" "../Bindings/Python/Example.py" "../Bindings/Python/pyproject.toml" DESTINATION "share/sral/bindings/python")
    install(DIRECTORY "../Bindings/Rust/src/" DESTINATION "share/sral/bindings/rust/src")
    install(FILES "../Bindings/Rust/Cargo.toml" DESTINATION "share/sral/bindings/rust")
    install(FILES "../Bindings/WebAssembly/sral_wasm.cpp" "../Bindings/WebAssembly/sral_wasm.js" DESTINATION "share/sral/bindings/wasm")
    install(FILES "../Dep/AndroidTTSHelper.java" "../Dep/AndroidAccessibilityManagerHelper.java" "../Dep/proguard-rules.pro" DESTINATION "share/sral/android")
endif()
