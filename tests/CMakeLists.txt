find_package(Catch2 3 REQUIRED CONFIG)

add_executable(lingua_tests
    core/test_byte_reader.cpp
    core/test_mapped_file.cpp
    detect/test_detect.cpp
    format/pe/test_pe.cpp
    format/elf/test_elf.cpp
    format/deb/test_deb.cpp
    format/appimage/test_appimage.cpp
    import/test_import.cpp
)
target_link_libraries(lingua_tests PRIVATE
    lingua lingua_import lingua_adapters Catch2::Catch2WithMain)
target_include_directories(lingua_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_features(lingua_tests PRIVATE cxx_std_20)

include(CTest)
add_test(NAME lingua_tests COMMAND lingua_tests)

if(LINGUA_BUILD_FUZZ)
    add_subdirectory(fuzz)
endif()