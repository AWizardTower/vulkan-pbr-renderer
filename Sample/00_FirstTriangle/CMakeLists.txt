cmake_minimum_required(VERSION 3.22)

add_executable(00_FirstTriangle
        ${SHADER_HEADERS}
        Main.cpp
)
target_link_libraries(00_FirstTriangle PRIVATE adiosy_core)
add_dependencies(00_FirstTriangle adiosy_sample_shaders)
