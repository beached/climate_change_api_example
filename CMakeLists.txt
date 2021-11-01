cmake_minimum_required(VERSION 3.14)
project(climate_change_api_example)

set(CMAKE_CXX_STANDARD 17)
set(Boost_NO_WARN_NEW_VERSIONS ON)

add_subdirectory(extern)

find_package(Boost REQUIRED COMPONENTS system thread)
find_path(CROW_INCLUDE_DIRS "crow.h")
include_directories( include/)
add_executable(${PROJECT_NAME} src/main.cpp)
target_include_directories(
        ${PROJECT_NAME}
        PRIVATE
        ${CROW_INCLUDE_DIRS}
)
target_link_directories( ${PROJECT_NAME} PRIVATE ${GUMBO_LIBRARY_DIRS} )
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
        daw::daw-gumbo-pp
)

install(TARGETS ${PROJECT_NAME}
        EXPORT ${PROJECT_NAME}_Targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
