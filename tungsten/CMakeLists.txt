cmake_minimum_required(VERSION 3.28)
project(tungsten_compiler VERSION 0.1.0)

# === Compiler Settings ===
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# === Required Packages ===
find_package(LLVM REQUIRED CONFIG)
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
find_file(AddLLVM_CMAKE_FILE AddLLVM.cmake)
include(HandleLLVMOptions)
include(AddLLVM)

# === CMake Options ===
if (CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)
    add_definitions(-DTUNGSTEN_DEBUG)
    if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
        #        add_compile_options(-fsanitize=address)
    endif ()
endif ()

# === Source Files ===
add_llvm_executable(tungsten src/main.cpp)

# === Compile options ===
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if (MSVC)
        add_compile_options(/GR-)
    else ()
        add_compile_options(-frtti)
    endif ()
endif ()

# === LLVM ===
target_include_directories(tungsten PRIVATE ${LLVM_INCLUDE_DIRS})
message(STATUS "found LLVM at: ${LLVM_DIR}")
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    file(READ "/etc/os-release" OS_RELEASE_CONTENT)
endif ()
if (OS_RELEASE_CONTENT MATCHES "ID=arch") # pacman package only contains libLLVM.a
    target_link_libraries(tungsten PRIVATE LLVM)
else ()
    llvm_map_components_to_libnames(LLVM_LIBS
            support
            core
            irreader
            native
            nativecodegen
            orcjit
    )
    message(STATUS "found LLVM libs: ${LLVM_LIBS}")
    target_link_libraries(tungsten PRIVATE ${LLVM_LIBS})
endif ()
add_definitions(${LLVM_DEFINITIONS})

# === Emscripten ===
#if (EMSCRIPTEN)
#    find_package(ZLIB REQUIRED)
#    target_link_libraries(tungsten PRIVATE ZLIB::ZLIB)
#    set_target_properties(tungsten PROPERTIES
#            LINK_FLAGS "--bind -sEXPORTED_RUNTIME_METHODS=['FS','FS_writeFile','FS_readFile','callMain'] -sEXCEPTION_CATCHING_ALLOWED='1'"
#            OUTPUT_NAME "tungsten"
#    )
#endif ()

# === Modules ===
file(GLOB_RECURSE MODULES "src/*.cppm")
target_sources(tungsten PUBLIC FILE_SET tungsten_modules TYPE CXX_MODULES FILES
        ${MODULES}
)

# === Output Structure ===
set(TUNGSTEN_OUTPUT_DIR ${CMAKE_BINARY_DIR}/tungsten/tungsten)
file(MAKE_DIRECTORY ${TUNGSTEN_OUTPUT_DIR})
file(MAKE_DIRECTORY ${TUNGSTEN_OUTPUT_DIR}/bin)
file(MAKE_DIRECTORY ${TUNGSTEN_OUTPUT_DIR}/lib)
file(MAKE_DIRECTORY ${TUNGSTEN_OUTPUT_DIR}/include)
set_target_properties(tungsten PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${TUNGSTEN_OUTPUT_DIR}/bin
)

# === Compile Core And Stdlib ===
# === Stdlib ===
file(GLOB STDLIB "src/stdlib/*.cpp")
add_library(stdlib STATIC ${STDLIB})
message(STATUS "adding ${STDLIB} to the stdlib")
set_target_properties(stdlib PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY ${TUNGSTEN_OUTPUT_DIR}/lib
)
# === Core ===
file(GLOB CORE "src/core/*.cpp")
add_library(core STATIC ${CORE})
set_target_properties(core PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY ${TUNGSTEN_OUTPUT_DIR}/lib
)
add_dependencies(tungsten stdlib core) # to make cmake produce the .lib/.a

# === Copy stdlib modules ===
file(GLOB STDLIB_MODULES "src/stdlib/*.tgs")
foreach (LIB IN LISTS STDLIB_MODULES)
    get_filename_component(LIBNAME ${LIB} NAME)
    message(STATUS "copying lib: ${LIBNAME}")
    file(COPY_FILE ${LIB} ${TUNGSTEN_OUTPUT_DIR}/include/${LIBNAME})
endforeach ()