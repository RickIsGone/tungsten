cmake_minimum_required(VERSION 3.28)
project(tungsten VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
file(GLOB_RECURSE SRC "src/*.cpp")
add_executable(tungsten ${SRC})

if (CMAKE_BUILD_TYPE MATCHES Debug)
    add_definitions(-DTUNGSTEN_DEBUG)
endif ()

if (EMSCRIPTEN)
    set_target_properties(tungsten PROPERTIES
            LINK_FLAGS "--bind -sEXPORTED_RUNTIME_METHODS=['FS','FS_writeFile','FS_readFile','callMain'] -sEXCEPTION_CATCHING_ALLOWED='1'"
            OUTPUT_NAME "tungsten"
    )
endif ()

file(GLOB_RECURSE MODULES "src/*.cppm")
target_sources(tungsten PUBLIC FILE_SET tungsten_modules TYPE CXX_MODULES FILES
        ${MODULES}
)