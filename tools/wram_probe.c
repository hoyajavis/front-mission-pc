#include "boot_loader.h"
#include "replay.h"
#include "snes.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void step(Snes *snes, uint16_t btn) {
    snes->input1->currentState = btn;
    snes_runFrame(snes);
}

static void tap(Snes *snes, uint16_t btn, int hold, int wait) {
    for (int i = 0; i < hold; ++i) step(snes, btn);
    for (int i = 0; i < wait; ++i) step(snes, FM_BTN_NONE);
}

static void find_monotonic_axis(const char *label,
                                const uint8_t *w0, const uint8_t *w1,
                                const uint8_t *w2, const uint8_t *w3, const uint8_t *w4) {
    printf("=== Monotonic Axis Candidates: %s ===\n", label);
    for (uint32_t i = 0; i < 0x20000; ++i) {
        if (i >= 0x1D00 && i <= 0x1FFF) continue;
        if (i >= 0x0DC0 && i <= 0x0DDE) continue;
        int d1 = (int)w1[i] - (int)w0[i];
        int d2 = (int)w2[i] - (int)w1[i];
        int d3 = (int)w3[i] - (int)w2[i];
        int d4 = (int)w4[i] - (int)w3[i];
        if ((d1 > 0 && d2 > 0 && d3 < 0 && d4 < 0) ||
            (d1 < 0 && d2 < 0 && d3 > 0 && d4 > 0)) {
            printf("  $%02X:%04X: %02X -> %02X -> %02X -> %02X -> %02X (d=%+d,%+d,%+d,%+d)\n",
                   0x7Eu + (i >> 16), i & 0xFFFFu, w0[i], w1[i], w2[i], w3[i], w4[i], d1, d2, d3, d4);
        }
    }
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <rom_path> <patches_dir> <trace.fmrec>\n", argv[0]);
        return 1;
    }

    FmReplayTrace *trace = fm_replay_load(argv[3]);
    if (!trace) return 1;

    FmBootConfig cfg = {0};
    cfg.rom_path = argv[1];
    cfg.patches_dir = argv[2];
    cfg.profile = (FmBootProfileId)trace->header.profile_id;
    cfg.recalculate_checksum = true;

    FmBootResult res;
    if (!fm_boot_load_rom(&cfg, &res)) {
        fm_replay_free(trace);
        return 1;
    }

    Snes *snes = snes_init();
    if (!snes || !snes_loadRom(snes, res.rom_data, (int)res.rom_size)) {
        return 1;
    }
    snes_setPixelFormat(snes, pixelFormatXRGB);

    /* Seek to end of trace (or optional frame arg) */
    uint32_t target = trace->header.frame_count;
    if (argc >= 5) {
        uint32_t req = (uint32_t)strtoul(argv[4], NULL, 10);
        if (req < target) target = req;
    }
    for (uint32_t f = 0; f < target; ++f) {
        step(snes, fm_replay_get_frame(trace, f));
    }
    for (int i = 0; i < 400; ++i) step(snes, FM_BTN_NONE);

    uint8_t *w0 = (uint8_t *)malloc(0x20000);
    uint8_t *w1 = (uint8_t *)malloc(0x20000);
    uint8_t *w2 = (uint8_t *)malloc(0x20000);
    uint8_t *w3 = (uint8_t *)malloc(0x20000);
    uint8_t *w4 = (uint8_t *)malloc(0x20000);

    /* X Axis: RIGHT, RIGHT, LEFT, LEFT */
    memcpy(w0, snes->ram, 0x20000);
    tap(snes, FM_BTN_RIGHT, 4, 25);
    memcpy(w1, snes->ram, 0x20000);
    tap(snes, FM_BTN_RIGHT, 4, 25);
    memcpy(w2, snes->ram, 0x20000);
    tap(snes, FM_BTN_LEFT, 4, 25);
    memcpy(w3, snes->ram, 0x20000);
    tap(snes, FM_BTN_LEFT, 4, 25);
    memcpy(w4, snes->ram, 0x20000);
    find_monotonic_axis("Tactical X (R, R, L, L)", w0, w1, w2, w3, w4);

    /* Y Axis: DOWN, DOWN, UP, UP */
    memcpy(w0, snes->ram, 0x20000);
    tap(snes, FM_BTN_DOWN, 4, 25);
    memcpy(w1, snes->ram, 0x20000);
    tap(snes, FM_BTN_DOWN, 4, 25);
    memcpy(w2, snes->ram, 0x20000);
    tap(snes, FM_BTN_UP, 4, 25);
    memcpy(w3, snes->ram, 0x20000);
    tap(snes, FM_BTN_UP, 4, 25);
    memcpy(w4, snes->ram, 0x20000);
    find_monotonic_axis("Tactical Y (D, D, U, U)", w0, w1, w2, w3, w4);

    /* Dump $7E:0290..$7E:02CF before and after 25x RIGHT pan */
    printf("=== Cursor/Camera block $7E:0290..$7E:02CF BEFORE pan ===\n");
    for (uint32_t a = 0x0290; a < 0x02D0; a += 2) {
        uint16_t val = (uint16_t)(snes->ram[a] | (snes->ram[a + 1] << 8));
        printf("  $7E:%04X = %04X (%d)\n", a, val, (int16_t)val);
    }

    memcpy(w0, snes->ram, 0x20000);
    for (int i = 0; i < 25; ++i) tap(snes, FM_BTN_RIGHT, 2, 6);
    memcpy(w1, snes->ram, 0x20000);
    printf("=== Cursor/Camera block $7E:0290..$7E:02CF AFTER 25x RIGHT ===\n");
    for (uint32_t a = 0x0290; a < 0x02D0; a += 2) {
        uint16_t val = (uint16_t)(snes->ram[a] | (snes->ram[a + 1] << 8));
        uint16_t oldv = (uint16_t)(w0[a] | (w0[a + 1] << 8));
        if (val != oldv) {
            printf("  $7E:%04X: %04X -> %04X (%+d)\n", a, oldv, val, (int16_t)val - (int16_t)oldv);
        }
    }
    printf("=== Scroll mirrors $7E:0D70..$7E:0D94 AFTER 25x RIGHT ===\n");
    for (uint32_t a = 0x0D70; a <= 0x0D94; a += 2) {
        uint16_t val = (uint16_t)(snes->ram[a] | (snes->ram[a + 1] << 8));
        uint16_t oldv = (uint16_t)(w0[a] | (w0[a + 1] << 8));
        if (val != oldv) {
            printf("  $7E:%04X: %04X -> %04X (%+d)\n", a, oldv, val, (int16_t)val - (int16_t)oldv);
        }
    }
    printf("=== Other 16-bit changes in $7E:0000..$7E:1000 on 25x RIGHT ===\n");
    for (uint32_t a = 0x0000; a < 0x1000; a += 2) {
        if (a >= 0x0DC0 && a <= 0x0DDE) continue;
        uint16_t val = (uint16_t)(snes->ram[a] | (snes->ram[a + 1] << 8));
        uint16_t oldv = (uint16_t)(w0[a] | (w0[a + 1] << 8));
        if (val != oldv && ((int16_t)val - (int16_t)oldv) > 16 && ((int16_t)val - (int16_t)oldv) < 400) {
            printf("  $7E:%04X: %04X -> %04X (%+d)\n", a, oldv, val, (int16_t)val - (int16_t)oldv);
        }
    }

    printf("=== Unit/Inventory sample $7E:D580..$7E:D620 ===\n");
    for (uint32_t a = 0xD580; a < 0xD620; a += 16) {
        printf("  $7E:%04X:", a);
        for (int j = 0; j < 16; ++j) printf(" %02X", snes->ram[a + j]);
        printf("\n");
    }

    free(w0); free(w1); free(w2); free(w3); free(w4);
    snes_free(snes);
    fm_boot_free_result(&res);
    fm_replay_free(trace);
    return 0;
}
