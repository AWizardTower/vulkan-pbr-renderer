cmake_minimum_required(VERSION 3.22)

add_executable(11_GLTFPBR
        ${SHADER_HEADERS}
        Main.cpp
)
target_link_libraries(11_GLTFPBR PRIVATE adiosy_core)
add_dependencies(11_GLTFPBR adiosy_sample_shaders)
