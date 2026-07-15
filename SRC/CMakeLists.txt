if(NOT CMAKE_CURRENT_SOURCE_DIR MATCHES "SRC$")
    message(FATAL_ERROR "[SRAL SECURITY] SRC/CMakeLists.txt must be processed via add_subdirectory() from the workspace root.")
endif()

set(DEP_SOURCES "")

if(IS_WINDOWS)
    list(APPEND DEP_SOURCES
        "${CMAKE_SOURCE_DIR}/Dep/nvda_control.c"
        "${CMAKE_SOURCE_DIR}/Dep/fsapi.c"
        "${CMAKE_SOURCE_DIR}/Dep/blastspeak.c"
    )
elseif(NOT IS_APPLE AND NOT IS_ANDROID AND NOT IS_EMSCRIPTEN)
    list(APPEND DEP_SOURCES "${CMAKE_SOURCE_DIR}/Dep/utf-8.c")
endif()

set(SRAL_DEPS_TARGET "")
if(DEP_SOURCES)
    add_library(sral_deps STATIC ${DEP_SOURCES})
    target_include_directories(sral_deps PRIVATE "${CMAKE_SOURCE_DIR}/Include")
    set_target_properties(sral_deps PROPERTIES POSITION_INDEPENDENT_CODE ON)
    set(SRAL_DEPS_TARGET sral_deps)
endif()
set(SHARED_WARNINGS "")
set(CPP_EXCLUSIVE_WARNINGS "")

if(NOT COMPILER_IS_MSVC_LIKE)
    list(APPEND SHARED_WARNINGS
        -Wformat=2
        -Wformat-security
        -Werror=format-security
        -Wconversion
        -Wsign-conversion
        -Wshadow
        -Wundef
        -Wcast-qual
        -Wswitch-enum
        -Wnull-dereference
    )
    list(APPEND CPP_EXCLUSIVE_WARNINGS
        -Wold-style-cast
        -Wnon-virtual-dtor
        -Wctor-dtor-privacy
        -Woverloaded-virtual
    )
endif()

set(CORE_SOURCES Engine.cpp SRAL.cpp)

if(NOT IS_EMSCRIPTEN) 
    if(SRAL_WITH_ACCESSKIT)
        list(APPEND CORE_SOURCES ACAnnouncer.cpp)
    else()
        add_compile_definitions(SRAL_NO_ACCESSKIT)
    endif()
endif()
if(IS_WINDOWS)
    list(APPEND CORE_SOURCES Encoding.cpp)
    if(SRAL_WITH_NVDA)
        list(APPEND CORE_SOURCES NVDA.cpp)
    else()
        add_compile_definitions(SRAL_NO_NVDA)
    endif()
    if(SRAL_WITH_JAWS)
        list(APPEND CORE_SOURCES Jaws.cpp)
    else()
        add_compile_definitions(SRAL_NO_JAWS)
    endif()
    if(SRAL_WITH_ZDSR)
        list(APPEND CORE_SOURCES ZDSR.cpp)
    else()
        add_compile_definitions(SRAL_NO_ZDSR)
    endif()
    if(SRAL_WITH_SAPI)
        list(APPEND CORE_SOURCES SAPI.cpp "${CMAKE_SOURCE_DIR}/Dep/wasapi.cpp")
    else()
        add_compile_definitions(SRAL_NO_SAPI)
    endif()
    if(SRAL_WITH_UIA)
        list(APPEND CORE_SOURCES UIA.cpp "${CMAKE_SOURCE_DIR}/Dep/UIAProvider.cpp")
    else()
        add_compile_definitions(SRAL_NO_UIA)
    endif()

elseif(IS_APPLE)
    if(SRAL_WITH_VOICEOVER)
        list(APPEND CORE_SOURCES VoiceOver.mm)
    else()
        add_compile_definitions(SRAL_NO_VOICEOVER)
    endif()
    if(SRAL_WITH_AVSPEECH)
        list(APPEND CORE_SOURCES AVSpeech.mm)
    else()
        add_compile_definitions(SRAL_NO_AVSPEECH)
    endif()
    if(NOT IS_IOS AND SRAL_WITH_NSSPEECH)
        list(APPEND CORE_SOURCES NSSpeech.mm)
    else()
        add_compile_definitions(SRAL_NO_NSSPEECH)
    endif()

