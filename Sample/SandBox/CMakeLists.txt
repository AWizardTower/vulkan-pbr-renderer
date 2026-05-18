cmake_minimum_required(VERSION 3.22)

add_executable(SandBox
        ${SHADER_HEADERS}
        Main.cpp
        AdEventTesting.cpp
)
target_link_libraries(SandBox PRIVATE adiosy_core)
add_dependencies(SandBox adiosy_sample_shaders)
