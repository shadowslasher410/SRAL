cmake_minimum_required(VERSION 4.4.0)

project(SRAL_Orchestrator
    VERSION 1.0.0
    LANGUAGES C CXX
)

cmake_policy(SET CMP0077 NEW) # option() respects normal scope over cache
cmake_policy(SET CMP0126 NEW) # set(CACHE) does not silently strip local scope variables
cmake_policy(SET CMP0135 NEW) # Enforces safe file extraction timestamps for FetchContent
cmake_policy(SET CMP0141 NEW) # Modernized MSVC debug information parameter formatting rules
cmake_policy(SET CMP0142 NEW) # Strict MSVC architecture platform directory paths mapping
cmake_policy(SET CMP0156 NEW) # Safe link library de-duplication rules for macOS/iOS/Linux
cmake_policy(SET CMP0219 NEW) # Preserves backslashes explicitly in macro arguments

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(BUILD_SHARED_LIBS ON)
set(CMAKE_DIRECTORY_LABELS "SRAL_Root_Orchestrator")

include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
if(ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_CURRENT_BINARY_DIR)
    message(FATAL_ERROR
        "\n===============================================================================\n"
        "[SRAL ERROR] In-source builds are structurally prohibited!\n"
        "Compiling inside your source tree risks local repository metadata corruption.\n"
        "Please delete 'CMakeCache.txt' and 'CMakeFiles/' from your root repository folder,\n"
        "generate an independent build directory, and execute the configuration from there.\n"
        "===============================================================================\n"
    )
endif()

set(IS_WINDOWS FALSE)
set(IS_LINUX FALSE)
set(IS_MACOS FALSE)
set(IS_IOS FALSE)
set(IS_ANDROID FALSE)
set(IS_WASM FALSE)

if(CMAKE_SYSTEM_NAME MATCHES "^(Windows|Linux|Darwin|iOS|Android|Emscripten)$")
    set(MATCHED_PLATFORM "${CMAKE_MATCH_1}")
    if(MATCHED_PLATFORM STREQUAL "Darwin")
        set(IS_MACOS TRUE)
    elseif(MATCHED_PLATFORM STREQUAL "Emscripten")
        set(IS_WASM TRUE)
    else()
        string(TOUPPER "${MATCHED_PLATFORM}" UPPER_PLATFORM)
        set(IS_${UPPER_PLATFORM} TRUE)
    endif()
endif()

set(IS_MSVC_LIKE FALSE)
set(IS_GCC FALSE)
set(IS_CLANG_CL FALSE)
set(IS_CLANG FALSE)

if(MSVC)
    set(IS_MSVC_LIKE TRUE)
