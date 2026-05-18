cmake_minimum_required(VERSION 3.22)

add_executable(08_ShadowMapPCF
        ${SHADER_HEADERS}
        Main.cpp
)
target_link_libraries(08_ShadowMapPCF PRIVATE adiosy_core)
add_dependencies(08_ShadowMapPCF adiosy_sample_shaders)
