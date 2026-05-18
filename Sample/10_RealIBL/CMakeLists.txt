cmake_minimum_required(VERSION 3.22)

add_executable(10_RealIBL
        ${SHADER_HEADERS}
        Main.cpp
)
target_link_libraries(10_RealIBL PRIVATE adiosy_core)
add_dependencies(10_RealIBL adiosy_sample_shaders)
