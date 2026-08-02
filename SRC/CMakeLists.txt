cmake_path(GET CMAKE_CURRENT_SOURCE_DIR FILENAME CURRENT_DIR_NAME)
if(NOT CURRENT_DIR_NAME STREQUAL "SRC")
    message(FATAL_ERROR "[SRAL ERROR] SRC/CMakeLists.txt must be processed via add_subdirectory() from the workspace root.")
endif()

set(ROOT_SRC "${CMAKE_SOURCE_DIR}")
set(DEP_SOURCES "")

if(IS_WINDOWS)
    set(DEP_SOURCES
        "${ROOT_SRC}/Dep/nvda_control.c"
        "${ROOT_SRC}/Dep/fsapi.c"
        "${ROOT_SRC}/Dep/blastspeak.c"
    )
elseif(NOT IS_MACOS AND NOT IS_IOS AND NOT IS_ANDROID AND NOT IS_WASM)
    set(DEP_SOURCES "${ROOT_SRC}/Dep/utf-8.c")
endif()

set(CORE_SOURCES "Engine.cpp" "SRAL.cpp")
set(LOCAL_FEATURE_DEFINITIONS "")

if(NOT IS_WASM)
    if(SRAL_WITH_ACCESSKIT STREQUAL "enabled")
        list(APPEND CORE_SOURCES "ACAnnouncer.cpp")
    else()
        list(APPEND LOCAL_FEATURE_DEFINITIONS "SRAL_NO_ACCESSKIT")
    endif()
endif()

if(IS_WINDOWS)
    list(APPEND CORE_SOURCES "Encoding.cpp")

    if(SRAL_WITH_NVDA STREQUAL "enabled")
        list(APPEND CORE_SOURCES "NVDA.cpp")
    else()
        list(APPEND LOCAL_FEATURE_DEFINITIONS "SRAL_NO_NVDA")
    endif()

    if(SRAL_WITH_JAWS STREQUAL "enabled")
        list(APPEND CORE_SOURCES "Jaws.cpp")
    else()
        list(APPEND LOCAL_FEATURE_DEFINITIONS "SRAL_NO_JAWS")
    endif()

    if(SRAL_WITH_ZDSR STREQUAL "enabled")
        list(APPEND CORE_SOURCES "ZDSR.cpp")
    else()
        list(APPEND LOCAL_FEATURE_DEFINITIONS "SRAL_NO_ZDSR")
    endif()

    if(SRAL_WITH_SAPI STREQUAL "enabled")
        list(APPEND CORE_SOURCES "SAPI.cpp" "${ROOT_SRC}/Dep/wasapi.cpp")
    else()
        list(APPEND LOCAL_FEATURE_DEFINITIONS "SRAL_NO_SAPI")
    endif()

    if(SRAL_WITH_UIA STREQUAL "enabled")
        list(APPEND CORE_SOURCES "UIA.cpp" "${ROOT_SRC}/Dep/UIAProvider.cpp")
    else()
        list(APPEND LOCAL_FEATURE_DEFINITIONS "SRAL_NO_UIA")
    endif()

elseif(IS_MACOS OR IS_IOS)
    if(SRAL_WITH_VOICEOVER STREQUAL "enabled")
        list(APPEND CORE_SOURCES "VoiceOver.mm")
    else()
        list(APPEND LOCAL_FEATURE_DEFINITIONS "SRAL_NO_VOICEOVER")
    endif()

    if(SRAL_WITH_AVSPEECH STREQUAL "enabled")
        list(APPEND CORE_SOURCES "AVSpeech.mm")
    else()
        list(APPEND LOCAL_FEATURE_DEFINITIONS "SRAL_NO_AVSPEECH")
    endif()

    if(NOT IS_IOS AND SRAL_WITH_NSSPEECH STREQUAL "enabled")
        list(APPEND CORE_SOURCES "NSSpeech.mm")
    else()
        list(APPEND LOCAL_FEATURE_DEFINITIONS "SRAL_NO_NSSPEECH")
    endif()

