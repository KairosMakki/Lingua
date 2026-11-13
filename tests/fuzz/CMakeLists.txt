if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    return()
endif()

add_executable(fuzz_pe fuzz_pe.cpp)
target_link_libraries(fuzz_pe PRIVATE lingua_format_pe)
target_compile_options(fuzz_pe PRIVATE -fsanitize=fuzzer,address)
target_link_options(fuzz_pe PRIVATE -fsanitize=fuzzer,address)

add_executable(fuzz_elf fuzz_elf.cpp)
target_link_libraries(fuzz_elf PRIVATE lingua_format_elf)
target_compile_options(fuzz_elf PRIVATE -fsanitize=fuzzer,address)
target_link_options(fuzz_elf PRIVATE -fsanitize=fuzzer,address)

add_executable(fuzz_deb fuzz_deb.cpp)
target_link_libraries(fuzz_deb PRIVATE lingua_format_deb)
target_compile_options(fuzz_deb PRIVATE -fsanitize=fuzzer,address)
target_link_options(fuzz_deb PRIVATE -fsanitize=fuzzer,address)

add_executable(fuzz_appimage fuzz_appimage.cpp)
target_link_libraries(fuzz_appimage PRIVATE lingua_format_appimage)
target_compile_options(fuzz_appimage PRIVATE -fsanitize=fuzzer,address)
target_link_options(fuzz_appimage PRIVATE -fsanitize=fuzzer,address)

add_executable(fuzz_macho fuzz_macho.cpp)
target_link_libraries(fuzz_macho PRIVATE lingua_format_macho)
target_compile_options(fuzz_macho PRIVATE -fsanitize=fuzzer,address)
target_link_options(fuzz_macho PRIVATE -fsanitize=fuzzer,address)

add_executable(fuzz_rpm fuzz_rpm.cpp)
target_link_libraries(fuzz_rpm PRIVATE lingua_format_rpm)
target_compile_options(fuzz_rpm PRIVATE -fsanitize=fuzzer,address)
target_link_options(fuzz_rpm PRIVATE -fsanitize=fuzzer,address)

add_executable(fuzz_apk fuzz_apk.cpp)
target_link_libraries(fuzz_apk PRIVATE lingua_format_apk)
target_compile_options(fuzz_apk PRIVATE -fsanitize=fuzzer,address)
target_link_options(fuzz_apk PRIVATE -fsanitize=fuzzer,address)