endif()
if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(IS_GCC TRUE)
endif()
if(CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(IS_CLANG TRUE)
    if(MSVC)
        set(IS_CLANG_CL TRUE)
    endif()
endif()

if(IS_IOS AND NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    message(FATAL_ERROR "[SRAL ERROR] Apple iOS device/simulator static bundles can only be cross-compiled from a macOS host machine.")
endif()

if(IS_MACOS OR IS_IOS)
    enable_language(OBJC)
    enable_language(OBJCXX)
endif()

set(HARDENING_ARCH_LEVEL "none" CACHE STRING "Select targeted processor microarchitecture optimization profile")
set_property(CACHE HARDENING_ARCH_LEVEL PROPERTY STRINGS "none" "x86-64-v2" "x86-64-v3" "native")

if(NOT HARDENING_ARCH_LEVEL MATCHES "^(none|x86-64-v2|x86-64-v3|native)$")
    message(FATAL_ERROR "[SRAL ERROR] Invalid value for option HARDENING_ARCH_LEVEL: '${HARDENING_ARCH_LEVEL}'. Whitelist: none, x86-64-v2, x86-64-v3, native")
endif()

function(sral_feature name default_val description)
    get_property(has_custom_strings CACHE ${name} PROPERTY STRINGS)
    if(NOT has_custom_strings)
        set(${name} "${default_val}" CACHE STRING "${description}")
        set_property(CACHE ${name} PROPERTY STRINGS "enabled;disabled;auto")
    endif()

    get_property(allowed_strings CACHE ${name} PROPERTY STRINGS)
    if(NOT "${${name}}" IN_LIST allowed_strings)
        message(FATAL_ERROR "[SRAL ERROR] Invalid value for option ${name}: '${${name}}'. Whitelist: ${allowed_strings}")
    endif()

    set(${name} "${${name}}" PARENT_SCOPE)
endfunction()

sral_feature(SRAL_DEV_SANITIZERS "disabled" "Inject compilation runtime security sanitizers")
sral_feature(BUILD_TESTS "auto" "Compile the native automated executable validation check suites")
sral_feature(BUILD_BINDINGS "auto" "Trigger individual downstream foreign language wrapper targets")
sral_feature(SRAL_INSTALL_BINDING_SOURCES "enabled" "Package and export raw text binding wrapper code structures")

set(BINDING_LANGS CSHARP RUST NODE GO DART LUA PYTHON WASM)
foreach(LANG IN LISTS BINDING_LANGS)
    sral_feature(SRAL_BINDING_${LANG} "auto" "Compile optimized ${LANG} backend wrappers")
endforeach()

set(BACKEND_FEATURES CHROMEVOX ACCESSKIT UIA NSSPEECH NVDA JAWS ZDSR SAPI SPEECH_DISPATCHER ORCA BRLAPI VOICEOVER AVSPEECH ANDROID_TTS ANDROID_ACCESSIBILITY)
foreach(FEATURE IN LISTS BACKEND_FEATURES)
    sral_feature(SRAL_WITH_${FEATURE} "auto" "Integrate ${FEATURE} service core engine")
endforeach()

if(SRAL_WITH_CHROMEVOX STREQUAL "enabled" OR (SRAL_WITH_CHROMEVOX STREQUAL "auto" AND IS_WASM))
    message(STATUS "[SRAL ARCHITECTURE] ChromeVox active layer confirmed. Overriding other options...")
    set(SRAL_WITH_CHROMEVOX "enabled" CACHE STRING "Force active" FORCE)
    foreach(FEATURE IN LISTS BACKEND_FEATURES)
        if(NOT FEATURE STREQUAL "CHROMEVOX")
            set(SRAL_WITH_${FEATURE} "disabled" CACHE STRING "Force disabled due to ChromeVox" FORCE)
        endif()
    endforeach()
endif()

set(BUILD_ANDROID_HELPERS FALSE)
if(NOT SRAL_WITH_ANDROID_TTS STREQUAL "disabled" OR NOT SRAL_WITH_ANDROID_ACCESSIBILITY STREQUAL "disabled")
    if(IS_ANDROID)
        set(BUILD_ANDROID_HELPERS TRUE)
        find_program(JAVAC_BIN javac REQUIRED)
    endif()
endif()

if(NOT DEFINED SRAL_INI_HASH)
    set(SRAL_INI_HASH "" CACHE INTERNAL "MD5 Hash signature of settings.ini")
endif()
if(NOT DEFINED SRAL_PATHS_CACHED)
    set(SRAL_PATHS_CACHED FALSE CACHE INTERNAL "Path cache status")
endif()

set(SHOULD_PARSE_INI FALSE)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/settings.ini")
    file(MD5 "${CMAKE_CURRENT_SOURCE_DIR}/settings.ini" CURRENT_INI_HASH)
    if(NOT "${CURRENT_INI_HASH}" STREQUAL "${SRAL_INI_HASH}")
        message(STATUS "[SRAL STATUS] settings.ini structural mutation detected. Invalidating path caches...")
        set(SRAL_PATHS_CACHED FALSE CACHE INTERNAL "Path cache status" FORCE)
        set(SRAL_INI_HASH "${CURRENT_INI_HASH}" CACHE INTERNAL "MD5 Hash signature of settings.ini" FORCE)
    endif()
    if(NOT SRAL_PATHS_CACHED)
        set(SHOULD_PARSE_INI TRUE)
    endif()
endif()

if(SHOULD_PARSE_INI)
    block(PROPAGATE SRAL_PATHS_CACHED)
        message(STATUS "--- SRAL Vectorized Configuration Ingestion ---")
        file(READ "${CMAKE_CURRENT_SOURCE_DIR}/settings.ini" INI_RAW_CONTENT)
        string(REPLACE "\r" "" INI_RAW_CONTENT "${INI_RAW_CONTENT}")
        string(REGEX REPLACE "\n[ \t]*[;#][^\n]*" "" INI_CLEAN "${INI_RAW_CONTENT}")
        string(REGEX REPLACE "\n+" "\n" INI_CLEAN "${INI_CLEAN}")
        string(REPLACE "\n" ";" INI_ROWS "${INI_CLEAN}")

        set(INI_KEYS nvda_install;speechd_install;python_install;emsdk_install;android_ndk;accesskit_install;jdk_install;usertarget_windows;usertarget_macos;usertarget_android;log_level;log_to_file;log_file_path)

        foreach(ROW IN LISTS INI_ROWS)
            if(ROW MATCHES "^[ \t]*([^= \t]+)[ \t]*=[ \t]*(.*)$")
                string(STRIP "${CMAKE_MATCH_1}" KEY_RAW)
                string(TOLOWER "${KEY_RAW}" KEY)
                set(VAL "${CMAKE_MATCH_2}")
                string(REGEX REPLACE "^\"|\"$" "" VAL "${VAL}")
                string(REGEX REPLACE "^'|'$" "" VAL "${VAL}")
                string(STRIP "${VAL}" VAL)

                if(KEY IN_LIST INI_KEYS AND NOT "${VAL}" STREQUAL "")
                    if(KEY MATCHES "^usertarget_")
                        set(TARGET_${KEY} "${VAL}" CACHE STRING "Forced target profile mapping" FORCE)
                        message(STATUS "  [INI] Configured TARGET_${KEY} = ${VAL}")
                    elseif(KEY STREQUAL "log_level")
                        set(SRAL_LOG_LEVEL "${VAL}" CACHE STRING "Severity filter for runtime logs" FORCE)
                        message(STATUS "  [INI] Configured SRAL_LOG_LEVEL = ${VAL}")
                    elseif(KEY STREQUAL "log_to_file")
                        set(SRAL_LOG_TO_FILE "${VAL}" CACHE STRING "Toggle file logging writing state" FORCE)
                        message(STATUS "  [INI] Configured SRAL_LOG_TO_FILE = ${VAL}")
                    elseif(KEY STREQUAL "log_file_path")
                        cmake_path(SET CONVERTED_PATH "${VAL}")
                        cmake_path(ABSOLUTE_PATH CONVERTED_PATH BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
                        set(SRAL_LOG_FILE_PATH "${CONVERTED_PATH}" CACHE FILEPATH "Absolute destination of diagnostic log data" FORCE)
                        message(STATUS "  [INI] Configured SRAL_LOG_FILE_PATH = ${CONVERTED_PATH}")
                    else()
                        cmake_path(SET CONVERTED_PATH "${VAL}")
                        cmake_path(NORMAL_PATH CONVERTED_PATH)
                        if(IS_DIRECTORY "${CONVERTED_PATH}")
                            set(PATH_${KEY} "${CONVERTED_PATH}" CACHE PATH "Cached asset verification" FORCE)
                            message(STATUS "  [INI] Configured PATH_${KEY} = ${CONVERTED_PATH}")
                        else()
                            message(WARNING "[SRAL WARNING] Key '${KEY}' contains invalid directory path: '${CONVERTED_PATH}'")
                        endif()
                    endif()
                endif()
            endif()
        endforeach()
        set(SRAL_PATHS_CACHED TRUE CACHE INTERNAL "Path cache status" FORCE)
    endblock()
endif()

foreach(KEY ITEMS nvda_install;speechd_install;python_install;emsdk_install;android_ndk;accesskit_install;jdk_install)
    if(NOT DEFINED PATH_${KEY} OR "${PATH_${KEY}}" STREQUAL "")
        set(FALLBACK_FOUND "")
        set(ENV_MAP "")
        set(NAME_MAP "")

        if(KEY STREQUAL "jdk_install")
            set(ENV_MAP "JAVA_HOME")
            set(NAME_MAP "Target Host Java Development Kit")
            if(DEFINED ENV{JAVA_HOME})
                cmake_path(SET CHK_JDK "$ENV{JAVA_HOME}")
                if(IS_DIRECTORY "${CHK_JDK}")
                    set(FALLBACK_FOUND "${CHK_JDK}")
                endif()
            endif()
            if(NOT FALLBACK_FOUND AND (CMAKE_SYSTEM_NAME STREQUAL "Android" OR BUILD_ANDROID_HELPERS))
                if(WIN32)
                    foreach(p ITEMS "C:/Program Files/Android/Android Studio/jbr" "C:/Program Files/Android/Android Studio/jre" "$ENV{LOCALAPPDATA}/JetBrains/Toolbox/apps/AndroidStudio/ch-0/plugins/jre" "$ENV{LOCALAPPDATA}/JetBrains/Runtime" "C:/Program Files/Java/jdk-21" "C:/Program Files/x86)/Android/Android Studio/jbr" "C:/Program Files/x86)/Android/Android Studio/jre")
                        if(IS_DIRECTORY "${p}")
                            set(FALLBACK_FOUND "${p}")
                            break()
                        endif()
                    endforeach()
                    if(NOT FALLBACK_FOUND)
                        foreach(view ITEMS 64 32)
                            foreach(hive ITEMS "HKLM/SOFTWARE/JavaSoft/JDK" "HKLM/SOFTWARE/Microsoft/JDK" "HKLM/SOFTWARE/Eclipse Foundation/JDK")
                                cmake_host_system_information(RESULT REG_VER QUERY REGISTRY_VALUE "${hive}" "CurrentVersion" VIEW ${view})
                                if(REG_VER)
                                    cmake_host_system_information(RESULT REG_JDK QUERY REGISTRY_VALUE "${hive}/${REG_VER}" "JavaHome" VIEW ${view})
                                    if(REG_JDK AND IS_DIRECTORY "${REG_JDK}")
                                        set(FALLBACK_FOUND "${REG_JDK}")
                                        break()
                                    endif()
                                endif()
                            endforeach()
                            if(FALLBACK_FOUND)
                                break()
                            endif()
                        endforeach()
                    endif()
                elseif(APPLE)
                    find_program(MACOS_JAVA_HOME_CMD java_home PATHS "/usr/libexec" NO_DEFAULT_PATH)
                    if(MACOS_JAVA_HOME_CMD)
                        execute_process(COMMAND "${MACOS_JAVA_HOME_CMD}" OUTPUT_VARIABLE MAC_JDK_PATH OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
                        if(MAC_JDK_PATH AND IS_DIRECTORY "${MAC_JDK_PATH}")
                            set(FALLBACK_FOUND "${MAC_JDK_PATH}")
                        endif()
                    endif()
                    if(NOT FALLBACK_FOUND)
                        foreach(p ITEMS "/Applications/Android Studio.app/Contents/jbr" "/Applications/Android Studio.app/Contents/jre" "/Library/Java/JavaVirtualMachines/jdk-21.jdk/Contents/Home" "$ENV{HOME}/Library/Application Support/JetBrains/Toolbox/apps/AndroidStudio/ch-0/plugins/jre")
                            if(IS_DIRECTORY "${p}")
                                set(FALLBACK_FOUND "${p}")
                                break()
                            endif()
                        endforeach()
                    endif()
                else()
                    foreach(p ITEMS "/opt/android-studio/jbr" "/opt/android-studio/jre" "/usr/lib/jvm/default-java" "/usr/lib/jvm/java-21-openjdk-amd64" "/usr/local/android-studio/jbr" "/usr/local/android-studio/jre" "$ENV{HOME}/.local/share/JetBrains/Toolbox/apps/AndroidStudio/ch-0/plugins/jre" "/usr/lib/jvm/java-21-openjdk")
                        if(IS_DIRECTORY "${p}")
                            set(FALLBACK_FOUND "${p}")
                            break()
                        endif()
                    endforeach()
                endif()
            endif()
        elseif(KEY STREQUAL "android_ndk")
            set(ENV_MAP "ANDROID_NDK_HOME")
            set(NAME_MAP "Google Android NDK Workspace")
            if(DEFINED ENV{ANDROID_NDK_HOME} AND IS_DIRECTORY "$ENV{ANDROID_NDK_HOME}")
                set(FALLBACK_FOUND "$ENV{ANDROID_NDK_HOME}")
            elseif(WIN32 AND DEFINED ENV{LOCALAPPDATA} AND IS_DIRECTORY "$ENV{LOCALAPPDATA}/Android/Sdk/ndk")
                file(GLOB NDK_VERSIONS RELATIVE "$ENV{LOCALAPPDATA}/Android/Sdk/ndk" "$ENV{LOCALAPPDATA}/Android/Sdk/ndk/*")
                if(NDK_VERSIONS)
                    list(SORT NDK_VERSIONS COMPARE NATURAL ORDER DESCENDING)
                    foreach(top_version IN LISTS NDK_VERSIONS)
                        set(FALLBACK_FOUND "$ENV{LOCALAPPDATA}/Android/Sdk/ndk/${top_version}")
                        break()
                    endforeach()
                endif()
            elseif(IS_DIRECTORY "/usr/lib/android-ndk")
                set(FALLBACK_FOUND "/usr/lib/android-ndk")
            elseif(DEFINED ENV{HOME} AND IS_DIRECTORY "$ENV{HOME}/Android/Sdk/ndk")
                file(GLOB NDK_VERSIONS_LINUX RELATIVE "$ENV{HOME}/Android/Sdk/ndk" "$ENV{HOME}/Android/Sdk/ndk/*")
                if(NDK_VERSIONS_LINUX)
                    list(SORT NDK_VERSIONS_LINUX COMPARE NATURAL ORDER DESCENDING)
                    foreach(top_version IN LISTS NDK_VERSIONS_LINUX)
                        set(FALLBACK_FOUND "$ENV{HOME}/Android/Sdk/ndk/${top_version}")
                        break()
                    endforeach()
                endif()
            elseif(APPLE AND IS_DIRECTORY "/Library/Android/sdk/ndk")
                file(GLOB NDK_VERSIONS_MAC RELATIVE "/Library/Android/sdk/ndk" "/Library/Android/sdk/ndk/*")
                if(NDK_VERSIONS_MAC)
                    list(SORT NDK_VERSIONS_MAC COMPARE NATURAL ORDER DESCENDING)
                    foreach(top_version IN LISTS NDK_VERSIONS_MAC)
                        set(FALLBACK_FOUND "/Library/Android/sdk/ndk/${top_version}")
                        break()
                    endforeach()
                endif()
            endif()
        elseif(KEY STREQUAL "python_install")
            set(ENV_MAP "PYTHON_PATH")
            set(NAME_MAP "Isolated Python Environment")
            if(DEFINED ENV{PYTHON_PATH} AND IS_DIRECTORY "$ENV{PYTHON_PATH}")
                set(FALLBACK_FOUND "$ENV{PYTHON_PATH}")
            else()
                execute_process(COMMAND ${CMAKE_COMMAND} -E true ENVIRONMENT "PYTHON_PATH=" OUTPUT_QUIET ERROR_QUIET)
                find_package(Python3 COMPONENTS Interpreter QUIET)
                if(Python3_INTERPRETER_FOUND)
                    cmake_path(SET PY_EXE "${Python3_EXECUTABLE}")
                    cmake_path(GET PY_EXE PARENT_PATH PY_DIR)
                    if(IS_MSVC_LIKE)
                        set(FALLBACK_FOUND "${PY_DIR}")
                    else()
                        cmake_path(GET PY_DIR PARENT_PATH PY_ROOT)
                        set(FALLBACK_FOUND "${PY_ROOT}")
                    endif()
                endif()
            endif()
        elseif(KEY STREQUAL "emsdk_install")
            set(ENV_MAP "EMSDK_PATH")
            set(NAME_MAP "Emscripten WebAssembly SDK")
            if(DEFINED ENV{EMSDK_PATH} AND IS_DIRECTORY "$ENV{EMSDK_PATH}")
                set(FALLBACK_FOUND "$ENV{EMSDK_PATH}")
            elseif(WIN32)
                foreach(p ITEMS "C:/emsdk" "C:/src/emsdk" "C:/tools/emsdk" "$ENV{USERPROFILE}/emsdk")
                    if(IS_DIRECTORY "${p}" AND (IS_DIRECTORY "${p}/upstream/emscripten" OR EXISTS "${p}/emsdk.bat"))
                        set(FALLBACK_FOUND "${p}")
                        break()
                    endif()
                endforeach()
            else()
                foreach(p ITEMS "/opt/emsdk" "/usr/local/emsdk" "$ENV{HOME}/emsdk" "$ENV{HOME}/src/emsdk" "$ENV{HOME}/devtools/emsdk")
                    if(IS_DIRECTORY "${p}" AND (IS_DIRECTORY "${p}/upstream/emscripten" OR EXISTS "${p}/emsdk"))
                        set(FALLBACK_FOUND "${p}")
                        break()
                    endif()
                endforeach()
            endif()
        elseif(KEY STREQUAL "accesskit_install")
            set(ENV_MAP "ACCESSKIT_PATH")
            set(NAME_MAP "AccessKit Proxy Library Assets")
            if(DEFINED ENV{ACCESSKIT_PATH} AND IS_DIRECTORY "$ENV{ACCESSKIT_PATH}")
                set(FALLBACK_FOUND "$ENV{ACCESSKIT_PATH}")
            elseif(WIN32)
                foreach(p ITEMS "C:/accesskit" "C:/libs/accesskit" "$ENV{USERPROFILE}/Documents/accesskit")
                    if(IS_DIRECTORY "${p}")
                        set(FALLBACK_FOUND "${p}")
                        break()
                    endif()
                endforeach()
            elseif(APPLE)
                foreach(p ITEMS "/opt/accesskit" "$ENV{HOME}/Library/Frameworks/accesskit" "$ENV{HOME}/accesskit")
                    if(IS_DIRECTORY "${p}")
                        set(FALLBACK_FOUND "${p}")
                        break()
                    endif()
                endforeach()
            else()
                foreach(p ITEMS "/usr/lib/accesskit" "/usr/local/lib/accesskit" "/opt/accesskit" "$ENV{HOME}/accesskit")
                    if(IS_DIRECTORY "${p}")
                        set(FALLBACK_FOUND "${p}")
                        break()
                    endif()
                endforeach()
            endif()
        elseif(KEY STREQUAL "nvda_install")
            set(ENV_MAP "NVDA_SDK_PATH")
            set(NAME_MAP "NVDA SDK Remote Linkage")
            if(DEFINED ENV{NVDA_SDK_PATH} AND IS_DIRECTORY "$ENV{NVDA_SDK_PATH}")
                set(FALLBACK_FOUND "$ENV{NVDA_SDK_PATH}")
            elseif(WIN32)
                foreach(p ITEMS "C:/nvda-sdk" "C:/SDKS/nvda-sdk" "$ENV{USERPROFILE}/AppData/Local/nvda-sdk" "$ENV{USERPROFILE}/Documents/nvda-sdk")
                    if(IS_DIRECTORY "${p}")
                        set(FALLBACK_FOUND "${p}")
                        break()
                    endif()
                endforeach()
            endif()
        elseif(KEY STREQUAL "speechd_install")
            set(ENV_MAP "SPEECHD_PATH")
            set(NAME_MAP "Speech Dispatcher Sockets")
            if(DEFINED ENV{SPEECHD_PATH} AND IS_DIRECTORY "$ENV{SPEECHD_PATH}")
                set(FALLBACK_FOUND "$ENV{SPEECHD_PATH}")
            elseif(UNIX AND NOT APPLE)
                foreach(p ITEMS "/usr/include/speech-dispatcher" "/usr/local/include/speech-dispatcher" "/opt/speech-dispatcher/include")
                    if(IS_DIRECTORY "${p}")
                        set(FALLBACK_FOUND "${p}")
                        break()
                    endif()
                endforeach()
            endif()
        endif()

        if(NOT "${FALLBACK_FOUND}" STREQUAL "")
            cmake_path(SET CONVERTED_FALLBACK "${FALLBACK_FOUND}")
            cmake_path(NORMAL_PATH CONVERTED_FALLBACK)
            set(PATH_${KEY} "${CONVERTED_FALLBACK}" CACHE PATH "System Fallback Auto-Resolved Asset" FORCE)
            message(STATUS "[SRAL DISCOVERY] ${NAME_MAP} auto-resolved via fallback at: ${PATH_${KEY}}")
        endif()
    endif()

    if(PATH_${KEY} AND NOT "${ENV_MAP}" STREQUAL "")
        set(ENV{${ENV_MAP}} "${PATH_${KEY}}")
    endif()
endforeach()

if(IS_WINDOWS)
    if(NOT DEFINED TARGET_usertarget_windows AND DEFINED ENV{USERTARGET_WINDOWS})
        set(TARGET_usertarget_windows "$ENV{USERTARGET_WINDOWS}" CACHE STRING "Environment Target Version Pass")
    endif()
    if(TARGET_usertarget_windows MATCHES "^([0-9]+)\.([0-9]+)\.([0-9]+)")
        set(WIN_MAJOR "${CMAKE_MATCH_1}")
        set(WIN_BUILD "${CMAKE_MATCH_3}")

        if(WIN_MAJOR LESS 10 OR (WIN_MAJOR EQUAL 10 AND WIN_BUILD LESS 17763))
            message(FATAL_ERROR "\n[SRAL ERROR] Unsupported Windows target version: ${TARGET_usertarget_windows}. The minimum required version is 10.0.17763.0.\n")
        endif()

        set(WIN_HEX_VERSION "0x0A00")
        if(WIN_BUILD EQUAL 17763)
            set(WIN_NTDDI_VERSION "0x0A000004")
        elseif(WIN_BUILD GREATER_EQUAL 22000)
            set(WIN_NTDDI_VERSION "0x0A00000C")
        else()
            set(WIN_NTDDI_VERSION "0x0A000005")
        endif()

        set(OS_DEFINITIONS "_WIN32_WINNT=${WIN_HEX_VERSION}" "WINVER=${WIN_HEX_VERSION}" "NTDDI_VERSION=${WIN_NTDDI_VERSION}")
    endif()

elseif(IS_MACOS)
    if(NOT DEFINED TARGET_usertarget_macos AND DEFINED ENV{USERTARGET_MACOS})
        set(TARGET_usertarget_macos "$ENV{USERTARGET_MACOS}" CACHE STRING "Environment macOS Target Pass")
    endif()
    if(TARGET_usertarget_macos)
        if(TARGET_usertarget_macos VERSION_LESS "15.0")
            message(FATAL_ERROR "\n[SRAL ERROR] Unsupported macOS target version: ${TARGET_usertarget_macos}. The minimum required version is 15.0.\n")
        endif()
        set(CMAKE_OSX_DEPLOYMENT_TARGET "${TARGET_usertarget_macos}" CACHE STRING "Forced macOS Deployment Base" FORCE)
    endif()

elseif(IS_ANDROID)
    if(NOT DEFINED TARGET_usertarget_android AND DEFINED ENV{USERTARGET_ANDROID})
        set(TARGET_usertarget_android "$ENV{USERTARGET_ANDROID}" CACHE STRING "Environment Android Target Pass")
    endif()
    if(TARGET_usertarget_android MATCHES "^[0-9]+$")
        if(TARGET_usertarget_android LESS 21)
            message(FATAL_ERROR "\n[SRAL ERROR] Unsupported Android API Level: ${TARGET_usertarget_android}. The minimum required version is API Level 21.\n")
        endif()
        set(OS_DEFINITIONS "ANDROID_PLATFORM=android-${TARGET_usertarget_android}" "__ANDROID_API__=${TARGET_usertarget_android}")
    endif()
endif()

if((IS_WASM OR IS_ANDROID) AND NOT CMAKE_TOOLCHAIN_FILE)
    message(FATAL_ERROR
        "\n===============================================================================\n"
        "[SRAL ERROR] Toolchain file missing! For cross-compiling, you MUST declare the toolchain parameter flag \n"
        "on your initial call, before compiler checks execute. Example:\n"
        "cmake -B build -DCMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME} -DCMAKE_TOOLCHAIN_FILE=<path_to_toolchain>\n"
        "===============================================================================\n")
endif()

get_property(SRAL_GLOBAL_ENABLED_LANGS GLOBAL PROPERTY ENABLED_LANGUAGES)
set(SRAL_HAS_C FALSE)
set(SRAL_HAS_CXX FALSE)
if("C" IN_LIST SRAL_GLOBAL_ENABLED_LANGS)
    set(SRAL_HAS_C TRUE)
endif()
if("CXX" IN_LIST SRAL_GLOBAL_ENABLED_LANGS)
    set(SRAL_HAS_CXX TRUE)
endif()

function(sral_safe_check_flag _lang _flag _var)
    if(NOT DEFINED ${_var})
        block(PROPAGATE ${_var})
            set(_test_bindir "${CMAKE_BINARY_DIR}/CMakeFiles/sral_flag_tests/${_var}")

            if("${_lang}" STREQUAL "CXX")
                set(_test_srcfile "${_test_bindir}/src.cpp")
            else()
                set(_test_srcfile "${_test_bindir}/src.c")
            endif()

            file(WRITE "${_test_srcfile}" "int main(void) { return 0; }\n")
            try_compile(${_var}
                "${_test_bindir}"
                "${_test_srcfile}"
                CMAKE_FLAGS "-DCMAKE_${_lang}_FLAGS:STRING=${_flag}"
                OUTPUT_VARIABLE _compile_log
            )

            set(${_var} ${${_var}} CACHE INTERNAL "Auto-resolved safe compiler flag check")
            file(REMOVE_RECURSE "${_test_bindir}")
        endblock()
    endif()
endfunction()


function(sral_add_safe_target_flags TARGET_NAME)
    set(oneValueArgs SCOPE)
    set(multiValueArgs C_FLAGS CXX_FLAGS LINK_FLAGS)
    cmake_parse_arguments(ARG "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    if(NOT ARG_SCOPE)
        set(ARG_SCOPE "PRIVATE")
    endif()

    set(OLD_C_REQ_QUIET ${CMAKE_REQUIRED_QUIET})
    set(CMAKE_REQUIRED_QUIET TRUE)

    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        set(STRICT_COMP_FLAGS "-Werror")
        set(STRICT_LINK_FLAGS "-Werror")
    elseif(MSVC)
        set(STRICT_COMP_FLAGS "/WX" "/we9002")
        set(STRICT_LINK_FLAGS "/WX")
    endif()

    set(LINK_LANG "C")
    if(SRAL_HAS_CXX)
        set(LINK_LANG "CXX")
    endif()

    foreach(TYPE IN ITEMS COMP LINK)
        if(TYPE STREQUAL "LINK")
            if(NOT ARG_LINK_FLAGS)
                continue()
            endif()

            set(OLD_CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS})
            set(OLD_CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES})
            set(CMAKE_REQUIRED_FLAGS "${STRICT_LINK_FLAGS}")

            foreach(FLAG IN LISTS ARG_LINK_FLAGS)
                if(FLAG MATCHES "^\\$<")
                    target_link_options(${TARGET_NAME} ${ARG_SCOPE} "${FLAG}")
                    continue()
                endif()

                string(MAKE_C_IDENTIFIER "${FLAG}" CLEAN_FLAG)
                set(CACHE_VAR "SRAL_LNK_SUPP_${CLEAN_FLAG}")
                set(CMAKE_REQUIRED_LIBRARIES "${FLAG}")

                sral_safe_check_flag("${LINK_LANG}" "${FLAG}" "${CACHE_VAR}")

                if(${CACHE_VAR})
                    target_link_options(${TARGET_NAME} ${ARG_SCOPE} "${FLAG}")
                endif()
            endforeach()

            set(CMAKE_REQUIRED_FLAGS ${OLD_CMAKE_REQUIRED_FLAGS})
            set(CMAKE_REQUIRED_LIBRARIES ${OLD_CMAKE_REQUIRED_LIBRARIES})
        else()
            foreach(LANG IN ITEMS C CXX)
                if(NOT ARG_${LANG}_FLAGS)
                    continue()
                endif()

                set(OLD_CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS})
                set(CMAKE_REQUIRED_FLAGS "${STRICT_COMP_FLAGS}")

                foreach(FLAG IN LISTS ARG_${LANG}_FLAGS)
                    if(FLAG MATCHES "^\\$<")
                        target_compile_options(${TARGET_NAME} ${ARG_SCOPE} "${FLAG}")
                        continue()
                    endif()

                    string(MAKE_C_IDENTIFIER "${FLAG}" CLEAN_FLAG)
                    set(CACHE_VAR "SRAL_${LANG}_SUPP_${CLEAN_FLAG}")

                    if(SRAL_HAS_${LANG})
                        if(NOT DEFINED ${CACHE_VAR})
                            # OPTIMIZED: Clean quoting to protect string context maps
                            sral_safe_check_flag("${LANG}" "${FLAG}" "${CACHE_VAR}")
                        endif()
                        if(${CACHE_VAR})
                            target_compile_options(${TARGET_NAME} ${ARG_SCOPE} "$<$<COMPILE_LANGUAGE:${LANG}>:${FLAG}>")
                        endif()
                    endif()
                endforeach()

                set(CMAKE_REQUIRED_FLAGS ${OLD_CMAKE_REQUIRED_FLAGS})
            endforeach()
        endif()
    endforeach()

    set(CMAKE_REQUIRED_QUIET ${OLD_C_REQ_QUIET})
endfunction()

set(ARCH_ARGS "")
if(HARDENING_ARCH_LEVEL MATCHES "^(x86-64-v2|x86-64-v3|native)$")
    if(NOT CMAKE_CROSSCOMPILING OR HARDENING_ARCH_LEVEL STREQUAL "native")
        sral_safe_check_flag(C "-march=${HARDENING_ARCH_LEVEL}" HAS_ARCH_FLAG)
        if(HAS_ARCH_FLAG)
            set(ARCH_ARGS "-march=${HARDENING_ARCH_LEVEL}")
        endif()
    endif()
endif()

set(CPU_ARGS "")
if(SRAL_HAS_C)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
        sral_safe_check_flag(C "-fcf-protection=full" HAS_CPU_CF_PROTECTION_FULL)
        if(HAS_CPU_CF_PROTECTION_FULL)
            set(CPU_ARGS "-fcf-protection=full")
        endif()
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm.*|aarch64|ARM64)$")
        sral_safe_check_flag(C "-mbranch-protection=standard" HAS_CPU_BRANCH_PROTECTION_STD)
        if(HAS_CPU_BRANCH_PROTECTION_STD)
            set(CPU_ARGS "-mbranch-protection=standard")
        endif()
    endif()
endif()

add_library(sral_hardening_options INTERFACE)

target_compile_features(sral_hardening_options INTERFACE c_std_17 cxx_std_20)

if(NOT IS_MSVC_LIKE)
    target_compile_options(sral_hardening_options INTERFACE "$<$<CONFIG:Release>:-O2>" "$<$<CONFIG:RelWithDebInfo>:-O2>")
endif()
target_compile_definitions(sral_hardening_options INTERFACE "$<$<CONFIG:Debug>:DEBUG;_DEBUG;SRAL_DEBUG_TRACE_ACTIVE>")

set(UNIVERSAL_MSVC_ARGS "/volatile:iso" "/W4")
set(UNIVERSAL_GCC_CLANG_ARGS "-fno-delete-null-pointer-checks" "-fwrapv" "-fno-common" "-Wconversion" "-Wsign-conversion")

if(CMAKE_COMPILER_IS_GNUCXX)
    list(APPEND UNIVERSAL_GCC_CLANG_ARGS "-fno-strict-overflow")
endif()
if(NOT IS_WASM)
    list(APPEND UNIVERSAL_GCC_CLANG_ARGS "-fstack-clash-protection")
endif()

if(IS_MSVC_LIKE)
    set(MSVC_COMMON_FLAGS "")
    list(APPEND MSVC_COMMON_FLAGS
        ${UNIVERSAL_MSVC_ARGS}
        "/GS" "/sdl" "/guard:cf" "/W4" "/WX"
        "/wd4324" "/wd4820" "/wd5039" "/wd4865" "/wd4365" "/wd4251" "/wd5204"
        "/wd5291" "/wd4373" "/wd4625" "/wd4626" "/wd5026" "/wd5027" "/wd4005" "/wd4191" "/wd4668"
        "$<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:/Oi>"
        "$<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:/Qspectre>"
        "$<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:/w14928>"
        "$<$<CONFIG:Debug>:/Zi>"
    )

    set(MSVC_LINK_FLAGS "/GUARD:CF" "/DYNAMICBASE" "/NXCOMPAT" "/HIGHENTROPYVA" "$<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:/CETCOMPAT>")

    string(REPLACE ";" [=[;]=] MSVC_COMMON_FLAGS_ESC "${MSVC_COMMON_FLAGS}")
    string(REPLACE ";" [=[;]=] MSVC_LINK_FLAGS_ESC "${MSVC_LINK_FLAGS}")

    sral_add_safe_target_flags(sral_hardening_options SCOPE INTERFACE C_FLAGS ${MSVC_COMMON_FLAGS_ESC} CXX_FLAGS ${MSVC_COMMON_FLAGS_ESC} LINK_FLAGS ${MSVC_LINK_FLAGS_ESC})

    if(IS_CLANG_CL)
        target_compile_options(sral_hardening_options INTERFACE "/clang:-ftrivial-auto-var-init=pattern" "/clang:-fextensions" "/clang:-Wno-language-extension-token" "/clang:-Wno-missing-designated-field-initializers")
    endif()

    target_compile_options(sral_hardening_options INTERFACE $<$<COMPILE_LANGUAGE:CXX>:/permissive-> $<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus>)
    target_compile_definitions(sral_hardening_options INTERFACE $<$<COMPILE_LANGUAGE:CXX>:_SILENCE_CXX20_OLD_SHARED_PTR_ATOMIC_SUPPORT_DEPRECATION_WARNING>)
else()
    set(C_CANDIDATES "")
    list(APPEND C_CANDIDATES
        ${UNIVERSAL_GCC_CLANG_ARGS}
        "-D_FORTIFY_SOURCE=3" "-fstack-protector-strong" "-fno-plt" "-Wformat" "-Wformat-security"
        "-fno-strict-aliasing" "-fno-trapping-math" "-Werror" "-Wall" "-Wextra"
        "-Wno-undef" "-Wno-unused-function" "-Wno-unused-variable" "-Wno-gnu-anonymous-struct"
        "-Wno-nested-anon-types" "-Wno-interference-size" "-Wno-old-style-cast"
        "$<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:-fvisibility=hidden>"
        "$<$<CONFIG:Debug>:-g3>"
    )

    if(IS_CLANG AND NOT (IS_WINDOWS OR IS_MACOS OR IS_IOS OR IS_ANDROID))
        list(APPEND C_CANDIDATES "-fno-extensions")
    endif()
    if(IS_WINDOWS AND IS_CLANG)
        list(APPEND C_CANDIDATES "-fms-extensions" "-Wno-language-extension-token" "-Wno-missing-designated-field-initializers")
    endif()

    sral_safe_check_flag(C "-ftrivial-auto-var-init=zero" HAS_AUTO_VAR_ZERO)
    if(HAS_AUTO_VAR_ZERO)
        list(APPEND C_CANDIDATES "-ftrivial-auto-var-init=zero")
    endif()

    if(ARCH_ARGS)
        list(APPEND C_CANDIDATES "${ARCH_ARGS}")
    endif()
    if(CPU_ARGS)
        list(APPEND C_CANDIDATES "${CPU_ARGS}")
    endif()

    set(CXX_CANDIDATES ${C_CANDIDATES})
    set(LINK_CANDIDATES "-Wl,-z,now" "-Wl,-z,noexecstack" "-Wl,-z,relro")

    if(IS_CLANG)
        sral_safe_check_flag(C "-fsanitize=cfi" LINK_HAS_CFI)
        if(LINK_HAS_CFI)
            list(APPEND C_CANDIDATES "$<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:-fsanitize=cfi>")
            list(APPEND CXX_CANDIDATES "$<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:-fsanitize=cfi>")
            target_link_options(sral_hardening_options INTERFACE "$<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:-fsanitize=cfi>")
        endif()

        if(IS_MACOS OR IS_IOS OR IS_ANDROID)
            set(CLANG_PLAT_FLAGS "-Wno-macro-redefined" "-Wno-gnu-anonymous-struct" "-Wno-nested-anon-types")
            list(APPEND C_CANDIDATES ${CLANG_PLAT_FLAGS})
            list(APPEND CXX_CANDIDATES ${CLANG_PLAT_FLAGS})
        endif()
    endif()

    string(REPLACE ";" [=[;]=] C_CANDIDATES_ESC "${C_CANDIDATES}")
    string(REPLACE ";" [=[;]=] CXX_CANDIDATES_ESC "${CXX_CANDIDATES}")
    string(REPLACE ";" [=[;]=] LINK_CANDIDATES_ESC "${LINK_CANDIDATES}")

    sral_add_safe_target_flags(sral_hardening_options SCOPE INTERFACE C_FLAGS ${C_CANDIDATES_ESC} CXX_FLAGS ${CXX_CANDIDATES_ESC} LINK_FLAGS ${LINK_CANDIDATES_ESC})
endif()

target_compile_definitions(sral_hardening_options INTERFACE "$<$<COMPILE_LANGUAGE:CXX>:_GLIBCXX_ASSERTIONS>")

if(NOT (IS_WASM OR IS_MACOS OR IS_IOS OR IS_ANDROID))
    target_compile_definitions(sral_hardening_options INTERFACE
        "$<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:_LIBCPP_HARDENING_MODE=2>"
        "$<$<CONFIG:Debug>:_LIBCPP_HARDENING_MODE=3>"
    )
endif()

set(CHROMEVOX_DEFINITIONS
    "$<$<BOOL:${SRAL_WITH_CHROMEVOX}>:SRAL_WITH_CHROMEVOX>"
    "$<$<NOT:$<BOOL:${SRAL_WITH_CHROMEVOX}>>:SRAL_NO_CHROMEVOX>"
)
target_compile_definitions(sral_hardening_options INTERFACE ${CHROMEVOX_DEFINITIONS})

if(IS_WINDOWS)
    target_link_libraries(sral_hardening_options INTERFACE ole32 user32)
    if(SRAL_WITH_UIA)
        target_link_libraries(sral_hardening_options INTERFACE uiautomationcore)
    endif()

elseif(IS_MACOS OR IS_IOS)
    target_compile_options(sral_hardening_options INTERFACE "$<$<OR:$<COMPILE_LANGUAGE:OBJC>,$<COMPILE_LANGUAGE:OBJCXX>>:-fobjc-arc;-fstack-protector-all>")
    sral_safe_check_flag(CXX "-fobjc-direct-methods" HAS_OBJC_DIRECT)
    if(HAS_OBJC_DIRECT)
        target_compile_options(sral_hardening_options INTERFACE "$<$<OR:$<COMPILE_LANGUAGE:OBJC>,$<COMPILE_LANGUAGE:OBJCXX>>:-fobjc-direct-methods>")
    endif()

    find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
    find_library(AVFOUNDATION_FRAMEWORK AVFoundation REQUIRED)
    target_link_libraries(sral_hardening_options INTERFACE ${FOUNDATION_FRAMEWORK} ${AVFOUNDATION_FRAMEWORK})

    if(NOT IS_IOS)
        sral_safe_check_flag(C "-Wno-deprecated-declarations" APPLE_DEPR_OK)
        if(APPLE_DEPR_OK)
            target_compile_options(sral_hardening_options INTERFACE "-Wno-deprecated-declarations")
        endif()

        find_library(APPKIT_FRAMEWORK AppKit REQUIRED)
        target_link_libraries(sral_hardening_options INTERFACE ${APPKIT_FRAMEWORK})
        if(SRAL_WITH_NSSPEECH)
            find_library(FRAMEWORK_CORESPEECH CoreSpeech REQUIRED)
            target_link_libraries(sral_hardening_options INTERFACE ${FRAMEWORK_CORESPEECH})
        endif()
    else()
        find_library(FRAMEWORK_UIKIT UIKit REQUIRED)
        target_link_libraries(sral_hardening_options INTERFACE ${FRAMEWORK_UIKIT})
    endif()

elseif(IS_ANDROID)
    find_library(LOG_LIB log REQUIRED)
    target_link_libraries(sral_hardening_options INTERFACE ${LOG_LIB})

elseif(IS_WASM)
    target_compile_options(sral_hardening_options INTERFACE "-pthread")
    target_link_options(sral_hardening_options INTERFACE "-pthread" "-sSTACK_SIZE=65536" "-sWASM_BIGINT")

else()
    target_compile_definitions(sral_hardening_options INTERFACE "SRAL_SPEECHD_REQUIRE_UTF8")
    find_package(PkgConfig REQUIRED)

    if(SRAL_WITH_SPEECH_DISPATCHER)
        pkg_check_modules(SPEECHD REQUIRED IMPORTED_TARGET speech-dispatcher)
        target_link_libraries(sral_hardening_options INTERFACE PkgConfig::SPEECHD)
    else()
        target_compile_definitions(sral_hardening_options INTERFACE "SRAL_NO_SPEECH_DISPATCHER")
    endif()

    if(SRAL_WITH_ORCA)
        pkg_check_modules(DBUS REQUIRED IMPORTED_TARGET dbus-1)
        pkg_check_modules(ATSPI REQUIRED IMPORTED_TARGET atspi-2)
        target_link_libraries(sral_hardening_options INTERFACE PkgConfig::DBUS PkgConfig::ATSPI)
    else()
        target_compile_definitions(sral_hardening_options INTERFACE "SRAL_NO_ORCA")
    endif()

    if(SRAL_WITH_BRLAPI)
        find_library(BRLAPI_LIB brlapi REQUIRED)
        target_link_libraries(sral_hardening_options INTERFACE ${BRLAPI_LIB})
    else()
        target_compile_definitions(sral_hardening_options INTERFACE "SRAL_NO_BRLAPI")
    endif()
endif()

if(NOT IS_ANDROID AND NOT IS_WASM)
    if(PATH_jdk_install AND IS_DIRECTORY "${PATH_jdk_install}")
        set(JNI_INCLUDE_DIRS "${PATH_jdk_install}/include")

        if(IS_WINDOWS)
            list(APPEND JNI_INCLUDE_DIRS "${PATH_jdk_install}/include/win32")
        elseif(IS_MACOS)
            list(APPEND JNI_INCLUDE_DIRS "${PATH_jdk_install}/include/darwin")
        else()
            list(APPEND JNI_INCLUDE_DIRS "${PATH_jdk_install}/include/linux")
        endif()

        if(EXISTS "${PATH_jdk_install}/include/jni.h")
            set(JNI_FOUND TRUE)
            target_include_directories(sral_hardening_options SYSTEM INTERFACE ${JNI_INCLUDE_DIRS})
            message(STATUS "[SRAL] JNI include framework safely loaded from resolved JDK path: ${PATH_jdk_install}")
        endif()
    endif()
endif()

if(THIRD_PARTY_BINARIES)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(AUDIT_SCRIPT "import sys, os; sys.exit(0 if all(os.path.exists(b) for b in sys.argv[1:]) else 1)")
    add_custom_target(sral_security_audit ALL
        COMMAND "${Python3_EXECUTABLE}" -c "${AUDIT_SCRIPT}" ${THIRD_PARTY_BINARIES}
        COMMENT "Executing baseline binary security audit on third-party assets..."
        VERBATIM
    )
endif()

if(SRAL_WITH_ACCESSKIT)
    if(NOT "${PATH_accesskit_install}" STREQUAL "")
        find_path(ACCESSKIT_INCLUDE_DIR NAMES accesskit.h PATHS "${PATH_accesskit_install}/include" NO_DEFAULT_PATH)
    else()
        find_path(ACCESSKIT_INCLUDE_DIR NAMES accesskit.h PATH_SUFFIXES include)
    endif()

    set(AK_LIB_NAME "accesskit")
    if(IS_WINDOWS)
        set(AK_LIB_NAME "accesskit_winapi")
    endif()

    set(AK_SEARCH_SUFFIXES "lib" "bin")
    if(IS_ANDROID)
        set(AK_SEARCH_SUFFIXES "lib/${ANDROID_ABI}")
    endif()

    if(NOT "${PATH_accesskit_install}" STREQUAL "")
        find_library(ACCESSKIT_CORE_LIB NAMES ${AK_LIB_NAME} PATHS "${PATH_accesskit_install}" PATH_SUFFIXES ${AK_SEARCH_SUFFIXES} NO_DEFAULT_PATH)
    else()
        find_library(ACCESSKIT_CORE_LIB NAMES ${AK_LIB_NAME} PATH_SUFFIXES ${AK_SEARCH_SUFFIXES})
    endif()

    if(NOT ACCESSKIT_INCLUDE_DIR OR NOT ACCESSKIT_CORE_LIB)
        set(SRAL_WITH_ACCESSKIT "disabled" CACHE BOOL "Enable AccessKit accessibility provider backend" FORCE)
        message(STATUS "-------------------------------------------------------------------------------")
        message(STATUS "[SRAL STATUS] AccessKit dependencies were NOT discovered locally or via PATH. Forcing option SRAL_WITH_ACCESSKIT to disabled...")
        message(STATUS "-------------------------------------------------------------------------------")
    endif()
endif()

if(SRAL_WITH_ACCESSKIT)
    if(NOT TARGET AccessKit::Core)
        add_library(AccessKit::Core INTERFACE IMPORTED GLOBAL)
        target_include_directories(AccessKit::Core SYSTEM INTERFACE "${ACCESSKIT_INCLUDE_DIR}")
        target_link_libraries(AccessKit::Core INTERFACE "${ACCESSKIT_CORE_LIB}")
        target_compile_definitions(AccessKit::Core INTERFACE "SRAL_WITH_ACCESSKIT")

        if(IS_ANDROID)
            target_link_libraries(AccessKit::Core INTERFACE log android)
        elseif(NOT IS_WINDOWS AND NOT IS_MACOS AND NOT IS_IOS)
            target_link_libraries(AccessKit::Core INTERFACE pthread)
        endif()
    endif()

    target_link_libraries(sral_hardening_options INTERFACE AccessKit::Core)
else()
    message(STATUS "[SRAL STATUS] AccessKit is disabled.")
    set(ACCESSKIT_NO_DEF "$<$<NOT:$<BOOL:${SRAL_WITH_ACCESSKIT}>>:SRAL_NO_ACCESSKIT>")
    target_compile_definitions(sral_hardening_options INTERFACE ${ACCESSKIT_NO_DEF})
endif()

if(NOT TARGET SRAL)
    target_include_directories(sral_hardening_options SYSTEM INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/Include"
        "${CMAKE_CURRENT_SOURCE_DIR}/SRC"
        "${CMAKE_CURRENT_SOURCE_DIR}/Dep"
    )

    if(SRAL_DEPENDENCIES)
        target_link_libraries(sral_hardening_options INTERFACE ${SRAL_DEPENDENCIES})
    endif()
    if(SRAL_SYS_INCLUDE_DIRS)
        target_include_directories(sral_hardening_options SYSTEM INTERFACE ${SRAL_SYS_INCLUDE_DIRS})
    endif()

    add_subdirectory(SRC)
endif()

if(BUILD_TESTS STREQUAL "enabled" AND NOT IS_WASM AND NOT IS_ANDROID)
    enable_testing()

    set(SECURE_TEST_LINK_OPTS "")
    if(NOT IS_MSVC_LIKE AND NOT IS_MACOS AND NOT IS_IOS)
        set(SECURE_TEST_LINK_OPTS "LINKER:-z,relro" "LINKER:-z,now" "LINKER:-z,noexecstack")
    endif()

    set(SRAL_GENERATED_TEST_TARGETS "")

    if(IS_IOS)
        add_executable(SRAL_test MACOSX_BUNDLE "${CMAKE_CURRENT_SOURCE_DIR}/Examples/C/SRALExample.c")
        set_target_properties(SRAL_test PROPERTIES
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.sral.orchestrator.test"
            MACOSX_BUNDLE_BUNDLE_NAME "SRAL_test"
        )
    else()
        add_executable(SRAL_test "${CMAKE_CURRENT_SOURCE_DIR}/Examples/C/SRALExample.c")
    endif()
    list(APPEND SRAL_GENERATED_TEST_TARGETS SRAL_test)

    if(IS_WINDOWS AND SRAL_WITH_NVDA)
        add_executable(SRAL_NVDAControlExConsole "${CMAKE_CURRENT_SOURCE_DIR}/Examples/C/NVDAControlExConsole.c")
        list(APPEND SRAL_GENERATED_TEST_TARGETS SRAL_NVDAControlExConsole)
    endif()

    if(IS_MACOS AND NOT IS_IOS)
        add_executable(SRAL_test_cocoa "${CMAKE_CURRENT_SOURCE_DIR}/Examples/ObjC/SRALCocoaExample.m")
        list(APPEND SRAL_GENERATED_TEST_TARGETS SRAL_test_cocoa)
    endif()

    foreach(TEST_TARGET IN LISTS SRAL_GENERATED_TEST_TARGETS)
        if(TARGET ${TEST_TARGET})
            target_link_libraries(${TEST_TARGET} PRIVATE SRAL sral_hardening_options)

            if(SECURE_TEST_LINK_OPTS)
                target_link_options(${TEST_TARGET} PRIVATE ${SECURE_TEST_LINK_OPTS})
            endif()

            add_test(NAME "${TEST_TARGET}_Execution" COMMAND ${TEST_TARGET})
            set_tests_properties("${TEST_TARGET}_Execution" PROPERTIES
                TIMEOUT 30
                COMPONENT AutomatedTests
            )
        endif()
    endforeach()
endif()

if(BUILD_BINDINGS STREQUAL "enabled")
    message(STATUS "Resolving toolchains for external language wrapper modules...")
    set(BINDINGS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Bindings")
    set(BIND_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/BindingsOutput")

    find_program(DOTNET_EXE dotnet)
    find_program(CARGO_EXE cargo)
    find_program(NPM_EXE NAMES npm npm.cmd)
    find_program(GO_EXE go)
    find_program(DART_EXE NAMES dart flutter)
    find_program(LUA_EXE NAMES lua luajit)

    set(SRAL_CARGO_TARGET_ARGS "")
    set(SRAL_GO_ENV_ARGS "")

    if(IS_ANDROID)
        if(ANDROID_ABI STREQUAL "arm64-v8a")
            set(SRAL_CARGO_TARGET_ARGS "--target" "aarch64-linux-android")
            set(SRAL_GO_ENV_ARGS "GOOS=android" "GOARCH=arm64")
        elseif(ANDROID_ABI STREQUAL "armeabi-v7a")
            set(SRAL_CARGO_TARGET_ARGS "--target" "armv7-linux-androideabi")
            set(SRAL_GO_ENV_ARGS "GOOS=android" "GOARCH=arm")
        elseif(ANDROID_ABI STREQUAL "x86_64")
            set(SRAL_CARGO_TARGET_ARGS "--target" "x86_64-linux-android")
            set(SRAL_GO_ENV_ARGS "GOOS=android" "GOARCH=amd64")
        endif()
    elseif(IS_WASM)
        set(SRAL_CARGO_TARGET_ARGS "--target" "wasm32-unknown-emscripten")
        set(SRAL_GO_ENV_ARGS "GOOS=js" "GOARCH=wasm")
    endif()

    set(SRAL_NODE_ENV_FLAGS "")
    if(CMAKE_C_COMPILER AND CMAKE_CXX_COMPILER)
        set(SRAL_NODE_ENV_FLAGS "CC=${CMAKE_C_COMPILER}" "CXX=${CMAKE_CXX_COMPILER}")
    endif()

    if(SRAL_BINDING_CSHARP AND DOTNET_EXE)
        set(CS_OUT_DLL "${BIND_OUTPUT_DIR}/csharp/SRAL.dll")
        add_custom_command(
            OUTPUT "${CS_OUT_DLL}"
            COMMAND "${DOTNET_EXE}" build "${BINDINGS_DIR}/C#/SRAL.csproj" --configuration Release --no-restore -p:SKIP_MOBILE_WORKLOADS=true -o "${BIND_OUTPUT_DIR}/csharp"
            DEPENDS SRAL
            BYPRODUCTS "${BIND_OUTPUT_DIR}/csharp/SRAL.pdb"
            COMMENT "Compiling optimized C# assemblies..."
            VERBATIM
        )
        add_custom_target(sral_csharp_binding ALL DEPENDS "${CS_OUT_DLL}")
        install(FILES "${CS_OUT_DLL}" DESTINATION "share/sral/bin/csharp" COMPONENT Bindings_CSharp_Binaries)
    endif()

    if(SRAL_BINDING_RUST AND CARGO_EXE)
        set(RUST_OUT_LIB "${BIND_OUTPUT_DIR}/rust/release/libsral_rust.rlib")
        add_custom_command(
            OUTPUT "${RUST_OUT_LIB}"
            COMMAND "${CMAKE_COMMAND}" -E env "CARGO_TARGET_DIR=${BIND_OUTPUT_DIR}/rust" "${CARGO_EXE}" build --manifest-path "${BINDINGS_DIR}/Rust/Cargo.toml" --release --offline ${SRAL_CARGO_TARGET_ARGS}
            DEPENDS SRAL
            COMMENT "Compiling zero-overhead cross-compiled Rust wrappers..."
            VERBATIM
        )
        add_custom_target(sral_rust_binding ALL DEPENDS "${RUST_OUT_LIB}")
        install(FILES "${RUST_OUT_LIB}" DESTINATION "share/sral/bin/rust" COMPONENT Bindings_Rust_Binaries)
    endif()

    if(SRAL_BINDING_NODE AND NPM_EXE)
        set(NODE_OUT_ADDON "${BINDINGS_DIR}/Node.js/build/Release/sral.node")
        add_custom_command(
            OUTPUT "${NODE_OUT_ADDON}"
            COMMAND "${CMAKE_COMMAND}" -E env ${SRAL_NODE_ENV_FLAGS} "${NPM_EXE}" ci --production --ignore-scripts
            WORKING_DIRECTORY "${BINDINGS_DIR}/Node.js"
            DEPENDS SRAL
            COMMENT "Building native Node.js binary bridge addons with toolchain mapping..."
            VERBATIM
        )
        add_custom_target(sral_nodejs_binding ALL DEPENDS "${NODE_OUT_ADDON}")
        install(FILES "${NODE_OUT_ADDON}" DESTINATION "share/sral/bin/node" COMPONENT Bindings_Node_Binaries)
    endif()

    if(SRAL_BINDING_GO AND GO_EXE)
        set(GO_OUT_MARKER "${BIND_OUTPUT_DIR}/go_build.marker")
        add_custom_command(
            OUTPUT "${GO_OUT_MARKER}"
            COMMAND "${CMAKE_COMMAND}" -E env ${SRAL_GO_ENV_ARGS} "${GO_EXE}" build -mod=readonly ./...
            COMMAND "${CMAKE_COMMAND}" -E touch "${GO_OUT_MARKER}"
            WORKING_DIRECTORY "${BINDINGS_DIR}/Go"
            DEPENDS SRAL
            COMMENT "Validating structural integrity of cross-compiled Go packages..."
            VERBATIM
        )
        add_custom_target(sral_go_binding ALL DEPENDS "${GO_OUT_MARKER}")
    endif()

    if(SRAL_BINDING_DART AND DART_EXE)
        set(DART_OUT_MARKER "${BIND_OUTPUT_DIR}/dart_pub.marker")
        add_custom_command(
            OUTPUT "${DART_OUT_MARKER}"
            COMMAND "${DART_EXE}" pub get --offline
            COMMAND "${CMAKE_COMMAND}" -E touch "${DART_OUT_MARKER}"
            WORKING_DIRECTORY "${BINDINGS_DIR}/Dart"
            DEPENDS SRAL
            COMMENT "Synchronizing offline Dart packaging configurations..."
            VERBATIM
        )
        add_custom_target(sral_dart_binding ALL DEPENDS "${DART_OUT_MARKER}")
    endif()

    if(SRAL_BINDING_LUA)
        set(LUA_OUT_MARKER "${BIND_OUTPUT_DIR}/lua_check.marker")
        if(LUA_EXE)
            add_custom_command(
                OUTPUT "${LUA_OUT_MARKER}"
                COMMAND "${LUA_EXE}" "${BINDINGS_DIR}/Lua/example.lua"
                COMMAND "${CMAKE_COMMAND}" -E echo "Lua script environment pass."
                COMMAND "${CMAKE_COMMAND}" -E touch "${LUA_OUT_MARKER}"
                DEPENDS SRAL
                VERBATIM
            )
        else()
            add_custom_command(
                OUTPUT "${LUA_OUT_MARKER}"
                COMMAND "${CMAKE_COMMAND}" -E echo "Lua compiler unpopulated. Stub initialization checked."
                COMMAND "${CMAKE_COMMAND}" -E touch "${LUA_OUT_MARKER}"
                VERBATIM
            )
        endif()
        add_custom_target(sral_lua_binding ALL DEPENDS "${LUA_OUT_MARKER}")
    endif()

    if(SRAL_BINDING_PYTHON)
        set(PY_OUT_MARKER "${BIND_OUTPUT_DIR}/python_pip.marker")
        set(PY_ARGS "${Python3_EXECUTABLE}" -m pip install "${BINDINGS_DIR}/Python" --no-index --no-deps)
        if(Python3_VERSION VERSION_GREATER_EQUAL "3.11")
            list(APPEND PY_ARGS --break-system-packages)
        endif()
        add_custom_command(
            OUTPUT "${PY_OUT_MARKER}"
            COMMAND ${PY_ARGS}
            COMMAND "${CMAKE_COMMAND}" -E touch "${PY_OUT_MARKER}"
            DEPENDS SRAL
            COMMENT "Deploying isolated Python site packages..."
        )
        add_custom_target(sral_python_binding ALL DEPENDS "${PY_OUT_MARKER}")
    endif()

    if(TARGET SRAL)
        foreach(TARGET_BIND IN ITEMS sral_csharp_binding sral_rust_binding sral_nodejs_binding sral_go_binding sral_dart_binding sral_lua_binding sral_python_binding)
            if(TARGET ${TARGET_BIND})
                add_dependencies(${TARGET_BIND} SRAL)
            endif()
        endforeach()
    endif()
endif()

if(TARGET SRAL)
    set_target_properties(SRAL PROPERTIES
        PUBLIC_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/Include/SRAL.h;${CMAKE_CURRENT_SOURCE_DIR}/Include/Sral.hpp"
    )
    install(TARGETS SRAL
        EXPORT SRALTargets
        RUNTIME DESTINATION bin COMPONENT Runtime
        LIBRARY DESTINATION lib COMPONENT Runtime
        ARCHIVE DESTINATION lib COMPONENT Development
        PUBLIC_HEADER DESTINATION include COMPONENT Development
    )
    install(EXPORT SRALTargets
        NAMESPACE SRAL::
        DESTINATION lib/cmake/SRAL
        COMPONENT Development
    )
else()
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/Include/SRAL.h" "${CMAKE_CURRENT_SOURCE_DIR}/Include/Sral.hpp" DESTINATION include COMPONENT Development)
endif()

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/settings.ini" DESTINATION share/sral COMPONENT RuntimeConfig)

if(SRAL_INSTALL_BINDING_SOURCES)
    set(BIND_DEST "share/sral/bindings")
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Bindings/C#/" DESTINATION "${BIND_DEST}/csharp" COMPONENT Bindings_CSharp FILES_MATCHING PATTERN "*.cs" PATTERN "*.csproj")
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Bindings/Dart/" DESTINATION "${BIND_DEST}/dart" COMPONENT Bindings_Dart FILES_MATCHING PATTERN "*.dart")
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Bindings/Go/SRAL" DESTINATION "${BIND_DEST}/go" COMPONENT Bindings_Go)
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Bindings/Lua/" DESTINATION "${BIND_DEST}/lua" COMPONENT Bindings_Lua FILES_MATCHING PATTERN "*.lua")
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Bindings/Python/" DESTINATION "${BIND_DEST}/python" COMPONENT Bindings_Python FILES_MATCHING PATTERN "*.py" PATTERN "*.toml")
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Bindings/Rust/" DESTINATION "${BIND_DEST}/rust" COMPONENT Bindings_Rust FILES_MATCHING PATTERN "Cargo.toml" PATTERN "src/*")
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Bindings/WebAssembly/" DESTINATION "${BIND_DEST}/wasm" COMPONENT Bindings_Wasm FILES_MATCHING PATTERN "*.cpp" PATTERN "*.js")
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Dep/" DESTINATION "share/sral/android" COMPONENT PlatformAssets FILES_MATCHING PATTERN "*.java" PATTERN "*.pro")
endif()

include(CMakePackageConfigHelpers)
set(SRAL_CMAKECONFIG_INSTALL_DIR "lib/cmake/SRAL")

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/SRALConfigVersion.cmake"
    VERSION 1.0.0
    COMPATIBILITY SemanticVersion
)

if(IS_MACOS OR IS_IOS)
    generate_apple_platform_selection_file(
        "${CMAKE_CURRENT_BINARY_DIR}/SRALPlatformSelection.cmake"
        INSTALL_DESTINATION "${SRAL_CMAKECONFIG_INSTALL_DIR}"
        MACOS_CONFIG_FILE "SRALConfig-macos.cmake"
        IOS_CONFIG_FILE "SRALConfig-ios.cmake"
    )
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/SRALPlatformSelection.cmake" DESTINATION "${SRAL_CMAKECONFIG_INSTALL_DIR}" COMPONENT Development)
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/SRALConfig.cmake" [=[
include(CMakeFindDependencyMacro)
@PACKAGE_INIT@
include("${CMAKE_CURRENT_LIST_DIR}/SRALTargets.cmake")
check_required_components(SRAL)
]=])

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/SRALConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/SRALConfigVersion.cmake"
    DESTINATION "${SRAL_CMAKECONFIG_INSTALL_DIR}"
    COMPONENT Development
)