cmake_minimum_required(VERSION 3.28)
project(tungsten)

set(CMAKE_CXX_STANDARD 20)

add_executable(tungsten main.cpp)
file(GLOB MODULES "*.cppm")
target_sources(tungsten PUBLIC FILE_SET tungsten_modules TYPE CXX_MODULES FILES
        ${MODULES}
)