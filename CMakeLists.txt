cmake_minimum_required(VERSION 4.3.1)

set(SRAL_IS_EMSCRIPTEN_BUILD FALSE)
if(EMSCRIPTEN OR CMAKE_TOOLCHAIN_FILE MATCHES "Emscripten.cmake" OR CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    set(SRAL_IS_EMSCRIPTEN_BUILD TRUE)
endif()

if(SRAL_IS_EMSCRIPTEN_BUILD)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Forced static library route for WebAssembly target" FORCE)
endif()

if(SRAL_IS_EMSCRIPTEN_BUILD)
    option(SRAL_AUTO_SETUP_EMSDK "Automatically download and bootstrap the Emscripten SDK if missing" ON)
    
    if(SRAL_AUTO_SETUP_EMSDK AND NOT SRAL_EMSDK_REEXEC_DONE AND NOT CMAKE_C_COMPILER_LOADED)
        message(STATUS "[SRAL] Emscripten environment missing or unconfigured. Commencing cross-platform auto-setup...")
        
        set(CMAKE_TLS_VERIFY ON)
        set(CMAKE_TLS_CAINFO "" CACHE FILEPATH "Path to TLS CA trust bundle (leave empty for system default)")
        
        find_package(Python3 3.8 COMPONENTS Interpreter QUIET)
        if(Python3_Interpreter_FOUND)
            set(SRAL_PYTHON_EXE ${Python3_EXECUTABLE})
        else()
            if(WIN32)
                set(PYTHON_VERSION_MAJOR 3)
                set(PYTHON_VERSION_MINOR 14)
                set(PYTHON_VERSION_PATCH 6)
                set(PYTHON_VER_FULL ${PYTHON_VERSION_MAJOR}.${PYTHON_VERSION_MINOR}.${PYTHON_VERSION_PATCH})
                set(PYTHON_VER_SHORT ${PYTHON_VERSION_MAJOR}${PYTHON_VERSION_MINOR})
                
                message(STATUS "[SRAL] Downloading pinned Python v${PYTHON_VER_FULL}...")
                include(FetchContent)
                
                FetchContent_Declare(
                    portable_python
                    URL "https://python.org{PYTHON_VER_FULL}/python-${PYTHON_VER_FULL}-embed-amd64.zip"
                    URL_HASH SHA256=e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
                    DOWNLOAD_NO_EXTRACT FALSE
                )
                FetchContent_MakeAvailable(portable_python)
                
                set(SRAL_PYTHON_EXE "${portable_python_SOURCE_DIR}/python.exe")
                set(PYTHON_PTH_FILE "${portable_python_SOURCE_DIR}/python${PYTHON_VER_SHORT}._pth")
                if(EXISTS "${PYTHON_PTH_FILE}")
                    file(WRITE "${PYTHON_PTH_FILE}" ".\npython${PYTHON_VER_SHORT}.zip\nimport site\n")
                endif()
            else()
                message(FATAL_ERROR "[SRAL] Build Dependency Missing: Python 3.8+ is required by Emscripten.")
            endif()
        endif()
        
        message(STATUS "[SRAL] Bootstrapping emsdk from GitHub...")
        include(FetchContent)
        
        FetchContent_Declare(
            emsdk_toolchain
            GIT_REPOSITORY "https://github.com/emscripten-core/emsdk"
            GIT_TAG "6.0.2"
			GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(emsdk_toolchain)
        
        set(_EMSDK_DIR "${emsdk_toolchain_SOURCE_DIR}")
        
        if(NOT WIN32)
            execute_process(
                COMMAND chmod +x emsdk.py
                WORKING_DIRECTORY "${_EMSDK_DIR}"
                OUTPUT_QUIET ERROR_QUIET
            )
        endif()
        
        execute_process(
            COMMAND "${SRAL_PYTHON_EXE}" emsdk.py install 6.0.2
            WORKING_DIRECTORY "${_EMSDK_DIR}"
            RESULT_VARIABLE _INSTALL_RES
            OUTPUT_VARIABLE _INSTALL_OUT
            ERROR_VARIABLE _INSTALL_ERR
        )
        if(NOT _INSTALL_RES EQUAL 0)
            message(FATAL_ERROR "[SRAL] emsdk install failed:\nLog: ${_INSTALL_OUT}\nError: ${_INSTALL_ERR}")
        endif()

        execute_process(
            COMMAND "${SRAL_PYTHON_EXE}" emsdk.py activate 6.0.2
            WORKING_DIRECTORY "${_EMSDK_DIR}"
            RESULT_VARIABLE _ACTIVATE_RES
            OUTPUT_VARIABLE _ACTIVATE_OUT
            ERROR_VARIABLE _ACTIVATE_ERR
        )
        if(NOT _ACTIVATE_RES EQUAL 0)
            message(FATAL_ERROR "[SRAL] emsdk activation failed:\nLog: ${_ACTIVATE_OUT}\nError: ${_ACTIVATE_ERR}")
        endif()
        
        file(TO_CMAKE_PATH "${_EMSDK_DIR}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake" DOWNLOADED_EMSDK_CMAKE)
        
        if(EXISTS "${DOWNLOADED_EMSDK_CMAKE}")
            message(STATUS "[SRAL] Emscripten bootstrapped! Forcing generation pass...")
            
            set(CMAKE_TOOLCHAIN_FILE "${DOWNLOADED_EMSDK_CMAKE}" CACHE FILEPATH "Emscripten Toolchain File" FORCE)
            set(SRAL_EMSDK_REEXEC_DONE ON CACHE INTERNAL "Re-exec tracking loop flag" FORCE)
            
            set(_REEXEC_ARGS
                -G "${CMAKE_GENERATOR}"
                "-DCMAKE_TOOLCHAIN_FILE=${DOWNLOADED_EMSDK_CMAKE}"
                -DSRAL_EMSDK_REEXEC_DONE=ON
                -DBUILD_SHARED_LIBS=OFF
            )

            if(CMAKE_MAKE_PROGRAM)
                list(APPEND _REEXEC_ARGS "-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}")
            endif()
            
            if(CMAKE_BUILD_TYPE)
                list(APPEND _REEXEC_ARGS "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")
            endif()

            if(BUILD_SRAL_TEST)
                list(APPEND _REEXEC_ARGS "-DBUILD_SRAL_TEST=${BUILD_SRAL_TEST}")
            endif()
            if(SRAL_WITH_CHROMEVOX)
                list(APPEND _REEXEC_ARGS "-DSRAL_WITH_CHROMEVOX=${SRAL_WITH_CHROMEVOX}")
            endif()
            
            execute_process(
                COMMAND "${CMAKE_COMMAND}" ${_REEXEC_ARGS} "${CMAKE_SOURCE_DIR}"
                WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
                RESULT_VARIABLE _REEXEC_RES
            )
            
            if(_REEXEC_RES EQUAL 0)
                return() 
            else()
                message(FATAL_ERROR "[SRAL] Automatic toolchain configuration failed during build system re-generation.")
            endif()
        else()
            message(FATAL_ERROR "[SRAL] Emscripten toolchain file missing after setup attempt.")
        endif()
    endif()
endif()

project("SRAL" LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(FetchContent)

if(NOT CMAKE_MSVC_RUNTIME_LIBRARY)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

set(INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/Include")

if(MSVC)
  if(POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
  endif()
endif()

# CORE COMPILATION OPTIONS & PLATFORM LOGIC SWITCHES
option(BUILD_SHARED_LIBS "Build shared library (.dll/.so/.dylib/.wasm)" ON)
option(BUILD_SRAL_TEST "Build SRAL examples/tests" ON)
option(SRAL_BUILD_BINDINGS "Enable management and compilation hooks for language bindings" OFF)

if(SRAL_BUILD_BINDINGS)
  option(SRAL_BINDING_CSHARP "Compile C# bindings (.NET SDK required)" ON)
  option(SRAL_BINDING_RUST   "Compile Rust binding wrapper (Cargo required)" OFF)
  option(SRAL_BINDING_NODE   "Build Node.js native addon (node-gyp/npm required)" OFF)
endif()

# --- Windows Features ---
if(WIN32 AND NOT SRAL_IS_EMSCRIPTEN_BUILD)
  option(SRAL_WITH_NVDA        "Enable NVDA Screen Reader backend" ON)
  option(SRAL_WITH_JAWS        "Enable JAWS Screen Reader backend" ON)
  option(SRAL_WITH_ZDSR        "Enable ZDSR Screen Reader backend" ON)
  option(SRAL_WITH_SAPI        "Enable Microsoft SAPI backend" ON)
  option(SRAL_WITH_UIA         "Enable Microsoft UI Automation (UIA) support" ON)
else()
  set(SRAL_WITH_NVDA OFF)
  set(SRAL_WITH_JAWS OFF)
  set(SRAL_WITH_ZDSR OFF)
  set(SRAL_WITH_SAPI OFF)
  set(SRAL_WITH_UIA OFF)
endif()

# --- Apple (macOS Desktop / iOS Mobile) Features ---
if(APPLE AND NOT SRAL_IS_EMSCRIPTEN_BUILD)
  if(CMAKE_SYSTEM_NAME STREQUAL "iOS" OR CMAKE_SYSTEM_NAME STREQUAL "tvOS" OR CMAKE_SYSTEM_NAME STREQUAL "watchOS" OR TARGET_OS_IPHONE)
    set(SRAL_IS_IOS_TARGET TRUE)
  else()
    set(SRAL_IS_IOS_TARGET FALSE)
  endif()
  option(SRAL_WITH_VOICEOVER "Enable VoiceOver accessibility channel target support" ON)
  option(SRAL_WITH_AVSPEECH  "Enable AVFoundation AVSpeechSynthesizer engine integration" ON)
  if(NOT SRAL_IS_IOS_TARGET)
    option(SRAL_WITH_NSSPEECH "Enable older legacy NSSpeechSynthesizer engine context (macOS-only)" ON)
  else()
    set(SRAL_WITH_NSSPEECH OFF)
  endif()
else()
  set(SRAL_WITH_VOICEOVER OFF)
  set(SRAL_WITH_AVSPEECH OFF)
  set(SRAL_WITH_NSSPEECH OFF)
endif()

# --- Android Features ---
if(ANDROID AND NOT SRAL_IS_EMSCRIPTEN_BUILD)
  option(SRAL_WITH_ANDROID_TTS "Enable native Android TextToSpeech engine wrappers" ON)
  option(SRAL_WITH_ANDROID_ACCESSIBILITY "Enable TalkBack framework AccessibilityManager triggers" ON)
else()
  set(SRAL_WITH_ANDROID_TTS OFF)
  set(SRAL_WITH_ANDROID_ACCESSIBILITY OFF)
endif()

# --- Linux & WebAssembly (Emscripten) Features ---
if(NOT WIN32 AND NOT APPLE AND NOT ANDROID AND NOT SRAL_IS_EMSCRIPTEN_BUILD)
  option(SRAL_WITH_SPEECH_DISPATCHER "Enable standard open system Speech Dispatcher socket pipelines" ON)
  option(SRAL_WITH_ORCA              "Enable Orca AT-SPI2 desktop bus messaging endpoints" ON)
  option(SRAL_WITH_BRLAPI            "Enable BRLTTY hardware cell line braille device layers" ON)
  option(SRAL_WITH_CHROMEVOX         "Enable ChromeVox system accessibility message channels (ChromeOS/Linux)" ON)
elseif(SRAL_IS_EMSCRIPTEN_BUILD)
  option(SRAL_WITH_CHROMEVOX         "Enable ChromeVox engine interfaces via extension scripting scopes" ON)
  set(SRAL_WITH_SPEECH_DISPATCHER OFF)
  set(SRAL_WITH_ORCA OFF)
  set(SRAL_WITH_BRLAPI OFF)
else()
  set(SRAL_WITH_CHROMEVOX OFF)
  set(SRAL_WITH_SPEECH_DISPATCHER OFF)
  set(SRAL_WITH_ORCA OFF)
  set(SRAL_WITH_BRLAPI OFF)
endif()


# OBJECT TARGET ARCHITECTURE & FILES
add_library(${PROJECT_NAME}_obj OBJECT)
target_include_directories(${PROJECT_NAME}_obj PUBLIC "${INCLUDES}")

target_sources(${PROJECT_NAME}_obj PRIVATE
  "SRC/Encoding.h" "SRC/Encoding.cpp"
  "SRC/Engine.h"   "SRC/Engine.cpp"
  "SRC/SRAL.cpp"
)

target_sources(${PROJECT_NAME}_obj PUBLIC
  FILE_SET HEADERS
  BASE_DIRS "${INCLUDES}"
  FILES "${INCLUDES}/SRAL.h" "${INCLUDES}/Sral.hpp"
)

if(SRAL_WITH_CHROMEVOX)
  target_sources(${PROJECT_NAME}_obj PRIVATE "SRC/ChromeVox.h" "SRC/ChromeVox.cpp")
else()
  target_compile_definitions(${PROJECT_NAME}_obj PRIVATE SRAL_NO_CHROMEVOX)
endif()

# --- Platform Specific Source Mapping ---
if(WIN32 AND NOT SRAL_IS_EMSCRIPTEN_BUILD)
  target_compile_definitions(${PROJECT_NAME}_obj PRIVATE _CRT_SECURE_NO_WARNINGS)
  if(SRAL_WITH_NVDA)
    target_sources(${PROJECT_NAME}_obj PRIVATE "SRC/NVDA.h" "SRC/NVDA.cpp" "Dep/nvda_control.h" "Dep/nvda_control.c")
  else()
    target_compile_definitions(${PROJECT_NAME}_obj PRIVATE SRAL_NO_NVDA)
  endif()
  if(SRAL_WITH_JAWS)
    target_sources(${PROJECT_NAME}_obj PRIVATE "SRC/Jaws.h" "SRC/Jaws.cpp" "Dep/fsapi.h" "Dep/fsapi.c")
  else()
    target_compile_definitions(${PROJECT_NAME}_obj PRIVATE SRAL_NO_JAWS)
  endif()
  if(SRAL_WITH_ZDSR)
    target_sources(${PROJECT_NAME}_obj PRIVATE "SRC/ZDSR.h" "SRC/ZDSR.cpp")
  else()
    target_compile_definitions(${PROJECT_NAME}_obj PRIVATE SRAL_NO_ZDSR)
  endif()
  if(SRAL_WITH_SAPI)
    target_sources(${PROJECT_NAME}_obj PRIVATE "SRC/SAPI.h" "SRC/SAPI.cpp" "Dep/blastspeak.h" "Dep/blastspeak.c" "Dep/wasapi.h" "Dep/wasapi.cpp")
  else()
    target_compile_definitions(${PROJECT_NAME}_obj PRIVATE SRAL_NO_SAPI)
  endif()
  if(SRAL_WITH_UIA)
    target_sources(${PROJECT_NAME}_obj PRIVATE "SRC/UIA.h" "SRC/UIA.cpp" "Dep/UIAProvider.h" "Dep/UIAProvider.cpp")
  else()
    target_compile_definitions(${PROJECT_NAME}_obj PRIVATE SRAL_NO_UIA)
  endif()

elseif(APPLE AND NOT SRAL_IS_EMSCRIPTEN_BUILD)
  enable_language(OBJC OBJCXX)
  target_compile_options(${PROJECT_NAME}_obj PRIVATE "$<COMPILE_LANGUAGE:OBJC,OBJCXX>:-fobjc-arc")
  if(SRAL_WITH_VOICEOVER)
    target_sources(${PROJECT_NAME}_obj PRIVATE "SRC/VoiceOver.h" "SRC/VoiceOver.mm")
  else()
    target_compile_definitions(${PROJECT_NAME}_obj PRIVATE SRAL_NO_VOICEOVER)
  endif()
  if(SRAL_WITH_AVSPEECH)
    target_sources(${PROJECT_NAME}_obj PRIVATE "SRC/AVSpeech.h" "SRC/AVSpeech.mm")
  else()
    target_compile_definitions(${PROJECT_NAME}_obj PRIVATE SRAL_NO_AVSPEECH)
  endif()
  if(SRAL_WITH_NSSPEECH)
    target_sources(${PROJECT_NAME}_obj PRIVATE "SRC/NSSpeech.h" "SRC/NSSpeech.mm")
  else()
    target_compile_definitions(${PROJECT_NAME}_obj PRIVATE SRAL_NO_NSSPEECH)
  endif()

elseif(ANDROID AND NOT SRAL_IS_EMSCRIPTEN_BUILD)
  target_sources(${PROJECT_NAME}_obj PRIVATE "Dep/AndroidContext.h" "Dep/AndroidContext.cpp")
  if(SRAL_WITH_ANDROID_TTS)
    target_sources(${PROJECT_NAME}_obj PRIVATE "SRC/AndroidTextToSpeech.h" "SRC/AndroidTextToSpeech.cpp")
  else()
    target_compile_definitions(${PROJECT_NAME}_obj PRIVATE SRAL_NO_ANDROID_TTS)
  endif()
  if(SRAL_WITH_ANDROID_ACCESSIBILITY)
    target_sources(${PROJECT_NAME}_obj PRIVATE "SRC/AndroidAccessibilityManager.h" "SRC/AndroidAccessibilityManager.cpp")
  else()
    target_compile_definitions(${PROJECT_NAME}_obj PRIVATE SRAL_NO_ANDROID_ACCESSIBILITY)
  endif()

elseif(SRAL_IS_EMSCRIPTEN_BUILD)
  message(STATUS "[SRAL] Compiling Object Target for WebAssembly Runtime context...")
  target_compile_options(${PROJECT_NAME}_obj PRIVATE "-pthread")

else() # Standard Linux
  target_sources(${PROJECT_NAME}_obj PRIVATE "Dep/utf-8.h" "Dep/utf-8.c" "SRC/ACAnnouncer.h" "SRC/ACAnnouncer.cpp")
  if(SRAL_WITH_SPEECH_DISPATCHER)
    target_sources(${PROJECT_NAME}_obj PRIVATE "SRC/SpeechDispatcher.h" "SRC/SpeechDispatcher.cpp")
  else()
    target_compile_definitions(${PROJECT_NAME}_obj PRIVATE SRAL_NO_SPEECH_DISPATCHER)
  endif()
  if(SRAL_WITH_ORCA)
    target_sources(${PROJECT_NAME}_obj PRIVATE "SRC/Orca.h" "SRC/Orca.cpp")
  else()
    target_compile_definitions(${PROJECT_NAME}_obj PRIVATE SRAL_NO_ORCA)
  endif()
  if(NOT SRAL_WITH_BRLAPI)
    target_compile_definitions(${PROJECT_NAME}_obj PRIVATE SRAL_NO_BRLAPI)
  endif()
endif()

set_property(TARGET ${PROJECT_NAME}_obj PROPERTY POSITION_INDEPENDENT_CODE ON)

# FINAL STATIC & SHARED LIBRARIES GENERATION & LINKING
if(BUILD_SHARED_LIBS)
  target_compile_definitions(${PROJECT_NAME}_obj PRIVATE WASAPI_EXPORTS)
  
  add_library(${PROJECT_NAME}_shared SHARED $<TARGET_OBJECTS:${PROJECT_NAME}_obj>)
  set_target_properties(${PROJECT_NAME}_shared PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
  
  target_include_directories(${PROJECT_NAME}_shared PUBLIC 
    $<BUILD_INTERFACE:${INCLUDES}>
    $<INSTALL_INTERFACE:include>
  )
else()
  target_compile_definitions(${PROJECT_NAME}_obj PRIVATE SRAL_STATIC)
endif()

add_library(${PROJECT_NAME}_static STATIC $<TARGET_OBJECTS:${PROJECT_NAME}_obj>)
if(WIN32)
  set_target_properties(${PROJECT_NAME}_static PROPERTIES OUTPUT_NAME "${PROJECT_NAME}_static")
else()
  set_target_properties(${PROJECT_NAME}_static PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
endif()

target_compile_definitions(${PROJECT_NAME}_static PUBLIC SRAL_STATIC)

target_include_directories(${PROJECT_NAME}_static PUBLIC 
  $<BUILD_INTERFACE:${INCLUDES}>
  $<INSTALL_INTERFACE:include>
)

set(SRAL_LINK_LIBS "")

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
  list(APPEND SRAL_LINK_LIBS "-pthread")
else() # Linux Desktops
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

# Bind platform libraries into compilation pipeline targets
if(BUILD_SHARED_LIBS)
  target_link_libraries(${PROJECT_NAME}_shared PRIVATE ${SRAL_LINK_LIBS})
  if(SRAL_IS_EMSCRIPTEN_BUILD)
    target_link_options(${PROJECT_NAME}_shared PRIVATE
      "-sWASM=1"
      "-sSIDE_MODULE=1"
      "-pthread"
    )
  endif()
endif()

target_link_libraries(${PROJECT_NAME}_static PUBLIC ${SRAL_LINK_LIBS})

# TEST EXECUTABLE COMPILATION BLOCK
if(BUILD_SRAL_TEST AND NOT SRAL_IS_EMSCRIPTEN_BUILD AND NOT ANDROID)
  add_executable(${PROJECT_NAME}_test "Examples/C/SRALExample.c")
  target_include_directories(${PROJECT_NAME}_test PRIVATE "${INCLUDES}")
  target_link_libraries(${PROJECT_NAME}_test PRIVATE ${PROJECT_NAME}_static)

  if(WIN32 AND SRAL_WITH_NVDA AND NOT SRAL_IS_EMSCRIPTEN_BUILD)
    add_executable(${PROJECT_NAME}_NVDAControlExConsole "Examples/C/NVDAControlExConsole.c")
    target_include_directories(${PROJECT_NAME}_NVDAControlExConsole PRIVATE "${INCLUDES}")
    target_link_libraries(${PROJECT_NAME}_NVDAControlExConsole PRIVATE ${PROJECT_NAME}_static)
  endif()

  if(APPLE AND NOT SRAL_IS_IOS_TARGET AND NOT SRAL_IS_EMSCRIPTEN_BUILD)
    add_executable(${PROJECT_NAME}_test_cocoa "Examples/ObjC/SRALCocoaExample.m")
    target_include_directories(${PROJECT_NAME}_test_cocoa PRIVATE "${INCLUDES}")
    target_compile_options(${PROJECT_NAME}_test_cocoa PRIVATE "$<COMPILE_LANGUAGE:OBJC>:-fobjc-arc")
    target_link_libraries(${PROJECT_NAME}_test_cocoa PRIVATE ${PROJECT_NAME}_static)
  endif()
endif()

# EXTERNAL LANGUAGE BINDINGS COMPILATION HOOKS
if(SRAL_BUILD_BINDINGS)
  if(SRAL_BINDING_CSHARP)
    find_program(DOTNET_EXE NAMES dotnet)
    if(DOTNET_EXE)
      add_custom_target(sral_csharp_binding ALL
        COMMAND ${DOTNET_EXE} build --configuration Release --restore -p:SKIP_MOBILE_WORKLOADS=true
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Bindings/C#"
        COMMENT "Restoring dependencies and compiling desktop-safe .NET 10 assemblies..."
      )
    else()
      message(WARNING "Skipping C# binding compilation: 'dotnet' command-line engine utility was not found.")
    endif()
  endif()

  if(SRAL_BINDING_RUST)
    find_program(CARGO_EXE NAMES cargo)
    if(CARGO_EXE)
      add_custom_target(sral_rust_binding ALL
        COMMAND ${CARGO_EXE} build --release
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Bindings/Rust"
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
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Bindings/Node.Js"
        COMMENT "Compiling Node.js native V8 C++ addon bridge binaries..."
      )
    else()
      message(WARNING "Skipping Node.js native addon compilation: 'npm' package utility was not found.")
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
  
  if(TARGET JNI::JNI)
    target_link_libraries(${PROJECT_NAME}_obj PRIVATE JNI::JNI)
  endif()
else()
  message(STATUS "[SRAL] WebAssembly target context active. Skipping all host Java/JNI environment lookups.")
endif()

target_sources(${PROJECT_NAME}_static PUBLIC
  FILE_SET HEADERS
  BASE_DIRS "${INCLUDES}"
  FILES "${INCLUDES}/SRAL.h" "${INCLUDES}/Sral.hpp"
)

if(BUILD_SHARED_LIBS)
  target_sources(${PROJECT_NAME}_shared PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "${INCLUDES}"
    FILES "${INCLUDES}/SRAL.h" "${INCLUDES}/Sral.hpp"
  )
endif()

# INSTALLATION & DEPLOYMENT STAGING TREE RULES
install(TARGETS ${PROJECT_NAME}_static EXPORT SRALTargets
  FILE_SET HEADERS DESTINATION "include"
  ARCHIVE DESTINATION "lib"
)

if(BUILD_SHARED_LIBS)
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
  install(FILES "Bindings/C#/Sral.cs" "Bindings/C#/Example.cs" "Bindings/C#/SRAL.csproj" DESTINATION "share/sral/bindings/csharp")
  install(FILES "Bindings/Dart/sral.dart" "Bindings/Dart/sral_types.dart" "Bindings/Dart/example.dart" DESTINATION "share/sral/bindings/dart")
  install(DIRECTORY "Bindings/go/" DESTINATION "share/sral/bindings/go")
  install(FILES "Bindings/Lua/sral.lua" "Bindings/Lua/example.lua" DESTINATION "share/sral/bindings/lua")
  install(FILES "Bindings/Python/sral.py" "Bindings/Python/Example.py" "Bindings/Python/pyproject.toml" DESTINATION "share/sral/bindings/python")
  install(DIRECTORY "Bindings/Rust/src/" DESTINATION "share/sral/bindings/rust/src")
  install(FILES "Bindings/Rust/Cargo.toml" DESTINATION "share/sral/bindings/rust")
  install(FILES "Bindings/WebAssembly/sral_wasm.cpp" "Bindings/WebAssembly/sral_wasm.js" DESTINATION "share/sral/bindings/wasm")
  install(FILES "Dep/AndroidTTSHelper.java" "Dep/AndroidAccessibilityManagerHelper.java" "Dep/proguard-rules.pro" DESTINATION "share/sral/android")
endif()
