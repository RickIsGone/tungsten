# The Tungsten programming language

<p align="center">
  <img src="tungsten logo.png" alt="the tungsten logo" width="300" />
</p>

## Introduction

*Tungsten* is a statically-typed multi-paradigm compiled programming language made with the LLVM framework and inspired
by C++ and Java

## Example

Below is a snippet of code which shows some of the basic functionalities of tungsten

<!-- ```c++
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
``` -->

```c++
Num fibonacci(Num num) {
   if (num <= 1) 
      return num;
   return fibonacci(num - 1) + fibonacci(num - 2);
}

Num main() {
   for (Num i = 0; i < 10; ++i) {
      print("fibonacci(%.0lf) = %.0lf\n", i, fibonacci(i));
   }
   return CodeSuccess;
}
```

> [!WARNING]
> the language is still under development, and everything is subject to change

# Trying the language

if you want to try *Tungsten* before installing the compiler, feel free to do so on
the [online tungsten compiler](https://rickisgone.hackclub.app)

# VSCode Extension

You can install the VSCode extension for *Tungsten* from
the [VSCode marketplace](https://marketplace.visualstudio.com/items?itemName=RickIsGone.tungsten)
or by searching for `tungsten` in the extensions tab of VSCode.
You'll need to have the compiler installed on your system for the extension to work properly.

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

Here I'm using vcpkg:

```bash
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE='VCPKG_DIR\scripts\buildsystems\vcpkg.cmake'  -DVCPKG_TARGET_TRIPLET=x64-windows-static -DLLVM_DIR='VCPKG_DIR/installed/x64-windows-static/share/llvm/' -Dzstd_DIR='VCPKG_DIR/installed/x64-windows-static/share/zstd'
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

<!-- | Type      | Alignment (Bytes) |
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
| `Uint128` | 16                | -->

| Type     | Alignment (Bytes) |
|----------|-------------------|
| `Void`   | N/A               |
| `Char`   | 2                 |
| `Bool`   | 1                 |
| `String` | N/A               |
| `Num`    | 8                 |

> [!IMPORTANT]
> `Num` is a `double` so to use functions like `print` and `input` you need to use the `%lf` format
> specifier

Tungsten has also a variadic type called `ArgPack` which is just like `...` in C

### Stack allocation

```c++
Num myVariable = 247;
Num myVariable{247};
```

<!-- ### Heap allocation

```c++
Int* myPointer = new Int{247};
```

To free the pointer:

```c++
free myPointer;
```

> [!WARNING]  
> Tungsten doesn't have a garbage collector so you have to manually free the pointers

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
``` -->

## Control flow statements

### If

If statements are written just like in C++

```cpp
if (condition) {
    // do stuff
} else {
    // do other stuff
}
```

And just like in C++ you can avoid using brackets for single instructions

```cpp
if (condition)
    // do stuff
else 
    // do other stuff
```

### While

Unlike in C++, while statements require brackets to work

```cpp
while (condition) {
    // do stuff

}
```

### Do while

Do whiles instead are just like in C++

```cpp
do {
    // do stuff
} while(condition);
```

### For

For statements just like while statements require brackets to work

```Java
for(Num i = 0; i < 10; ++i) {
    // do stuff
}
```

## Functions

Just like in C++, functions are declared like this:

```c++
Num myFunction() {
    return 247;
}
```

<!-- You can return arrays by doing:

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
``` -->

### Extern Functions

There are two types of extern functions

- C functions
- tungsten functions

You can import C functions with

```cpp
extern "C" Void myCFun();
```

and tungsten functions with

```cpp
extern Void myFun();
```

### Main Function

The `main` function can return either `Int` or `Int32`. Command line arguments are passed as a `String` array.
If no return value is provided, `0` will be returned by default.

```c++
Num main() {
    /* your code */
    return CodeSuccess;
}
```

### Integrated Core Functions

Tungsten has a reduced number of integrated core functions  

| Type       | Name                 | Arguments              | functionality                                 |
|------------|----------------------|------------------------|-----------------------------------------------|
| `Num`      | shell                | (String cmd)           | same function as `system()` in C              |
| `Void`     | print                | (String fmt, ArgPack)  | same function as `printf()` in C              |
| `Void`     | input                | (String fmt, ArgPack)  | same function as `scanf()` in C               |  
| `String`   | __builtinFile        | no arguments           | returns the name of the file                  |
| `String`   | __builtinFunction    | no arguments           | returns the name of the function              |
| `Num`      | __builtinColumn      | no arguments           | returns the number of the column              |
| `Num`      | __builtinLine        | no arguments           | returns the number of the line                |
| `String`   | nameof               | (any variable)         | returns the name of the variable              |
| `String`   | typeof               | (any variable)         | returns the type of the variable              |
| `Num`      | sizeof               | (any variable)         | returns the size of the type of the variable  |



<!-- 
If you don't need the command-line arguments, you can simply omit them:

```c++
Int main() {
    /* your code */
    return 0;
}
``` -->

## Building a project

To build a project, you can either compile via cmd by calling the `tungsten` compiler or you can make a `build.tgs` file
and use the integrated buildsystem

### Compiling via cmd

> [!IMPORTANT]
> Tungsten currently uses *clang* to compile the generated llvm ir to an executable, so you need to have it installed
> for the compiler to work

```shell
cd projectDirectory
tungsten <file> [flags]
```

### Compiling via buildsystem

> [!IMPORTANT]
> Not added yet

A simple project's build file will look something like this:

```c++
import build;

Project myProject{Executable, "myProject"};

Int main() {
    myProject.addSource("main.tgs");
    myProject.build();
}
```

To build the file, you'll then have to call the compiler in the directory where the build file is located

```shell
cd projectDirectory
tungsten tgs-build [flags] 
```

The compiler will then automatically compile the `build.tgs` file.  
After compiling the file, the compiler will automatically execute the `build` executable which will compile the project
following the rules set by the `build.tgs` file.  
The build output will then be put into the `projectDirectory/build` directory

<!-- # TPKG

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
    "args": [
      "myargs"
    ]
  }
}
``` -->

# License

This project is licensed under the Apache License 2.0. See the [LICENSE](LICENSE.txt) file for details.
