#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-client.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <errno.h>
#include <signal.h>

// Include generated XDG Shell protocol
#include "xdg-shell-client-protocol.h"

// Add this near the top of the file, after the includes
#define UNUSED(x) (void)(x)

// Wayland global objects
struct {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_seat *seat;
    struct wl_shm *shm;
    struct wl_surface *surface;
    struct xdg_wm_base *xdg_wm_base;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;
    struct wl_buffer *buffer;
    int width, height;
    int running;
    uint32_t last_serial;
} wayland;

// Shared memory buffer
struct {
    void *data;
    size_t size;
} shm_data;

// Signal handler
void signal_handler(int signum) {
    UNUSED(signum);
    wayland.running = 0;
}

// ARGB format color
uint32_t color_to_argb(uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
    return (a << 24) | (r << 16) | (g << 8) | b;
}

// Create shared memory file
static int create_shm_file(size_t size) {
    int fd;
    
    // Try using memfd_create (requires Linux 3.17+)
    #ifdef __linux__
    fd = memfd_create("wayland_shm", 0);
    if (fd >= 0) {
        goto done;
    } else if (errno != ENOSYS) {
        perror("memfd_create failed");
        return -1;
    }
    #endif
    
    // Fallback: create temporary file in /tmp
    char template[] = "/tmp/wayland-XXXXXX";
    fd = mkstemp(template);
    if (fd >= 0) {
        // Unlink file immediately so it's automatically deleted when closed
        unlink(template);
    } else {
        perror("mkstemp failed");
        return -1;
    }
    
done:
    if (ftruncate(fd, size) < 0) {
        perror("ftruncate failed");
        close(fd);
        return -1;
    }

    return fd;
}

// Create shared memory buffer
static struct wl_buffer* create_buffer(int width, int height) {
    int stride = width * 4; // 4 bytes/pixel (ARGB)
    size_t size = stride * height;
    
    int fd = create_shm_file(size);
    if (fd < 0) {
        return NULL;
    }

    shm_data.data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm_data.data == MAP_FAILED) {
        perror("mmap failed");
        close(fd);
        return NULL;
    }
    
    shm_data.size = size;

    struct wl_shm_pool *pool = wl_shm_create_pool(wayland.shm, fd, size);
    if (!pool) {
        fprintf(stderr, "Cannot create shared memory pool\n");
        close(fd);
        return NULL;
    }
    
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, WL_SHM_FORMAT_ARGB8888);
    if (!buffer) {
        fprintf(stderr, "Cannot create buffer\n");
        wl_shm_pool_destroy(pool);
        close(fd);
        return NULL;
    }
    
    wl_shm_pool_destroy(pool);
    close(fd);
    
    return buffer;
}

// Draw circle
static void draw_circle(void *data, int width, int height) {
    uint32_t *pixels = data;
    int radius = width < height ? width / 3 : height / 3;
    int center_x = width / 2;
    int center_y = height / 2;
    
    // Clear buffer to black background
    memset(pixels, 0, width * height * 4);
    
    // Draw circle
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Calculate distance from point to center
            float dist = sqrtf(powf(x - center_x, 2) + powf(y - center_y, 2));
            
            // If point is inside circle, set to red
            if (dist <= radius) {
                pixels[y * width + x] = color_to_argb(255, 255, 0, 0); // Red
            }
        }
    }
}

// xdg_wm_base events
static void xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial) {
    UNUSED(data);
    printf("Received ping event (serial: %u)\n", serial);
    xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping
};

// xdg_surface events
static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial) {
    UNUSED(data);
    printf("Received surface configure event (serial: %u)\n", serial);
    wayland.last_serial = serial;
    xdg_surface_ack_configure(xdg_surface, serial);
    
    // Only draw when we have a valid buffer and non-zero dimensions
    if (wayland.buffer && wayland.width > 0 && wayland.height > 0) {
        printf("Configuration valid, drawing: %dx%d\n", wayland.width, wayland.height);
        draw_circle(shm_data.data, wayland.width, wayland.height);
        wl_surface_attach(wayland.surface, wayland.buffer, 0, 0);
        wl_surface_damage(wayland.surface, 0, 0, wayland.width, wayland.height);
        wl_surface_commit(wayland.surface);
    } else {
        printf("Skipping draw, waiting for valid dimensions\n");
    }
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure
};

