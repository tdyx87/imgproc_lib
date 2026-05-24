enable_testing()

# Each test file has its own main(), build as separate executables
macro(add_test_exe name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE imgproc_lib)
    add_test(NAME ${name} COMMAND ${name})
endmacro()

add_test_exe(test_image_codec test_image_codec.cpp)
add_test_exe(test_qrcode test_qrcode.cpp)
add_test_exe(test_qrcode_generator test_qrcode_generator.cpp)
add_test_exe(test_text_renderer test_text_renderer.cpp)
add_test_exe(test_compression test_compression.cpp)
add_test_exe(test_barcode test_barcode.cpp)
