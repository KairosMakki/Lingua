add_library(lingua_detect
    detector.cpp
)
add_library(lingua::detect ALIAS lingua_detect)
target_link_libraries(lingua_detect PUBLIC lingua_core)
target_include_directories(lingua_detect
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_compile_features(lingua_detect PUBLIC cxx_std_20)
install(TARGETS lingua_detect EXPORT LinguaTargets ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin INCLUDES DESTINATION include)