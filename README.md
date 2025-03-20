# Wayland Circle

A simple demonstration program that draws a circle using low-level Wayland protocols.

## Building

1. Create a build directory and navigate to it:
```bash
mkdir build
cd build
```

2. Configure the project with CMake:
```bash
cmake ..
```

3. Build the program:
```bash
make
```

## Cleaning

To clean the last build, you have several options:

1. From the build directory, remove all generated files:
```bash
make clean
```

2. For a complete clean (removing all CMake-generated files), delete the build directory:
```bash
# From the project root
rm -rf build/
```

This second method is recommended when you want to start fresh, as it removes all cached CMake files and build artifacts.
