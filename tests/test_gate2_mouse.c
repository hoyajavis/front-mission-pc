#include "fm_wram.h"
#include "input.h"
#include "replay.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_wram_overlay_layout(void) {
    FmWramOverlay *ov = (FmWramOverlay *)calloc(1, sizeof(FmWramOverlay));
    assert(ov != NULL);
    assert(sizeof(*ov) == FM_WRAM_SIZE);

    ov->mapped.cursor_screen_x = 0x0048u;
    ov->mapped.cursor_screen_y = 0x0090u;
    assert(fm_wram_rd16(ov->raw, FM_WRAM_CURSOR_SCREEN_X) == 0x0048u);
    assert(fm_wram_rd16(ov->raw, FM_WRAM_CURSOR_SCREEN_Y) == 0x0090u);

    ov->mapped.menu_cursor_idx = 0x03u;
    ov->mapped.menu_cursor_mirror = 0x03u;
    assert(ov->raw[FM_WRAM_MENU_CURSOR_IDX] == 0x03u);
    assert(ov->raw[FM_WRAM_MENU_CURSOR_MIR] == 0x03u);

    free(ov);
}

static void test_window_to_snes_mapping(void) {
    int sx = -1, sy = -1;

    /* 16:9 (1600x896 = 2x 800x448): left wing is [0..287], center SNES is [288..1311] */
    assert(!fm_input_window_to_snes(100, 200, 1600, 896, true, &sx, &sy));
    assert(sx == 0);

    assert(fm_input_window_to_snes(288, 0, 1600, 896, true, &sx, &sy));
    assert(sx == 0 && sy == 0);

    assert(fm_input_window_to_snes(800, 448, 1600, 896, true, &sx, &sy));
    assert(sx == 256 && sy == 224);

    assert(!fm_input_window_to_snes(1400, 448, 1600, 896, true, &sx, &sy));
    assert(sx == 511);

    /* 4:3 Classic letterboxed/pillarboxed in 1280x720 */
    assert(fm_input_window_to_snes(640, 360, 1280, 720, false, &sx, &sy));
    assert(sx == 256 && sy == 224);
}

static void test_isometric_projection(void) {
    int gx = 0, gy = 0;
    int sx = 0, sy = 0;

    for (int ty = -4; ty <= 10; ++ty) {
        for (int tx = -4; tx <= 10; ++tx) {
            fm_input_iso_tile_to_screen(tx, ty, 32, 16, &sx, &sy);
            fm_input_screen_to_iso_tile(sx, sy, 32, 16, &gx, &gy);
            assert(gx == tx && gy == ty);
        }
    }
}

static void test_pillar1_tactical_and_pan(void) {
    FmWramOverlay *ov = (FmWramOverlay *)calloc(1, sizeof(FmWramOverlay));
    assert(ov != NULL);
    ov->mapped.bg1_scroll_x = 100;
    ov->mapped.bg1_scroll_y = 200;

    FmMouseState mouse;
    fm_input_init(&mouse);

    /* Move pointer to center of 800x448 (snes 256, 224 -> native 128, 112) */
    fm_input_on_mouse_move(&mouse, 400, 224, 800, 448, true);
    uint16_t joy = 0;
    assert(fm_input_apply_pre_frame(&mouse, FM_INPUT_MODE_TACTICAL, NULL, ov->raw, &joy));
    assert(ov->mapped.cursor_screen_x == 128);
    assert(ov->mapped.cursor_screen_y == 112);
    assert(joy == 0);

    /* Right-drag by (-20, -10) in 800x448 -> pans camera by (+10, +5) */
    fm_input_on_mouse_button(&mouse, 3, true, 400, 224, 800, 448, true);
    fm_input_on_mouse_move(&mouse, 380, 214, 800, 448, true);
    assert(fm_input_apply_pre_frame(&mouse, FM_INPUT_MODE_TACTICAL, NULL, ov->raw, &joy));
    assert(ov->mapped.bg1_scroll_x == 110);
    assert(ov->mapped.bg1_scroll_y == 205);
    assert(ov->mapped.bg2_scroll_x == 110);
    assert(ov->mapped.bg3_scroll_y == 205);
    assert(ov->mapped.bg_scroll_shadow[0] == 110);
    assert(ov->mapped.bg_scroll_shadow[1] == 205);

    /* Releasing right button after drag must NOT trigger B cancel */
    fm_input_on_mouse_button(&mouse, 3, false, 380, 214, 800, 448, true);
    joy = 0;
    assert(!fm_input_apply_pre_frame(&mouse, FM_INPUT_MODE_TACTICAL, NULL, ov->raw, &joy));
    assert(joy == 0);

    free(ov);
}

