#include "input.h"
#include "fm_wram.h"
#include "replay.h"

#include <math.h>
#include <string.h>

#define FM_DRAG_THRESHOLD_PX 6

void fm_input_init(FmMouseState *state) {
    if (!state) return;
    memset(state, 0, sizeof(*state));
}

static int clamp_int(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

bool fm_input_window_to_snes(int win_x, int win_y,
                             int win_w, int win_h,
                             bool widescreen_16_9,
                             int *out_snes_x, int *out_snes_y) {
    if (win_w <= 0 || win_h <= 0) {
        if (out_snes_x) *out_snes_x = 0;
        if (out_snes_y) *out_snes_y = 0;
        return false;
    }

    double virt_w = widescreen_16_9 ? (double)FM_WIDESCREEN_W : (double)FM_SNES_PPU_W;
    double virt_h = (double)FM_SNES_PPU_H;

    double sx_scale = (double)win_w / virt_w;
    double sy_scale = (double)win_h / virt_h;
    double scale = (sx_scale < sy_scale) ? sx_scale : sy_scale;
    if (scale <= 0.0) {
        if (out_snes_x) *out_snes_x = 0;
        if (out_snes_y) *out_snes_y = 0;
        return false;
    }

    double vp_w = virt_w * scale;
    double vp_h = virt_h * scale;
    double vp_x = ((double)win_w - vp_w) * 0.5;
    double vp_y = ((double)win_h - vp_h) * 0.5;

    int vx = (int)floor(((double)win_x - vp_x) / scale);
    int vy = (int)floor(((double)win_y - vp_y) / scale);

    int snes_x = widescreen_16_9 ? (vx - FM_WIDESCREEN_WING) : vx;
    int snes_y = vy;

    bool inside = (snes_x >= 0 && snes_x < FM_SNES_PPU_W &&
                   snes_y >= 0 && snes_y < FM_SNES_PPU_H);

    if (out_snes_x) *out_snes_x = clamp_int(snes_x, 0, FM_SNES_PPU_W - 1);
    if (out_snes_y) *out_snes_y = clamp_int(snes_y, 0, FM_SNES_PPU_H - 1);
    return inside;
}

static int floor_div_pow2(int num, int shift) {
    if (num >= 0) {
        return num >> shift;
    }
    return -(((-num) + (1 << shift) - 1) >> shift);
}

void fm_input_screen_to_iso_tile(int snes_x, int snes_y,
                                 int cam_x, int cam_y,
                                 int *out_gx, int *out_gy) {
    int wx = (snes_x >> 1) + cam_x;
    int wy = (snes_y >> 1) + cam_y;

    int gx = floor_div_pow2(wx + 2 * wy + 16, 5);
    int gy = floor_div_pow2(2 * wy - wx + 16, 5);

    if (out_gx) *out_gx = gx;
    if (out_gy) *out_gy = gy;
}

void fm_input_iso_tile_to_screen(int gx, int gy,
                                 int cam_x, int cam_y,
                                 int *out_snes_x, int *out_snes_y) {
    int wx = (gx - gy) * 16;
    int wy = (gx + gy) * 8;
    if (out_snes_x) *out_snes_x = (wx - cam_x) * 2;
    if (out_snes_y) *out_snes_y = (wy - cam_y) * 2;
}

bool fm_input_menu_hit_test(const FmMenuLayout *layout,
                            int snes_x, int snes_y,
                            int *out_index) {
    if (!layout || layout->rows <= 0 || layout->cols <= 0 ||
        layout->row_w <= 0 || layout->row_h <= 0) {
        return false;
    }

    int stride = (layout->col_stride > 0) ? layout->col_stride : layout->row_w;
    int rel_y = snes_y - layout->y;
    if (rel_y < 0) return false;
    int row = rel_y / layout->row_h;
    if (row >= layout->rows) return false;

    int rel_x = snes_x - layout->x;
    if (rel_x < 0) return false;
    int col = rel_x / stride;
    if (col >= layout->cols) return false;

    int in_col_x = rel_x - col * stride;
    if (in_col_x >= layout->row_w) return false;

    if (out_index) {
        *out_index = row * layout->cols + col;
    }
    return true;
}

void fm_input_on_mouse_move(FmMouseState *state,
                            int win_x, int win_y,
                            int win_w, int win_h,
                            bool widescreen_16_9) {
    if (!state) return;

    int dx = win_x - state->last_win_x;
    int dy = win_y - state->last_win_y;
    state->last_win_x = win_x;
    state->last_win_y = win_y;

    int sx = 0, sy = 0;
    bool inside = fm_input_window_to_snes(win_x, win_y, win_w, win_h,
                                          widescreen_16_9, &sx, &sy);
    state->inside_snes_view = inside;
    state->snes_x = sx;
    state->snes_y = sy;

    if (dx == 0 && dy == 0) {
        return;
    }

    if (state->right_down || state->middle_down) {
        state->drag_accum_x += (dx < 0 ? -dx : dx);
        state->drag_accum_y += (dy < 0 ? -dy : dy);
        if (state->drag_accum_x + state->drag_accum_y >= FM_DRAG_THRESHOLD_PX) {
            state->dragging_camera = true;
        }
        if (state->dragging_camera) {
            state->drag_delta_x += dx;
            state->drag_delta_y += dy;
        }
    } else if (inside) {
        state->has_pending_motion = true;
    }
}

void fm_input_on_mouse_button(FmMouseState *state,
                              int button,
                              bool down,
                              int win_x, int win_y,
                              int win_w, int win_h,
                              bool widescreen_16_9) {
    if (!state) return;

    int sx = 0, sy = 0;
    bool inside = fm_input_window_to_snes(win_x, win_y, win_w, win_h,
                                          widescreen_16_9, &sx, &sy);
    state->inside_snes_view = inside;
    state->snes_x = sx;
    state->snes_y = sy;
    state->last_win_x = win_x;
    state->last_win_y = win_y;

    if (button == 1) { /* Left */
        if (down && !state->left_down && inside) {
            state->left_pressed = true;
            state->has_pending_motion = true;
        }
        state->left_down = down;
    } else if (button == 2) { /* Middle */
        if (down && !state->middle_down) {
            state->drag_accum_x = 0;
            state->drag_accum_y = 0;
            state->dragging_camera = false;
        }
        state->middle_down = down;
        if (!down && !state->right_down) {
            state->dragging_camera = false;
        }
    } else if (button == 3) { /* Right */
        if (down && !state->right_down) {
            state->drag_accum_x = 0;
            state->drag_accum_y = 0;
            state->dragging_camera = false;
        } else if (!down && state->right_down) {
            if (!state->dragging_camera && inside) {
                state->right_pressed = true;
            }
            if (!state->middle_down) {
                state->dragging_camera = false;
            }
        }
        state->right_down = down;
    }
}

bool fm_input_apply_pre_frame(FmMouseState *state,
                              FmInputMode mode,
                              const FmMenuLayout *menu_layout,
                              uint8_t *wram,
                              uint16_t *joypad_mask) {
    if (!state || !wram) return false;

    /* Pillar 3: Instant Zero-Invasion Yield when mouse is stationary */
    bool has_camera_pan = (state->drag_delta_x != 0 || state->drag_delta_y != 0);
    if (!state->has_pending_motion && !state->left_pressed &&
        !state->right_pressed && !has_camera_pan) {
        return false;
    }

    bool modified = false;

    if (mode == FM_INPUT_MODE_TACTICAL) {
        if (has_camera_pan) {
            int pan_x = -(state->drag_delta_x / 2);
            int pan_y = -(state->drag_delta_y / 2);
            if (pan_x != 0 || pan_y != 0) {
                uint16_t cur_x = fm_wram_rd16(wram, FM_WRAM_BG1_SCROLL_X);
                uint16_t cur_y = fm_wram_rd16(wram, FM_WRAM_BG1_SCROLL_Y);
                uint16_t nx = (uint16_t)((int16_t)cur_x + pan_x);
                uint16_t ny = (uint16_t)((int16_t)cur_y + pan_y);

                fm_wram_wr16(wram, FM_WRAM_BG1_SCROLL_X, nx);
                fm_wram_wr16(wram, FM_WRAM_BG1_SCROLL_Y, ny);
                fm_wram_wr16(wram, FM_WRAM_BG2_SCROLL_X, nx);
                fm_wram_wr16(wram, FM_WRAM_BG2_SCROLL_Y, ny);
                fm_wram_wr16(wram, FM_WRAM_BG3_SCROLL_X, nx);
                fm_wram_wr16(wram, FM_WRAM_BG3_SCROLL_Y, ny);
                fm_wram_wr16(wram, FM_WRAM_CAMERA_TARGET_X, nx);
                fm_wram_wr16(wram, FM_WRAM_CAMERA_TARGET_Y, ny);

                for (uint32_t i = 0; i < 6; i += 2) {
                    fm_wram_wr16(wram, FM_WRAM_BG_SCROLL_SHADOW + i * 2u, nx);
                    fm_wram_wr16(wram, FM_WRAM_BG_SCROLL_SHADOW + (i + 1u) * 2u, ny);
                }
                modified = true;
            }
            state->drag_delta_x = 0;
            state->drag_delta_y = 0;
        }

        if (state->has_pending_motion && state->inside_snes_view) {
            uint16_t sx = (uint16_t)(state->snes_x >> 1);
            uint16_t sy = (uint16_t)(state->snes_y >> 1);
            fm_wram_wr16(wram, FM_WRAM_CURSOR_SCREEN_X, sx);
            fm_wram_wr16(wram, FM_WRAM_CURSOR_SCREEN_Y, sy);
            modified = true;
        }
    } else if (mode == FM_INPUT_MODE_MENU) {
        if (state->has_pending_motion && state->inside_snes_view && menu_layout) {
            int idx = 0;
            if (fm_input_menu_hit_test(menu_layout, state->snes_x, state->snes_y, &idx)) {
                wram[FM_WRAM_MENU_CURSOR_IDX] = (uint8_t)idx;
                wram[FM_WRAM_MENU_CURSOR_MIR] = (uint8_t)idx;
                modified = true;
            }
        }
    } else if (mode == FM_INPUT_MODE_WORLDMAP) {
        if (state->has_pending_motion && state->inside_snes_view) {
            uint16_t sx = (uint16_t)(state->snes_x >> 1);
            uint16_t sy = (uint16_t)(state->snes_y >> 1);
            fm_wram_wr16(wram, FM_WRAM_CURSOR_SCREEN_X, sx);
            fm_wram_wr16(wram, FM_WRAM_CURSOR_SCREEN_Y, sy);
            modified = true;
        }
    }

    if (joypad_mask) {
        if (state->left_pressed) {
            *joypad_mask |= FM_BTN_A;
            modified = true;
        }
        if (state->right_pressed) {
            *joypad_mask |= FM_BTN_B;
            modified = true;
        }
    }

    state->has_pending_motion = false;
    state->left_pressed = false;
    state->right_pressed = false;
    state->drag_delta_x = 0;
    state->drag_delta_y = 0;

    return modified;
}
