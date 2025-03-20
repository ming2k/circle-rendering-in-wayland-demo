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

## Building and Running

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

4. Run the program from the build directory:
```bash
./wayland-circle
```

The program will:
- Open a 400x400 window
- Display a red circle on a black background
- Respond to window resize events
- Close gracefully when you press Ctrl+C or close the window

5. Cleaning up:
   - To remove generated files from the build directory:
     ```bash
     make clean
     ```
   - For a complete clean (removing all CMake-generated files):
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

### Rendering Process

The rendering in this program follows these key steps:

1. **Shared Memory Buffers**
   - The client (our program) allocates a shared memory buffer using the Wayland SHM (Shared Memory) protocol
   - This buffer acts as a canvas where we can draw pixels directly
   - The buffer uses the ARGB8888 format, meaning each pixel is represented by 4 bytes (Alpha, Red, Green, Blue)

2. **Client-Side Drawing**
   - All actual drawing is performed by the client application, not by Wayland
   - The program directly writes pixel data into the shared memory buffer
   - The circle is drawn using basic mathematics: for each pixel, we calculate its distance from the center
   - If the distance is less than the radius, we color that pixel red (otherwise it remains black)
   - This is pure CPU-based rendering without any GPU acceleration

3. **Buffer Sharing**
   - Once drawing is complete, the client notifies the Wayland compositor
   - The compositor receives access to the shared memory buffer
   - No actual pixel data is copied between processes - the memory is shared

4. **Compositor's Role**
   - The Wayland compositor (like Weston, Mutter, or KWin) is responsible for:
     - Reading our shared buffer
     - Compositing it with other windows
     - Handling transparency and effects
     - Finally displaying the result on screen

5. **Window Management**
   - The XDG shell protocol handles window management
   - It manages window decorations, resizing, and positioning
   - When the window is resized, the program:
     - Creates a new shared memory buffer of the appropriate size
     - Redraws the circle at the new dimensions
     - Shares the new buffer with the compositor

This architecture demonstrates Wayland's core principle of "every frame is perfect" - the client has complete control over its pixels, and the compositor ensures smooth, tear-free presentation.

### Performance Considerations

- This implementation uses software rendering, which means all drawing operations are CPU-bound
- For more complex graphics, you might want to consider:
  - Using OpenGL/EGL for GPU-accelerated rendering
  - Using a graphics library like Cairo
  - Implementing hardware acceleration through Vulkan or OpenGL

## Notes

- The program only works on Wayland compositors - it will not run on X11
- The window is resizable, and the circle will scale proportionally
- All graphics are rendered using shared memory buffers (no GPU acceleration)

## License

This project is open source and available under the MIT License.
