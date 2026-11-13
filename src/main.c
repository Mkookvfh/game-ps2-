/*
 * Mueang Isara - PlayStation 2 entry point
 *
 * This is a minimal bootable scaffold used to verify the PS2SDK build.
 * It does not yet implement graphics, input, gameplay, or audio.
 */

#include <tamtypes.h>
#include <kernel.h>

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    /* Keep the EE program alive while the game systems are added. */
    for (;;) {
        asm volatile("nop");
    }

    return 0;
}
