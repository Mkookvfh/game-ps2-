
#include <tamtypes.h>
#include <graph.h>
#include <gs_psm.h>

int main(void)
{
    int framebuffer;

    // Allocate framebuffer
    framebuffer = graph_vram_allocate(
        640,
        448,
        GS_PSM_32,
        GRAPH_ALIGN_PAGE
    );

    // Initialize GS directly
    graph_initialize(
        framebuffer,
        640,
        448,
        GS_PSM_32,
        0,
        0
    );

    graph_set_bgcolor(255, 0, 0);
    graph_enable_output();

    // Test four colors
    while (1)
    {
        graph_set_bgcolor(255, 0, 0);
        graph_wait_vsync();
        graph_wait_vsync();

        graph_set_bgcolor(0, 255, 0);
        graph_wait_vsync();
        graph_wait_vsync();

        graph_set_bgcolor(0, 0, 255);
        graph_wait_vsync();
        graph_wait_vsync();

        graph_set_bgcolor(255, 255, 255);
        graph_wait_vsync();
        graph_wait_vsync();
    }

    return 0;
}
