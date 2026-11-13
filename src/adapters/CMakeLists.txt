add_library(lingua_adapters
    adapter.cpp
)
add_library(lingua::adapters ALIAS lingua_adapters)
target_link_libraries(lingua_adapters PUBLIC lingua_core)
target_include_directories(lingua_adapters
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_compile_features(lingua_adapters PUBLIC cxx_std_20)
install(TARGETS lingua_adapters EXPORT LinguaTargets ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin INCLUDES DESTINATION include)