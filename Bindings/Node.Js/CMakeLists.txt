cmake_minimum_required(VERSION 4.3.1)
project(SRAL_bridge)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(NODE_ADDON_API_DIR "${CMAKE_CURRENT_SOURCE_DIR}/node_modules/node-addon-api")

set(SOURCE_FILES "src/SRAL_bridge.cpp")

if(NOT ANDROID AND NOT IOS)
    list(APPEND SOURCE_FILES "${NODE_ADDON_API_DIR}/node_api.cc")
endif()

add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES} ${CMAKE_JS_SRC})

if(ANDROID OR IOS)
    set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "lib")
else()
    set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" SUFFIX ".node")
endif()

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /EHsc)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -fexceptions)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_JS_INC}
    "${NODE_ADDON_API_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../Include"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE NAPI_DISABLE_CPP_EXCEPTIONS)

if(NOT ANDROID AND NOT IOS)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_JS_LIB})
endif()

if(WIN32)
    if(SRAL_CORE_LIB_DIR)
        get_filename_component(SRAL_ABSOLUTE_LIB_DIR "${SRAL_CORE_LIB_DIR}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()

    set(SEARCH_PATHS
        "${SRAL_ABSOLUTE_LIB_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../out/build"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../out/build/Release"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../out/build/Debug"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../out/build/x64-Release"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../out/build/x64-Debug"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../out/build/Ninja-Release"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../out/build/Ninja-Debug"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../build"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../build/Release"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../build/Debug"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../x64/Release"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../x64/Debug"
    )

    find_file(SRAL_LIB_PATH 
        NAMES SRAL.lib
        PATHS ${SEARCH_PATHS}
        NO_DEFAULT_PATH
    )

    if(SRAL_LIB_PATH)
        message(STATUS "[SUCCESS] Found core dependency linkage asset at: ${SRAL_LIB_PATH}")
        target_link_libraries(${PROJECT_NAME} PRIVATE "${SRAL_LIB_PATH}")
    else()
        string(REPLACE ";" "\n  - " SEARCH_PATHS_DISPLAY "${SEARCH_PATHS}")
        message(FATAL_ERROR 
            "Linker Error: SRAL.lib could not be found anywhere.\n"
            "Please ensure you have compiled your core C++ SRAL library project first!\n\n"
            "Scanned Locations:\n  - ${SEARCH_PATHS_DISPLAY}\n"
        )
    endif()

elseif(ANDROID)
    target_link_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../out/build/android/${ANDROID_ABI}")
    target_link_libraries(${PROJECT_NAME} PRIVATE sral)
elseif(IOS)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0")
    target_link_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../out/build/ios")
    target_link_libraries(${PROJECT_NAME} PRIVATE sral)
else()
    if(APPLE)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15")
    endif()
    target_link_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../out/build")
    target_link_libraries(${PROJECT_NAME} PRIVATE sral)
endif()
