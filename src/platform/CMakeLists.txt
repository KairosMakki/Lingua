add_library(lingua_platform
    platform.cpp
)
add_library(lingua::platform ALIAS lingua_platform)
target_link_libraries(lingua_platform PUBLIC lingua_core)
target_include_directories(lingua_platform
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_compile_features(lingua_platform PUBLIC cxx_std_20)
install(TARGETS lingua_platform EXPORT LinguaTargets ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin INCLUDES DESTINATION include)