cmake_minimum_required(VERSION 3.22)

add_executable(09_IBL
        ${SHADER_HEADERS}
        Main.cpp
)
target_link_libraries(09_IBL PRIVATE adiosy_core)
add_dependencies(09_IBL adiosy_sample_shaders)
