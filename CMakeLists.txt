cmake_minimum_required(VERSION 3.10)

project(Orthorectify VERSION 1.0)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

string(REPLACE "/O2" "/Ox" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")

add_subdirectory("src")
add_subdirectory("vendor")

configure_file(version.h.in version.h)

source_group("Headers" FILES ${HEADERS_LIST})

# Add exectuteable
add_executable(${PROJECT_NAME} ${SRC_LIST} ${HEADERS_LIST})
set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 17
)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING
      "Choose the type of build, options are: Debug Release"
      FORCE)
endif(NOT CMAKE_BUILD_TYPE)

#message("CMAKE_CXX_FLAGS_DEBUG is ${CMAKE_CXX_FLAGS_DEBUG}")
#message("CMAKE_CXX_FLAGS_RELEASE is ${CMAKE_CXX_FLAGS_RELEASE}")

if (WIN32)
    set(CMAKE_CONFIGURATION_TYPES "${CMAKE_BUILD_TYPE}")
endif()

target_compile_definitions(${PROJECT_NAME} PUBLIC "$<$<CONFIG:DEBUG>:DEBUG>")

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif()

set(DOWNLOADS_DIR "${CMAKE_BINARY_DIR}/downloads")

if (WIN32)
    function (download_dependency target_zip url)
        if (NOT EXISTS "${DOWNLOADS_DIR}/${target_zip}")
            message("Downloading ${target_zip}...")
            file(DOWNLOAD "${url}" "${DOWNLOADS_DIR}/${target_zip}" STATUS result)
            message("Download result: ${result}")
        endif()
    endfunction()

    function(extract_dependency target_zip path_check dest_dir )
        if (NOT EXISTS "${DOWNLOADS_DIR}/${path_check}")
            message("Extracting ${dest_dir}...")
            execute_process(COMMAND powershell Expand-Archive "\"${DOWNLOADS_DIR}/${target_zip}\"" "\"${DOWNLOADS_DIR}/${dest_dir}\" -Force")
        endif()
    endfunction()

    # Download GDAL/CURL/sqlite3/spatialite/PDAL and supporting libraries
    download_dependency("gdal_libs.zip" "https://github.com/DroneDB/DroneDB/releases/download/v0.9.13/release-1928-x64-gdal-3-3-2-mapserver-7-6-4-libs.zip")
    download_dependency("gdal_bins.zip" "https://github.com/DroneDB/DroneDB/releases/download/v0.9.13/release-1928-x64-gdal-3-3-2-mapserver-7-6-4-sans-sqlite3.zip")

    extract_dependency("gdal_libs.zip" "gdal/include" "gdal")
    extract_dependency("gdal_bins.zip" "gdal/bin" "gdal")

    set(GDAL_LIBRARY "${DOWNLOADS_DIR}/gdal/lib/gdal_i.lib")
    set(GDAL_INCLUDE_DIR "${DOWNLOADS_DIR}/gdal/include")

    # Fix: auto_ptr is not a member of std
    add_compile_definitions(_HAS_AUTO_PTR_ETC=1)
endif()

find_package(GDAL REQUIRED)
find_package(OpenMP REQUIRED)

if(OpenMP_CXX_FOUND)
    target_link_libraries(${PROJECT_NAME} PUBLIC OpenMP::OpenMP_CXX ${GDAL_LIBRARY})
else()
    target_link_libraries(${PROJECT_NAME} ${GDAL_LIBRARY})
endif()

if (NOT WIN32 AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PUBLIC stdc++fs)
endif()

target_include_directories(${PROJECT_NAME} PUBLIC
                           "${PROJECT_BINARY_DIR}"
                           )

include_directories(${GDAL_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/vendor)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
