#include "scenes.h"
#include "sh1106.h"
#include "sh1106_host.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define SH1106_PAGES (SH1106_HEIGHT / 8)

static void write_pgm(const char *path, const uint8_t *snap)
{
    FILE *f = fopen(path, "wb");
    if (!f) {
        perror(path);
        exit(1);
    }
    fprintf(f, "P5\n%d %d\n255\n", SH1106_WIDTH, SH1106_HEIGHT);
    for (int y = 0; y < SH1106_HEIGHT; y++) {
        for (int x = 0; x < SH1106_WIDTH; x++) {
            int     page = y / 8;
            int     bit  = y % 8;
            int     lit  = (snap[page * SH1106_WIDTH + x] >> bit) & 1;
            uint8_t v    = lit ? 0xFF : 0x00;
            fwrite(&v, 1, 1, f);
        }
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    const char *outdir = (argc > 1) ? argv[1] : "docs/img";

    sh1106_init();

    for (int i = 0; i < scenes_count; i++) {
        sh1106_host_release(); // clear any leftover capture lock
        scenes[i].setup();

        char path[512];
        snprintf(path, sizeof(path), "%s/%s.pgm", outdir, scenes[i].name);
        write_pgm(path, sh1106_host_snapshot());
        printf("wrote %s\n", path);
    }
    return 0;
}