elseif(IS_ANDROID)
    list(APPEND CORE_SOURCES "${ROOT_SRC}/Dep/AndroidContext.cpp")

    if(BUILD_ANDROID_HELPERS AND JAVAC_BIN AND PATH_jdk_install)
        cmake_path(SET JAR_BIN_PATH "${PATH_jdk_install}")
        cmake_path(APPEND JAR_BIN_PATH "bin" "jar")

        set(ANDROID_JAR_OUT "${CMAKE_CURRENT_BINARY_DIR}/SRALAndroidHelpers.jar")
        add_custom_command(
            OUTPUT "${ANDROID_JAR_OUT}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/java_classes"
            COMMAND "${JAVAC_BIN}" -d "${CMAKE_CURRENT_BINARY_DIR}/java_classes" "${ROOT_SRC}/Dep/AndroidAccessibilityManagerHelper.java" "${ROOT_SRC}/Dep/AndroidTTSHelper.java"
            COMMAND "${JAR_BIN_PATH}" cf "${ANDROID_JAR_OUT}" -C "${CMAKE_CURRENT_BINARY_DIR}/java_classes" .
            DEPENDS "${ROOT_SRC}/Dep/AndroidAccessibilityManagerHelper.java" "${ROOT_SRC}/Dep/AndroidTTSHelper.java"
            COMMENT "Assembling Android Java Helper Target Archives Safely via File-Tracking..."
            VERBATIM
        )
        add_custom_target(SRALAndroidHelpers ALL DEPENDS "${ANDROID_JAR_OUT}")
        install(FILES "${ANDROID_JAR_OUT}" DESTINATION "share/sral/android" COMPONENT PlatformAssets)
    endif()

    if(SRAL_WITH_ANDROID_TTS STREQUAL "enabled")
        list(APPEND CORE_SOURCES "AndroidTextToSpeech.cpp")
    else()
        list(APPEND LOCAL_FEATURE_DEFINITIONS "SRAL_NO_ANDROID_TTS")
    endif()

    if(SRAL_WITH_ANDROID_ACCESSIBILITY STREQUAL "enabled")
        list(APPEND CORE_SOURCES "AndroidAccessibilityManager.cpp")
    else()
        list(APPEND LOCAL_FEATURE_DEFINITIONS "SRAL_NO_ANDROID_ACCESSIBILITY")
    endif()

elseif(IS_WASM)
    if(SRAL_WITH_CHROMEVOX STREQUAL "enabled")
        list(APPEND CORE_SOURCES "ChromeVox.cpp")
    else()
        list(APPEND LOCAL_FEATURE_DEFINITIONS "SRAL_NO_CHROMEVOX")
    endif()

    list(APPEND LOCAL_FEATURE_DEFINITIONS
        "SRAL_NO_NVDA" "SRAL_NO_JAWS" "SRAL_NO_ZDSR" "SRAL_NO_SAPI" "SRAL_NO_UIA"
        "SRAL_NO_SPEECHD" "SRAL_NO_ORCA" "SRAL_NO_BRLAPI"
    )
else()
    list(APPEND CORE_SOURCES "Encoding.cpp")

    if(SRAL_WITH_SPEECH_DISPATCHER STREQUAL "enabled")
        list(APPEND CORE_SOURCES "SpeechDispatcher.cpp")
    else()
        list(APPEND LOCAL_FEATURE_DEFINITIONS "SRAL_NO_SPEECHD")
    endif()

    if(SRAL_WITH_ORCA STREQUAL "enabled")
        list(APPEND CORE_SOURCES "Orca.cpp")
    else()
        list(APPEND LOCAL_FEATURE_DEFINITIONS "SRAL_NO_ORCA")
    endif()
endif()

if(NOT IS_WASM)
    list(APPEND LOCAL_FEATURE_DEFINITIONS "SRAL_NO_CHROMEVOX")
endif()

