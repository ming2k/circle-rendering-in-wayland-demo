# Low-Level Circle Rendering Demonstration in Wayland

A simple demonstration program that draws a circle using low-level Wayland protocols. This project serves as an educational example of how to interact directly with the Wayland display server protocol.

## Features

- Creates a resizable window using XDG shell protocol
- Renders a red circle on a black background
- Handles window resize events dynamically
- Uses shared memory buffers for rendering
- Implements proper Wayland resource cleanup
- Responds to window close and Ctrl+C events

## Prerequisites

To build and run this program, you need:

- CMake (>= 3.10)
- C compiler with C11 support
- Wayland development libraries
- wayland-protocols package
- pkg-config

On Ubuntu/Debian, install dependencies with:
```bash
sudo apt install cmake build-essential libwayland-dev wayland-protocols pkg-config
```

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

## Running

After building, you can run the program from the build directory:
```bash
./wayland-circle
```

The program will:
- Open a 400x400 window
- Display a red circle on a black background
- Respond to window resize events
- Close gracefully when you press Ctrl+C or close the window

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

## Project Structure

```
wayland-circle/
├── CMakeLists.txt          # CMake build configuration
├── src/
│   └── wayland-circle-xdg.c # Main source code
├── protocols/              # Generated Wayland protocol files
│   ├── .gitkeep
│   ├── xdg-shell-client-protocol.h
│   └── xdg-shell-protocol.c
└── README.md              # This file
```

## Technical Details

- Uses the XDG shell protocol for window management
- Implements shared memory buffer rendering
- Handles Wayland events and protocols directly
- Demonstrates proper resource management and cleanup
- Shows how to create and manage Wayland surfaces

## Notes

- The program only works on Wayland compositors - it will not run on X11
- The window is resizable, and the circle will scale proportionally
- All graphics are rendered using shared memory buffers (no GPU acceleration)

## License

This project is open source and available under the MIT License.