// xdg_toplevel events
static void xdg_toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel,
                                 int32_t width, int32_t height,
                                 struct wl_array *states) {
    UNUSED(data);
    UNUSED(xdg_toplevel);
    UNUSED(states);
    printf("Received toplevel configure event: width=%d, height=%d\n", width, height);
    
    // If compositor sends 0 dimensions, use our default/current dimensions
    if (width == 0 || height == 0) {
        if (wayland.width > 0 && wayland.height > 0) {
            printf("Using current dimensions: %dx%d\n", wayland.width, wayland.height);
            return; // Keep current dimensions
        } else {
            printf("Using default dimensions: 400x400\n");
            width = 400;
            height = 400;
        }
    }
    
    // Only recreate buffer if dimensions change
    if (width != wayland.width || height != wayland.height) {
        printf("Updating dimensions to %dx%d\n", width, height);
        wayland.width = width;
        wayland.height = height;
        
        if (wayland.buffer) {
            wl_buffer_destroy(wayland.buffer);
            wayland.buffer = NULL;
        }
        
        wayland.buffer = create_buffer(width, height);
        if (!wayland.buffer) {
            fprintf(stderr, "Cannot create buffer for new dimensions\n");
        } else {
            printf("Successfully created buffer for new dimensions\n");
        }
    }
}

static void xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel) {
    UNUSED(data);
    UNUSED(xdg_toplevel);
    printf("Received close event\n");
    wayland.running = 0;
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = xdg_toplevel_configure,
    .close = xdg_toplevel_close,
};

// Registry handler callbacks
static void registry_handle_global(void *data, struct wl_registry *registry,
                                 uint32_t id, const char *interface, uint32_t version) {
    UNUSED(data);
    printf("Found interface: %s (version %u)\n", interface, version);
    
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        wayland.compositor = wl_registry_bind(registry, id, &wl_compositor_interface, 1);
        printf("Bound to compositor: %p\n", (void*)wayland.compositor);
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        wayland.xdg_wm_base = wl_registry_bind(registry, id, &xdg_wm_base_interface, 1);
        printf("Bound to xdg_wm_base: %p\n", (void*)wayland.xdg_wm_base);
        xdg_wm_base_add_listener(wayland.xdg_wm_base, &xdg_wm_base_listener, NULL);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        wayland.shm = wl_registry_bind(registry, id, &wl_shm_interface, 1);
        printf("Bound to shm: %p\n", (void*)wayland.shm);
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        wayland.seat = wl_registry_bind(registry, id, &wl_seat_interface, 1);
        printf("Bound to seat: %p\n", (void*)wayland.seat);
    }
}

static void registry_handle_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
    UNUSED(data);
    UNUSED(registry);
    printf("Global object removed: %u\n", name);
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_handle_global,
    .global_remove = registry_handle_global_remove,
};

// Initialize Wayland connection
static int init_wayland(void) {
    printf("Connecting to Wayland server...\n");
    wayland.display = wl_display_connect(NULL);
    if (!wayland.display) {
        fprintf(stderr, "Cannot connect to Wayland display server\n");
        return -1;
    }
    printf("Connected to Wayland server\n");
    
    wayland.registry = wl_display_get_registry(wayland.display);
    if (!wayland.registry) {
        fprintf(stderr, "Cannot get registry\n");
        return -1;
    }
    
    wl_registry_add_listener(wayland.registry, &registry_listener, NULL);
    printf("Added registry listener, waiting for events...\n");
    
    // Sync and wait for server events - do twice to ensure we get all interfaces
    wl_display_roundtrip(wayland.display);
    wl_display_roundtrip(wayland.display);
    
    if (!wayland.compositor) {
        fprintf(stderr, "Cannot get compositor interface\n");
        return -1;
    }
    
    if (!wayland.xdg_wm_base) {
        fprintf(stderr, "Cannot get xdg_wm_base interface\n");
        return -1;
    }
    
    if (!wayland.shm) {
        fprintf(stderr, "Cannot get shm interface\n");
        return -1;
    }
    
    printf("Successfully initialized all required Wayland interfaces\n");
    return 0;
}

