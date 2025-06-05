# Tungsten

Tungsten is a statically typed compiled language inspired by C++ and Java
> [!WARNING]  
> the language is still under development and everything is subject to change

# Building

## Prerequisites

- cmake 3.28 or newer
- git

> [!IMPORTANT]  
> this project uses C++ modules, which are currently not supported by all compilers and CMake generators, so make sure
> to use both a compiler and a CMake generator that support modules

<ins> **1. Cloning the repo:** </ins>

Start by cloning the repo with `git clone https://github.com/rickisgone/tungsten`  
after doing this follow the instruction for the targeted OS

<details><summary><big>WINDOWS</big></summary><p>

  <ins> **2. Compiling the project:** </ins>  

  ```bash
  mkdir build
  cmake -S . -B build
  cmake --build build --config Release
  ```

</details>

<details><summary><big>LINUX</big></summary><p>  

   <ins> **2. Compiling the project:** </ins>

*the default compiler and CMake generator on linux (gcc and Make) don't support modules, so I'll be using Clang and
Ninja in the example below*

  ```bash
  mkdir build
  cmake -S . -B build -GNinja -DCMAKE_CXX_COMPILER=clang++
  cmake --build build --config Release
  ```

</details><p>  

# Syntax

## Variables declaration

Types follow the ***PascalCase*** convention, they are:

| Type     | Alignment (Bytes) |
|----------|-------------------|
| `Void`   | N/A               |
| `Int`    | 4                 |
| `Float`  | 4                 |
| `Double` | 8                 |
| `Char`   | 2                 |
| `Bool`   | 1                 |
| `String` | N/A               |
| `Int8`   | 1                 |
| `Int16`  | 2                 |
| `Int32`  | 4                 |
| `Int64`  | 8                 |
| `Uint8`  | 1                 |
| `Uint16` | 2                 |
| `Uint32` | 4                 |
| `Uint64` | 8                 |

### Stack allocation

```c++
Int myVariable = 247;
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

Just like in C++ and java, functions are declared like this:

```c++
Int myFunction() {
    return 247;
}
```

Like in java you can return arrays by doing:

```c++
Int[] myfunction() {
    Int[3] array{2, 4, 7};
    return array;
}
```

Or you can use a C++ like approach and use a pointer:

```c++
Int* myFunction() {
    Int* array = new Int[3]{2, 4, 7};
    return array;
}
```

### Main Function

The `main` function can return either `Void` or `Int`. Command line arguments are passed as a `String` array.  
If the function is declared as `Int` and no return value is provided, `0` will be returned by default.

```c++
Int main(String[] args) {
    /* your code */
    return 0;
}

Void main(String[] args) {
    /* your code */
}
```

If you don't need the command-line arguments, you can simply omit them:

```c++
Int main() {
    /* your code */
    return 0;
}
```
