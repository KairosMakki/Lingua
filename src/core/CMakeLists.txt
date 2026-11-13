add_library(lingua_core
    byte_reader.cpp
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