// Create surface
static int create_surface(int width, int height) {
    printf("Creating %dx%d surface...\n", width, height);
    wayland.width = width;
    wayland.height = height;
    
    wayland.surface = wl_compositor_create_surface(wayland.compositor);
    if (!wayland.surface) {
        fprintf(stderr, "Cannot create surface\n");
        return -1;
    }
    printf("Surface created successfully\n");
    
    // Create XDG surface
    wayland.xdg_surface = xdg_wm_base_get_xdg_surface(wayland.xdg_wm_base, wayland.surface);
    if (!wayland.xdg_surface) {
        fprintf(stderr, "Cannot create xdg_surface\n");
        return -1;
    }
    printf("xdg_surface created successfully\n");
    
    xdg_surface_add_listener(wayland.xdg_surface, &xdg_surface_listener, NULL);
    
    // Create XDG toplevel
    wayland.xdg_toplevel = xdg_surface_get_toplevel(wayland.xdg_surface);
    if (!wayland.xdg_toplevel) {
        fprintf(stderr, "Cannot create xdg_toplevel\n");
        return -1;
    }
    printf("xdg_toplevel created successfully\n");
    
    xdg_toplevel_add_listener(wayland.xdg_toplevel, &xdg_toplevel_listener, NULL);
    xdg_toplevel_set_title(wayland.xdg_toplevel, "Wayland Low-Level Circle Rendering");
    
    // Set minimum size first to avoid compositor sending 0 dimensions
    xdg_toplevel_set_min_size(wayland.xdg_toplevel, width, height);
    
    // Commit surface to get configuration
    wl_surface_commit(wayland.surface);
    
    // Wait for surface configuration - do multiple roundtrips to ensure we get all events
    wl_display_roundtrip(wayland.display);
    wl_display_roundtrip(wayland.display);
    
    // Create initial buffer - use new dimensions if we received them from configure event
    int buffer_width = wayland.width > 0 ? wayland.width : width;
    int buffer_height = wayland.height > 0 ? wayland.height : height;
    
    if (!wayland.buffer) {  // Might have been created in configure event
        wayland.buffer = create_buffer(buffer_width, buffer_height);
        if (!wayland.buffer) {
            fprintf(stderr, "Cannot create buffer\n");
            return -1;
        }
        printf("Buffer created successfully: %dx%d\n", buffer_width, buffer_height);
    }
    
    // Initial draw
    printf("Drawing initial content...\n");
    draw_circle(shm_data.data, buffer_width, buffer_height);
    
    // Attach buffer and commit
    wl_surface_attach(wayland.surface, wayland.buffer, 0, 0);
    wl_surface_damage(wayland.surface, 0, 0, buffer_width, buffer_height);
    wl_surface_commit(wayland.surface);
    printf("Initial content committed\n");
    
    return 0;
}

int main(int argc, char **argv) {
    UNUSED(argc);
    UNUSED(argv);
    
    printf("Wayland Low-Level Circle Rendering Program Starting\n");
    
    memset(&wayland, 0, sizeof(wayland));
    memset(&shm_data, 0, sizeof(shm_data));
    
    wayland.running = 1;
    
    if (init_wayland() < 0) {
        fprintf(stderr, "Wayland initialization failed\n");
        return 1;
    }
    
    if (create_surface(400, 400) < 0) {
        fprintf(stderr, "Surface creation failed\n");
        return 1;
    }
    
    printf("Entering main loop - Press Ctrl+C to exit\n");
    // Main loop - wait for exit signal
    while (wayland.running && wl_display_dispatch(wayland.display) != -1) {
        // Keep running until error or user termination
    }
    
    printf("Cleaning up resources...\n");
    // Cleanup resources
    if (wayland.buffer) {
        wl_buffer_destroy(wayland.buffer);
    }
    
    if (wayland.xdg_toplevel) {
        xdg_toplevel_destroy(wayland.xdg_toplevel);
    }
    
    if (wayland.xdg_surface) {
        xdg_surface_destroy(wayland.xdg_surface);
    }
    
    if (wayland.surface) {
        wl_surface_destroy(wayland.surface);
    }
    
    if (wayland.xdg_wm_base) {
        xdg_wm_base_destroy(wayland.xdg_wm_base);
    }
    
    if (wayland.seat) {
        wl_seat_destroy(wayland.seat);
    }
    
    if (wayland.compositor) {
        wl_compositor_destroy(wayland.compositor);
    }
    
    if (wayland.shm) {
        wl_shm_destroy(wayland.shm);
    }
    
    if (wayland.registry) {
        wl_registry_destroy(wayland.registry);
    }
    
    if (wayland.display) {
        wl_display_disconnect(wayland.display);
    }
    
    if (shm_data.data) {
        munmap(shm_data.data, shm_data.size);
    }
    
    printf("Program exited gracefully\n");
    return 0;
}
