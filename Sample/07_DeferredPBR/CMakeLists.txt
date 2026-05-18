cmake_minimum_required(VERSION 3.22)

add_executable(07_DeferredPBR
        ${SHADER_HEADERS}
        Main.cpp
)
target_link_libraries(07_DeferredPBR PRIVATE adiosy_core)
add_dependencies(07_DeferredPBR adiosy_sample_shaders)