elseif(IS_ANDROID)
    list(APPEND CORE_SOURCES "${CMAKE_SOURCE_DIR}/Dep/AndroidContext.cpp")
    if(SRAL_WITH_ANDROID_TTS)
        list(APPEND CORE_SOURCES AndroidTextToSpeech.cpp)
    else()
        add_compile_definitions(SRAL_NO_ANDROID_TTS)
    endif()
    if(SRAL_WITH_ANDROID_ACCESSIBILITY)
        list(APPEND CORE_SOURCES AndroidAccessibilityManager.cpp)
    else()
        add_compile_definitions(SRAL_NO_ANDROID_ACCESSIBILITY)
    endif()

elseif(IS_EMSCRIPTEN)
    if(SRAL_WITH_CHROMEVOX)
        list(APPEND CORE_SOURCES ChromeVox.cpp)
    else()
        add_compile_definitions(SRAL_NO_CHROMEVOX)
    endif()
    add_compile_definitions(
        SRAL_NO_NVDA SRAL_NO_JAWS SRAL_NO_ZDSR SRAL_NO_SAPI SRAL_NO_UIA 
        SRAL_NO_SPEECH_DISPATCHER SRAL_NO_ORCA SRAL_NO_BRLAPI
    )
else()
    list(APPEND CORE_SOURCES Encoding.cpp)
    if(SRAL_WITH_SPEECH_DISPATCHER)
        list(APPEND CORE_SOURCES SpeechDispatcher.cpp)
    else()
        add_compile_definitions(SRAL_NO_SPEECH_DISPATCHER)
    endif()
    if(SRAL_WITH_ORCA)
        list(APPEND CORE_SOURCES Orca.cpp)
    else()
        add_compile_definitions(SRAL_NO_ORCA)
    endif()
endif()

if(NOT IS_EMSCRIPTEN)
    add_compile_definitions(SRAL_NO_CHROMEVOX)
endif()

if(BUILD_SHARED_LIBS)
    add_library(SRAL SHARED ${CORE_SOURCES})
    target_compile_definitions(SRAL PRIVATE SRAL_BUILDING_DLL WASAPI_EXPORTS)
else()
    add_library(SRAL STATIC ${CORE_SOURCES})
    target_compile_definitions(SRAL PUBLIC SRAL_STATIC)
endif()

target_include_directories(SRAL 
    PUBLIC "${CMAKE_SOURCE_DIR}/Include" 
    PRIVATE "${CMAKE_SOURCE_DIR}/Dep" ${SRAL_SYS_INCLUDE_DIRS}
)
target_compile_options(SRAL PRIVATE ${SHARED_WARNINGS} $<$<COMPILE_LANGUAGE:CXX>:${CPP_EXCLUSIVE_WARNINGS}>)

if(SRAL_DEPS_TARGET)
    target_link_libraries(SRAL PRIVATE ${SRAL_DEPS_TARGET})
endif()

target_link_libraries(SRAL PUBLIC ${SRAL_DEPENDENCIES})
if(NOT COMPILER_IS_MSVC_LIKE)
    set(DESIRED_LINK_ARGS "-Wl,-z,relro" "-Wl,-z,now" "-Wl,-z,noexecstack")
    if(NOT IS_APPLE)
        list(APPEND DESIRED_LINK_ARGS "-Wl,-z,defs")
    else()
        list(APPEND DESIRED_LINK_ARGS "-Wl,-undefined,error")
    endif()
    
    foreach(FLAG ${DESIRED_LINK_ARGS})
        string(MAKE_C_IDENTIFIER "LINK_FLAG_${FLAG}" FLAG_VAR)
        include(CheckCXXCompilerFlag)
        check_cxx_compiler_flag("${FLAG}" ${FLAG_VAR})
        if(${FLAG_VAR})
            target_link_options(SRAL PRIVATE "${FLAG}")
        endif()
    endforeach()
else()
    target_link_options(SRAL PRIVATE /NXCOMPAT /DYNAMICBASE /HIGHENTROPYVA)
    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
        target_link_options(SRAL PRIVATE /CETCOMPAT)
    endif()
endif()

set_target_properties(SRAL PROPERTIES
    COMPILE_WARNING_AS_ERROR ON
    POSITION_INDEPENDENT_CODE ON
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

install(TARGETS SRAL
    EXPORT SRALTargets
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)