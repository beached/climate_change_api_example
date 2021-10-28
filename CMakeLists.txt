cmake_minimum_required(VERSION 3.20)
project(climate_change_api_example)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(extern)

find_path(CROW_INCLUDE_DIRS "crow.h")
find_package(PkgConfig REQUIRED)

pkg_search_module(GUMBO REQUIRED gumbo)
include_directories(SYSTEM ${GUMBO_INCLUDE_DIRS})
link_directories(${GUMBO_LIBRARY_DIRS})

add_executable(${PROJECT_NAME} main.cpp)
target_include_directories(
        ${PROJECT_NAME}
        PRIVATE
        ${CROW_INCLUDE_DIRS}
)
target_link_libraries(
        ${PROJECT_NAME}
        PRIVATE
        daw::daw-json-link
        daw::daw-header-libraries
        daw::daw-curl-wrapper
        daw::daw-utf-range
        ${CROW_LIBRARIES}
        ${GUMBO_LIBRARIES}
)
