add_library(lingua_format_apk
    apk_parser.cpp
)
add_library(lingua::apk ALIAS lingua_format_apk)
target_link_libraries(lingua_format_apk PUBLIC lingua_core lingua_detect)
target_include_directories(lingua_format_apk
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_compile_features(lingua_format_apk PUBLIC cxx_std_20)
install(TARGETS lingua_format_apk EXPORT LinguaTargets ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin INCLUDES DESTINATION include)