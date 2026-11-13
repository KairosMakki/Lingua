add_library(lingua_core
    archive.cpp
    byte_reader.cpp
    hash.cpp
    error.cpp
    format_registry.cpp
    logger.cpp
    mapped_file.cpp
)
add_library(lingua::core ALIAS lingua_core)

target_include_directories(lingua_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

find_package(ZLIB QUIET)
find_package(zstd CONFIG QUIET)
if(ZLIB_FOUND)
    target_link_libraries(lingua_core PRIVATE ZLIB::ZLIB)
    target_compile_definitions(lingua_core PRIVATE LINGUA_HAVE_ZLIB)
endif()
if(zstd_FOUND)
    if(TARGET zstd::libzstd)
        target_link_libraries(lingua_core PRIVATE zstd::libzstd)
    elseif(TARGET zstd::libzstd_shared)
        target_link_libraries(lingua_core PRIVATE zstd::libzstd_shared)
    elseif(TARGET zstd::libzstd_static)
        target_link_libraries(lingua_core PRIVATE zstd::libzstd_static)
    endif()
    target_compile_definitions(lingua_core PRIVATE LINGUA_HAVE_ZSTD)
endif()

target_compile_features(lingua_core PUBLIC cxx_std_20)

if(BUILD_SHARED_LIBS)
    target_compile_definitions(lingua_core PRIVATE LINGUA_CORE_EXPORTS)
endif()

install(TARGETS lingua_core EXPORT LinguaTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)