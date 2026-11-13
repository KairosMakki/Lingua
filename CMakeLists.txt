cmake_minimum_required(VERSION 3.20)

project(Lingua VERSION 0.1.0 LANGUAGES CXX
    HOMEPAGE_URL https://github.com/KairosMakki/Lingua
    DESCRIPTION "Modular C++ library for reading, inspecting, and importing cross-platform application formats")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LINGUA_BUILD_TESTS "Build tests" ON)
option(LINGUA_BUILD_CLI "Build CLI" ON)
option(LINGUA_BUILD_WEB "Build web interface (optional)" OFF)
option(LINGUA_BUILD_FUZZ "Build fuzz targets" OFF)
option(LINGUA_BUILD_SHARED "Build shared libraries" OFF)

if(LINGUA_BUILD_SHARED)
    set(BUILD_SHARED_LIBS ON)
endif()

if(BUILD_SHARED_LIBS)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

include(GNUInstallDirs)

add_subdirectory(src)

if(LINGUA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Install export
install(EXPORT LinguaTargets
    FILE LinguaTargets.cmake
    NAMESPACE lingua::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Lingua
)

include(CMakePackageConfigHelpers)
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/LinguaConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/LinguaConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Lingua
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/LinguaConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/LinguaConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/LinguaConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Lingua
)

# pkg-config
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/lingua.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/lingua.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/lingua.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)