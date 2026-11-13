add_library(lingua_import
    import.cpp
)
add_library(lingua::import ALIAS lingua_import)
target_link_libraries(lingua_import PUBLIC lingua_core lingua_adapters)
target_include_directories(lingua_import
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_compile_features(lingua_import PUBLIC cxx_std_20)
install(TARGETS lingua_import EXPORT LinguaTargets ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin INCLUDES DESTINATION include)