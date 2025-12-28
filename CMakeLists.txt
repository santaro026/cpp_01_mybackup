cmake_minimum_required(VERSION 3.10)

# project name
project(main VERSION 0.0.0
        DESCRIPTION "My C++ Project"
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB SRC_FILES "${PROJECT_SOURCE_DIR}/*.cpp")

add_executable(${PROJECT_NAME}
    ${PROJECT_SOURCE_DIR}/src/main.cpp
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

install(TARGETS ${PROJECT_NAME} DESTINATION ${PROJECT_SOURCE_DIR}/bin/${PROJECT_VERSION})

set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "${PROJECT_NAME}-${PROJECT_VERSION}"
)

if(WIN32)
    target_compile_definitions(${PROJECT_NAME}
        PRIVATE
            WINDOWS_PLATFORM
    )
elseif(UNIX AND NOT APPLE)
    target_compile_definitions(${PROJECT_NAME}
        PRIVATE
            LINUX_PLATFORM
    )
endif()


