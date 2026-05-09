cmake_minimum_required (VERSION 3.25)
project ("SRAL" LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/Include")

# Target-specific MSVC warnings (TODO implemented)
if(MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

# MSVC Runtime and Hot Reload
if(NOT CMAKE_MSVC_RUNTIME_LIBRARY)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()
if (MSVC AND POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

# Options
option (BUILD_SRAL_TEST "Build SRAL examples/tests" ON)
option (SRAL_DISABLE_UIA "Disable UIA (UI Automation) support" OFF)
option (SRAL_DISABLE_NSSPEECH "Disable NSSpeech (macOS-only NSSpeechSynthesizer) support" OFF)

# Core Object Library
add_library(${PROJECT_NAME}_obj OBJECT)
target_sources(${PROJECT_NAME}_obj PRIVATE 
  "SRC/Encoding.h" "SRC/Encoding.cpp" 
  "SRC/SRAL.cpp" "SRC/Engine.h" "SRC/Engine.cpp")

target_sources(${PROJECT_NAME}_obj PUBLIC 
  FILE_SET HEADERS 
  BASE_DIRS "${INCLUDES}" 
  FILES "${INCLUDES}/SRAL.h")

# --- Platform Specific Source Selection & Dependency Checks ---

if(WIN32)
  # Standard Windows Engines
  target_sources(${PROJECT_NAME}_obj PRIVATE 
    "SRC/NVDA.h" "SRC/NVDA.cpp" "SRC/SAPI.h" "SRC/SAPI.cpp"
    "Dep/blastspeak.h" "Dep/blastspeak.c" "Dep/fsapi.h" "Dep/fsapi.c" 
    "Dep/nvda_control.h" "Dep/nvda_control.c"
    "SRC/Jaws.h" "SRC/Jaws.cpp" 
    "Dep/wasapi.h" "Dep/wasapi.cpp" "SRC/ZDSR.h" "SRC/ZDSR.cpp")

  # Optional UIA
  if(NOT SRAL_DISABLE_UIA)
    target_sources(${PROJECT_NAME}_obj PRIVATE "SRC/UIA.cpp" "Dep/UIAProvider.h" "Dep/UIAProvider.cpp")
    target_link_libraries(${PROJECT_NAME}_obj PRIVATE "uiautomationcore.lib")
  endif()

  # Check for AccessKit
  find_library(ACCESSKIT_LIB NAMES accesskit)
  if(ACCESSKIT_LIB)
    message(STATUS "AccessKit found: ${ACCESSKIT_LIB}")
    target_sources(${PROJECT_NAME}_obj PRIVATE "SRC/ACAnnouncer.h" "SRC/ACAnnouncer.cpp")
    target_link_libraries(${PROJECT_NAME}_obj PRIVATE ${ACCESSKIT_LIB})
  else()
    message(STATUS "AccessKit not found. ACAnnouncer will be disabled.")
    target_compile_definitions(${PROJECT_NAME}_obj PRIVATE SRAL_NO_ACCESSKIT)
  endif()

  target_link_libraries(${PROJECT_NAME}_obj PRIVATE "ole32.lib" "user32.lib")

elseif(APPLE)
  enable_language(OBJC OBJCXX)
  target_sources(${PROJECT_NAME}_obj PRIVATE "SRC/AVSpeech.h" "SRC/AVSpeech.mm" "SRC/VoiceOver.h" "SRC/VoiceOver.mm")
  
  if(NOT SRAL_DISABLE_NSSPEECH)
    target_sources(${PROJECT_NAME}_obj PRIVATE "SRC/NSSpeech.h" "SRC/NSSpeech.mm")
  endif()

  target_link_libraries(${PROJECT_NAME}_obj INTERFACE "-framework AppKit" "-framework Foundation" "-framework AVFoundation")

elseif(ANDROID)
  target_sources(${PROJECT_NAME}_obj PRIVATE 
    "SRC/AndroidTextToSpeech.h" "SRC/AndroidTextToSpeech.cpp"
    "SRC/AndroidAccessibilityManager.h" "SRC/AndroidAccessibilityManager.cpp"
    "Dep/AndroidContext.h" "Dep/AndroidContext.cpp")
  
  target_link_libraries(${PROJECT_NAME}_obj PRIVATE log)

else() # Linux
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SpeechD REQUIRED speech-dispatcher)
  pkg_check_modules(DBUS REQUIRED dbus-1)
  find_library(BRLAPI "brlapi" REQUIRED)

  target_sources(${PROJECT_NAME}_obj PRIVATE 
    "Dep/utf-8.h" "Dep/utf-8.c" 
    "SRC/SpeechDispatcher.h" "SRC/SpeechDispatcher.cpp"
    "SRC/Orca.h" "SRC/Orca.cpp")

  target_link_libraries(${PROJECT_NAME}_obj PRIVATE ${SpeechD_LIBRARIES} ${DBUS_LIBRARIES} ${BRLAPI})
  target_include_directories(${PROJECT_NAME}_obj PRIVATE ${SpeechD_INCLUDE_DIRS} ${DBUS_INCLUDE_DIRS})
endif()

set_property(TARGET ${PROJECT_NAME}_obj PROPERTY POSITION_INDEPENDENT_CODE ON)

# Feature-based Definitions
if(SRAL_DISABLE_UIA)
  target_compile_definitions(${PROJECT_NAME}_obj PRIVATE SRAL_NO_UIA)
endif()
if(SRAL_DISABLE_NSSPEECH)
  target_compile_definitions(${PROJECT_NAME}_obj PRIVATE SRAL_NO_NSSPEECH)
endif()

# --- Library Targets ---

# Static Library
add_library(${PROJECT_NAME}_static STATIC $<TARGET_OBJECTS:${PROJECT_NAME}_obj>)
target_include_directories(${PROJECT_NAME}_static PUBLIC 
    $<BUILD_INTERFACE:${INCLUDES}> 
    $<INSTALL_INTERFACE:include>)
# Inherit private links from object library
target_link_libraries(${PROJECT_NAME}_static PRIVATE $<TARGET_PROPERTY:${PROJECT_NAME}_obj,LINK_LIBRARIES>)

# Shared Library (Optional)
if(BUILD_SHARED_LIBS)
  add_library(${PROJECT_NAME} SHARED $<TARGET_OBJECTS:${PROJECT_NAME}_obj>)
  target_include_directories(${PROJECT_NAME} PUBLIC 
    $<BUILD_INTERFACE:${INCLUDES}> 
    $<INSTALL_INTERFACE:include>)
  target_link_libraries(${PROJECT_NAME} PRIVATE $<TARGET_PROPERTY:${PROJECT_NAME}_obj,LINK_LIBRARIES>)
endif()

# --- Examples / Tests ---

if (BUILD_SRAL_TEST)
  add_executable(${PROJECT_NAME}_test "Examples/C/SRALExample.c")
  target_link_libraries(${PROJECT_NAME}_test PRIVATE ${PROJECT_NAME}_static)

  if(APPLE)
     add_executable(${PROJECT_NAME}_test_cocoa "Examples/ObjC/SRALCocoaExample.m")
     target_link_libraries(${PROJECT_NAME}_test_cocoa PRIVATE ${PROJECT_NAME}_static)
  endif()
endif()

# --- Installation ---

install(TARGETS ${PROJECT_NAME}_obj FILE_SET HEADERS DESTINATION "include")
install(TARGETS ${PROJECT_NAME}_static DESTINATION "lib")
if(BUILD_SHARED_LIBS)
  install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION "bin" LIBRARY DESTINATION "lib" ARCHIVE DESTINATION "lib")
endif()
