cmake_minimum_required(VERSION 3.22)

add_executable(06_DeferredGBuffer
        ${SHADER_HEADERS}
        Main.cpp
)
target_link_libraries(06_DeferredGBuffer PRIVATE adiosy_core)
add_dependencies(06_DeferredGBuffer adiosy_sample_shaders)