set(SRAL_DEPS_TARGET "")
if(DEP_SOURCES)
    add_library(sral_deps STATIC)
    target_sources(sral_deps
        PRIVATE
        ${DEP_SOURCES}
    )

    set_target_properties(sral_deps PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        SKIP_LINTING ON
        SKIP_UNITY_BUILD_INCLUSION ON
    )
    target_include_directories(sral_deps PRIVATE "${ROOT_SRC}/Include" "${ROOT_SRC}/Dep")
    target_link_libraries(sral_deps PRIVATE sral_hardening_options)

    if(LOCAL_FEATURE_DEFINITIONS)
        target_compile_definitions(sral_deps PRIVATE "${LOCAL_FEATURE_DEFINITIONS}")
    endif()
    set(SRAL_DEPS_TARGET "sral_deps")
endif()

if(BUILD_SHARED_LIBS)
    add_library(SRAL SHARED)
    target_sources(SRAL PRIVATE ${CORE_SOURCES})

    add_library(SRAL_Static STATIC)
    target_sources(SRAL_Static PRIVATE ${CORE_SOURCES})
    set_target_properties(SRAL_Static PROPERTIES OUTPUT_NAME "SRAL_static")
    set(FINAL_TARGETS SRAL SRAL_Static)
else()
    add_library(SRAL STATIC)
    target_sources(SRAL PRIVATE ${CORE_SOURCES})

    set_target_properties(SRAL PROPERTIES OUTPUT_NAME "SRAL")
    set(FINAL_TARGETS SRAL)
endif()

set(LOCAL_SHARED_WARNINGS
    "-Wformat=2" "-Wformat-security" "-Wconversion" "-Wsign-conversion"
    "-Wshadow" "-Wundef" "-Wcast-qual" "-Wswitch-enum" "-Wnull-dereference"
)
set(LOCAL_CPP_WARNINGS "-Wctor-dtor-privacy" "-Woverloaded-virtual")

foreach(TARGET_OBJ IN LISTS FINAL_TARGETS)
    set_target_properties(${TARGET_OBJ} PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        COMPILE_WARNING_AS_ERROR ON
        C_VISIBILITY_PRESET hidden
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        OPTIMIZE_DEPENDENCIES ON
    )

    target_include_directories(${TARGET_OBJ}
        PUBLIC $<BUILD_INTERFACE:${ROOT_SRC}/Include> $<INSTALL_INTERFACE:include>
        PRIVATE "${ROOT_SRC}/Include" "${ROOT_SRC}/Dep"
    )

    target_link_libraries(${TARGET_OBJ} PRIVATE sral_hardening_options)

    if(SRAL_DEPS_TARGET)
        target_link_libraries(${TARGET_OBJ} PRIVATE ${SRAL_DEPS_TARGET})
    endif()

    if(LOCAL_FEATURE_DEFINITIONS)
        target_compile_definitions(${TARGET_OBJ} PRIVATE "${LOCAL_FEATURE_DEFINITIONS}")
    endif()

    if(BUILD_SHARED_LIBS AND TARGET_OBJ STREQUAL "SRAL")
        target_compile_definitions(${TARGET_OBJ} PRIVATE "SRAL_BUILDING_DLL" "WASAPI_EXPORTS")
    else()
        target_compile_definitions(${TARGET_OBJ} PRIVATE "SRAL_STATIC")
    endif()

    if(NOT IS_MSVC_LIKE)
        sral_add_safe_target_flags(${TARGET_OBJ}
            SCOPE PRIVATE
            C_FLAGS ${LOCAL_SHARED_WARNINGS}
            CXX_FLAGS ${LOCAL_SHARED_WARNINGS} ${LOCAL_CPP_WARNINGS}
        )

        set(DESIRED_LINK_ARGS "-Wl,-z,relro" "-Wl,-z,now" "-Wl,-z,noexecstack")
        if(NOT IS_MACOS AND NOT IS_IOS)
            list(APPEND DESIRED_LINK_ARGS "-Wl,-z,defs")
        else()
            list(APPEND DESIRED_LINK_ARGS "-Wl,-undefined,error")
        endif()

        sral_add_safe_target_flags(${TARGET_OBJ}
            SCOPE PRIVATE
            LINK_FLAGS ${DESIRED_LINK_ARGS}
        )
    endif()

    if(SRAL_PROJECT_LINK_FLAGS)
        target_link_options(${TARGET_OBJ} PRIVATE "${SRAL_PROJECT_LINK_FLAGS}")
    endif()
endforeach()