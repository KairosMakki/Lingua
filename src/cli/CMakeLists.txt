add_executable(lingua-cli
    main.cpp
)
target_link_libraries(lingua-cli PRIVATE lingua lingua_import lingua_adapters)
target_include_directories(lingua-cli PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_features(lingua-cli PRIVATE cxx_std_20)
set_target_properties(lingua-cli PROPERTIES OUTPUT_NAME lingua)
install(TARGETS lingua-cli RUNTIME DESTINATION bin)