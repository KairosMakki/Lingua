add_library(lingua_web
    server.cpp
)
add_library(lingua::web ALIAS lingua_web)
target_link_libraries(lingua_web PUBLIC lingua_core lingua_import lingua_adapters)
target_include_directories(lingua_web
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_compile_features(lingua_web PUBLIC cxx_std_20)

# Optional openssl
find_package(OpenSSL QUIET)
if(OpenSSL_FOUND)
    target_link_libraries(lingua_web PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    target_compile_definitions(lingua_web PRIVATE LINGUA_HAVE_OPENSSL)
endif()

# zlib? no

if(WIN32)
    target_link_libraries(lingua_web PRIVATE ws2_32)
endif()

install(TARGETS lingua_web EXPORT LinguaTargets ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin INCLUDES DESTINATION include)