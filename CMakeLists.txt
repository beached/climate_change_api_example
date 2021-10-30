cmake_minimum_required(VERSION 3.14)
project(climate_change_api_example)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(extern)

set(Boost_NO_WARN_NEW_VERSIONS ON)
find_package(Boost REQUIRED COMPONENTS system thread)
find_path(CROW_INCLUDE_DIRS "crow.h")
find_package(PkgConfig REQUIRED)

#pkg_search_module(GUMBO REQUIRED gumbo)

include_directories(SYSTEM ${GUMBO_INCLUDE_DIRS})

# VCPKG has no cmake or pkg config files for gumbo
find_path(GUMBO_INCLUDE_DIRS "gumbo.h")
find_library(GUMBO_LIBRARY_FILE gumbo )

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
        daw::parallel
        Boost::boost
        ${CROW_LIBRARIES}
        ${GUMBO_LIBRARY_FILE}
)

install(TARGETS ${PROJECT_NAME}
        EXPORT ${PROJECT_NAME}_Targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