static void test_pillar2_menu_hit_testing(void) {
    FmMenuLayout layout = {
        .x = 64,
        .y = 80,
        .row_w = 120,
        .row_h = 24,
        .rows = 4,
        .cols = 2,
        .col_stride = 140
    };

    int idx = -1;
    assert(!fm_input_menu_hit_test(&layout, 60, 80, &idx));
    assert(fm_input_menu_hit_test(&layout, 64, 80, &idx) && idx == 0);
    assert(fm_input_menu_hit_test(&layout, 204, 80, &idx) && idx == 1);
    assert(!fm_input_menu_hit_test(&layout, 190, 80, &idx)); /* gap between columns */
    assert(fm_input_menu_hit_test(&layout, 210, 128, &idx) && idx == 5); /* row 2, col 1 */

    FmWramOverlay *ov = (FmWramOverlay *)calloc(1, sizeof(FmWramOverlay));
    assert(ov != NULL);
    FmMouseState mouse;
    fm_input_init(&mouse);

    /* Left-click on row 2, col 1 (snes 210, 128 -> win 144+210=354, 128 in 800x448) */
    fm_input_on_mouse_button(&mouse, 1, true, 354, 128, 800, 448, true);
    uint16_t joy = 0;
    assert(fm_input_apply_pre_frame(&mouse, FM_INPUT_MODE_MENU, &layout, ov->raw, &joy));
    assert(ov->mapped.menu_cursor_idx == 5);
    assert(ov->mapped.menu_cursor_mirror == 5);
    assert((joy & FM_BTN_A) != 0);

    /* Right-click without drag injects FM_BTN_B */
    fm_input_on_mouse_button(&mouse, 3, true, 354, 128, 800, 448, true);
    fm_input_on_mouse_button(&mouse, 3, false, 354, 128, 800, 448, true);
    joy = 0;
    assert(fm_input_apply_pre_frame(&mouse, FM_INPUT_MODE_MENU, &layout, ov->raw, &joy));
    assert((joy & FM_BTN_B) != 0);

    free(ov);
}

static void test_pillar3_zero_invasion_idle(void) {
    uint8_t *wram = (uint8_t *)malloc(FM_WRAM_SIZE);
    uint8_t *snap = (uint8_t *)malloc(FM_WRAM_SIZE);
    assert(wram != NULL && snap != NULL);
    for (size_t i = 0; i < FM_WRAM_SIZE; ++i) {
        wram[i] = (uint8_t)((i * 73u + 19u) & 0xFFu);
    }
    memcpy(snap, wram, FM_WRAM_SIZE);

    FmMouseState mouse;
    fm_input_init(&mouse);

    /* Stationary mouse: repeated moves to same coordinate must yield 0-byte mutation */
    fm_input_on_mouse_move(&mouse, 0, 0, 800, 448, true);
    uint16_t joy = FM_BTN_START;
    assert(!fm_input_apply_pre_frame(&mouse, FM_INPUT_MODE_TACTICAL, NULL, wram, &joy));
    assert(joy == FM_BTN_START);
    assert(memcmp(wram, snap, FM_WRAM_SIZE) == 0);

    free(wram);
    free(snap);
}

int main(void) {
    test_wram_overlay_layout();
    test_window_to_snes_mapping();
    test_isometric_projection();
    test_pillar1_tactical_and_pan();
    test_pillar2_menu_hit_testing();
    test_pillar3_zero_invasion_idle();
    printf("[PASS] Gate2MouseTest: all 6 test suites passed (0-byte idle drift)\n");
    return 0;
}
