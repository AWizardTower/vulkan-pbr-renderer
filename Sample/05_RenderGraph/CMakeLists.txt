cmake_minimum_required(VERSION 3.22)

add_executable(05_RenderGraph
        ${SHADER_HEADERS}
        Main.cpp
)
target_link_libraries(05_RenderGraph PRIVATE adiosy_core)
add_dependencies(05_RenderGraph adiosy_sample_shaders)
