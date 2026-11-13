add_library(lingua
    api.cpp
)
add_library(lingua::lingua ALIAS lingua)
target_link_libraries(lingua
    PUBLIC
        lingua_core
        lingua_detect
        lingua_format_pe
        lingua_format_elf
        lingua_format_deb
        lingua_format_appimage
        lingua_adapters
        lingua_import
)
target_include_directories(lingua
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_compile_features(lingua PUBLIC cxx_std_20)
install(TARGETS lingua EXPORT LinguaTargets ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin INCLUDES DESTINATION include)