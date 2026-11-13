add_library(lingua_format_deb
    deb_parser.cpp
)
add_library(lingua::deb ALIAS lingua_format_deb)
target_link_libraries(lingua_format_deb PUBLIC lingua_core lingua_detect)
target_include_directories(lingua_format_deb
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_compile_features(lingua_format_deb PUBLIC cxx_std_20)
install(TARGETS lingua_format_deb EXPORT LinguaTargets ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin INCLUDES DESTINATION include)