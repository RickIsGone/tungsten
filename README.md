# The Tungsten programming language

<p align="center">
  <img src="tungsten logo.png" alt="the tungsten logo" width="300" />
</p>

## Introduction

*Tungsten* is a statically-typed multi-paradigm compiled programming language made with the LLVM framework and inspired
by C++ and Java

## Example

Below is a snippet of code which shows some of the basic functionalities of tungsten

```c++
import stdio;
Uint fibonacci(Uint num) {
    if (num <= 1) return num;
    return fibonacci(num - 1) + fibonacci(num - 2);
}

Int main(String[] args) {
    for (Uint8 i = 0; i < 10; ++i) {
        print("${}, ", fibonacci(i));
    }
    return 0;
}
```

> [!WARNING]
> the language is still under development, and everything is subject to change

# Trying the language

if you want to try *Tungsten* before installing the compiler, feel free to do so on
the [online tungsten compiler](https://rickisgone.github.io/tungsten-sandbox/)

# Building

| Platform | Build Status                                                                                                                                                                             |
|----------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Ubuntu   | [![Ubuntu Build](https://github.com/rickisgone/tungsten/actions/workflows/Ubuntu%20build.yml/badge.svg)](https://github.com/rickisgone/tungsten/actions/workflows/Ubuntu%20build.yml)    |
| Windows  | [![Windows Build](https://github.com/rickisgone/tungsten/actions/workflows/Windows%20Build.yml/badge.svg)](https://github.com/rickisgone/tungsten/actions/workflows/Windows%20Build.yml) |
| Mac      | **not added yet**                                                                                                                                                                        |

## Prerequisites

Global

- cmake 3.28 or newer
- git

Compiler

- LLVM

Package Manager

- Libcurl
- libarchive

If you are on windows i'd suggest using [vcpkg](https://github.com/microsoft/vcpkg) to install the dependencies

> [!IMPORTANT]
> this project uses C++ modules, which are currently not supported by all compilers and CMake generators, so make sure
> to use both a compiler and a CMake generator that supports modules

<ins> **1. Cloning the repo:** </ins>

Start by cloning the repo with `git clone https://github.com/rickisgone/tungsten`
after doing this follow the instruction for the targeted OS

<details><summary><big>WINDOWS</big></summary><p>

<ins> **2. Compiling the project:** </ins>  

if you're using vcpkg:

```bash
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE="VCPKG_DIR\scripts\buildsystems\vcpkg.cmake"  -DVCPKG_TARGET_TRIPLET=x64-windows-static -DLLVM_DIR='VCPKG_DIR/installed/x64-windows-static/share/llvm/' -Dzstd_DIR='VCPKG_DIR/installed/x64-windows-static/share/zstd'
cmake --build build --config Release
```

replace `VCPKG_DIR` with the vcpkg directory
</details>

<details><summary><big>LINUX</big></summary><p>

<ins> **2. Compiling the project:** </ins>

*the default compiler and CMake generator on linux (gcc and Make) don't support modules, so I'll be using Clang and
Ninja in the example below*

```bash
cmake -S . -B build -GNinja -DCMAKE_CXX_COMPILER=clang++
cmake --build build --config Release
```

</details><p>

# Syntax

## Variables declaration

Types follow the ***PascalCase*** convention, they are:

| Type      | Alignment (Bytes) |
|-----------|-------------------|
| `Void`    | N/A               |
| `Int`     | 4                 |
| `Uint`    | 4                 |
| `Float`   | 4                 |
| `Double`  | 8                 |
| `Char`    | 2                 |
| `Bool`    | 1                 |
| `String`  | N/A               |
| `Int8`    | 1                 |
| `Int16`   | 2                 |
| `Int32`   | 4                 |
| `Int64`   | 8                 |
| `Int128`  | 16                |
| `Uint8`   | 1                 |
| `Uint16`  | 2                 |
| `Uint32`  | 4                 |
| `Uint64`  | 8                 |
| `Uint128` | 16                |

### Stack allocation

```c++
Int myVariable = 247;
Int myVariable{247};
```

### Heap allocation

```c++
Int* myPointer = new Int{247};
```

To free the pointer:

```c++
free myPointer;
```

## Arrays

### Stack allocation

```c++
Int[10] myArray;
```

### Heap allocation

```c++
Int* myArray = new Int[10];
```

To free the array:

```c++
free[] myArray;
```

## Functions

Just like in C++, functions are declared like this:

```c++
Int myFunction() {
    return 247;
}
```

You can return arrays by doing:

```c++
Int[] myfunction() {
    Int[3] array{2, 4, 7};
    return array;
}
```

Or you can allocate on the heap and use a pointer:

```c++
Int* myFunction() {
    Int* array = new Int[3]{2, 4, 7};
    return array;
}
```

### Main Function

The `main` function can return either `Int` or `Int32`. Command line arguments are passed as a `String` array.
If no return value is provided, `0` will be returned by default.

```c++
Int main(String[] args) {
    /* your code */
    return 0;
}
```

If you don't need the command-line arguments, you can simply omit them:

```c++
Int main() {
    /* your code */
    return 0;
}
```

## Building a project

To build a project, you need to make a `build.tgs` file  
A simple project's build file will look something like this:

```c++
import build;

Project myProject{Executable, "myProject"};

Void main() {
    myProject.addSource("main.tgs");
    myProject.build();
}
```

to then build the file, you'll then have to call the compiler in the directory where the build file is located

```shell
cd projectDirectory
tungsten 
```

The compiler will then automatically compile the `build.tgs` file.  
After compiling the file, the compiler will automatically execute the `build` executable which will compile the project
following the rules set by the `build.tgs` file.  
The build output will then be put into the `projectDirectory/build` directory

# TPKG

tpkg is the package manager included with the *Tungsten programming language*

> [!NOTE]
> tpkg uses a centralized git directory, which is not checked, meaning it's on the users to open issues reporting
> malicious packages

package files are written in **json**, and a simple package file looks like this

```json
{
  "name": "myPackage",
  "version": "1.0.0",
  "description": "my package description",
  "homepage": "homepage url",
  "license": "MIT/Apache 2.0/ecc.",
  "source": "source/release code zip url",
  "dependencies": [
    "your dependencies"
  ],
  "build": {
    "type": "cmd/buildsystem",
    "args": "args to pass to the compiler"
  }
}
```

# License

This project is licensed under the Apache License 2.0. See the [LICENSE](LICENSE.txt) file for details.
