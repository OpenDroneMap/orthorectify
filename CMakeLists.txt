cmake_minimum_required(VERSION 3.21)

project(Orthorectify VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build" FORCE)
endif()

if(MSVC)
    string(REPLACE "/O2" "/Ox" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
endif()

configure_file(version.h.in "${CMAKE_BINARY_DIR}/version.h")

find_package(GDAL REQUIRED)
find_package(Eigen3 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(cxxopts CONFIG REQUIRED)
find_package(plog CONFIG REQUIRED)
find_package(OpenMP REQUIRED)

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif()

add_subdirectory("src")

add_executable(${PROJECT_NAME} ${SRC_LIST})

target_compile_definitions(${PROJECT_NAME} PRIVATE
    "$<$<CONFIG:DEBUG>:DEBUG>"
)

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_BINARY_DIR}"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    GDAL::GDAL
    Eigen3::Eigen
    nlohmann_json::nlohmann_json
    cxxopts::cxxopts
    plog::plog
    OpenMP::OpenMP_CXX
)

if(NOT WIN32 AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE stdc++fs)
endif()

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
