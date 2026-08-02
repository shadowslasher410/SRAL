cmake_minimum_required(VERSION 4.4.0)

project(SRAL_bridge)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(NODE_ADDON_API_DIR "${CMAKE_CURRENT_SOURCE_DIR}/node_modules/node-addon-api")
set(SOURCE_FILES "src/SRAL_bridge.cpp")

add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES} ${CMAKE_JS_SRC})

if(ANDROID OR IOS)
    set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "lib")
else()
    set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" SUFFIX ".node")
endif()

if(MSVC OR CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
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

if(TARGET SRAL)
    message(STATUS "[SRAL BRIDGE] Internal master target detected. Binding link paths concurrently...")
    target_link_libraries(${PROJECT_NAME} PRIVATE SRAL)
else()
    message(STATUS "[SRAL BRIDGE] Standalone compilation pass active. Running search sweeps...")

    set(SRAL_SEARCH_PATHS "")
    
    if(SRAL_CORE_LIB_DIR)
        get_filename_component(SRAL_ABSOLUTE_LIB_DIR "${SRAL_CORE_LIB_DIR}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        list(APPEND SRAL_SEARCH_PATHS "${SRAL_ABSOLUTE_LIB_DIR}")
    endif()
    
    if(DEFINED ENV{CMAKE_BINARY_DIR})
        list(APPEND SRAL_SEARCH_PATHS "$ENV{CMAKE_BINARY_DIR}/SRC")
    endif()
    
    list(APPEND SRAL_SEARCH_PATHS 
        "${CMAKE_CURRENT_SOURCE_DIR}/../../build/SRC"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../build"
    )

    set(SRAL_LIB_TOKEN "SRAL")
    if(NOT MSVC AND NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
        set(SRAL_LIB_TOKEN "SRAL")
    endif()

    find_library(SRAL_CORE_ASSET
        NAMES ${SRAL_LIB_TOKEN}
        PATHS ${SRAL_SEARCH_PATHS}
        NO_DEFAULT_PATH
    )

    if(SRAL_CORE_ASSET)
        message(STATUS "[SUCCESS] Linked core dependency linkage asset at: ${SRAL_CORE_ASSET}")
        target_link_libraries(${PROJECT_NAME} PRIVATE "${SRAL_CORE_ASSET}")
    else()
        string(REPLACE ";" "\n  - " SEARCH_PATHS_DISPLAY "${SRAL_SEARCH_PATHS}")
        message(FATAL_ERROR 
            "Linker Error: Core binary dependency asset could not be discovered anywhere.\n"
            "Please ensure you have successfully compiled your C++ SRAL target library project first!\n\n"
            "Scanned Locations:\n  - ${SEARCH_PATHS_DISPLAY}\n"
        )
    endif()
endif()

if(ANDROID OR IOS)
    if(APPLE)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0")
    endif()
endif()