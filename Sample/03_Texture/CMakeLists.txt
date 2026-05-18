cmake_minimum_required(VERSION 3.22)

add_executable(03_Texture
        ${SHADER_HEADERS}
        Main.cpp
)
target_link_libraries(03_Texture PRIVATE adiosy_core)
add_dependencies(03_Texture adiosy_sample_shaders)
