#ifndef FM_INPUT_H
#define FM_INPUT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FM_SNES_PPU_W      512
#define FM_SNES_PPU_H      448
#define FM_SNES_NATIVE_W   256
#define FM_SNES_NATIVE_H   224
#define FM_WIDESCREEN_W    800
#define FM_WIDESCREEN_WING 144

typedef enum FmInputMode {
    FM_INPUT_MODE_NONE = 0,     /* Title / cutscene / idle (no direct WRAM writes) */
    FM_INPUT_MODE_TACTICAL = 1, /* Pillar 1: Isometric tactical grid + camera pan */
    FM_INPUT_MODE_MENU = 2,     /* Pillar 2: Menus, Hangar, Shop, Coliseum */
    FM_INPUT_MODE_WORLDMAP = 3  /* Pillar 3: World map cursor + click confirm */
} FmInputMode;

typedef struct FmMenuLayout {
    int x;          /* Top-left X in 512x448 PPU space */
    int y;          /* Top-left Y in 512x448 PPU space */
    int row_w;      /* Width of each cell in 512x448 PPU space */
    int row_h;      /* Height of each row in 512x448 PPU space */
    int rows;       /* Number of rows */
    int cols;       /* Number of columns (>= 1) */
    int col_stride; /* Horizontal stride between columns (0 => row_w) */
} FmMenuLayout;

typedef struct FmMouseState {
    int last_win_x;
    int last_win_y;
    int snes_x;              /* Clamped [0..511] in 512x448 PPU space */
    int snes_y;              /* Clamped [0..447] in 512x448 PPU space */
    bool inside_snes_view;   /* True if last pointer was inside 512x448 viewport */
    bool has_pending_motion; /* True only on frames with actual pointer movement */
    bool left_down;
    bool left_pressed;       /* Rising edge this frame */
    bool right_down;
    bool right_pressed;      /* Rising edge this frame (without drag) */
    bool middle_down;
    bool dragging_camera;    /* True while Right/Middle drag exceeds threshold */
    int drag_accum_x;
    int drag_accum_y;
    int drag_delta_x;        /* Pending camera pan X in 512x448 PPU pixels */
    int drag_delta_y;        /* Pending camera pan Y in 512x448 PPU pixels */
} FmMouseState;

void fm_input_init(FmMouseState *state);

/*
 * Aspect-correct window-to-SNES (512x448) coordinate conversion.
 * Returns true if (win_x, win_y) falls inside the active 512x448 SNES viewport.
 */
bool fm_input_window_to_snes(int win_x, int win_y,
                             int win_w, int win_h,
                             bool widescreen_16_9,
                             int *out_snes_x, int *out_snes_y);

/*
 * Pillar 1: Isometric 32x16 (native 16x8 half-tile) screen-to-tile projection.
 * Converts 512x448 PPU coordinates + 256x224 camera scroll to tile (gx, gy).
 */
void fm_input_screen_to_iso_tile(int snes_x, int snes_y,
                                 int cam_x, int cam_y,
                                 int *out_gx, int *out_gy);

void fm_input_iso_tile_to_screen(int gx, int gy,
                                 int cam_x, int cam_y,
                                 int *out_snes_x, int *out_snes_y);

/*
 * Pillar 2: Deterministic 2D bounding-box hit-testing for menus/lists.
 * Returns true and sets *out_index (row * cols + col) if inside a slot.
 */
bool fm_input_menu_hit_test(const FmMenuLayout *layout,
                            int snes_x, int snes_y,
                            int *out_index);

/*
 * Feed host pointer events into FmMouseState.
 */
void fm_input_on_mouse_move(FmMouseState *state,
                            int win_x, int win_y,
                            int win_w, int win_h,
                            bool widescreen_16_9);

void fm_input_on_mouse_button(FmMouseState *state,
                              int button, /* 1=Left, 2=Middle, 3=Right */
                              bool down,
                              int win_x, int win_y,
                              int win_w, int win_h,
                              bool widescreen_16_9);

/*
 * Direct-State Pre-Frame Application:
 * - Zero-Invasion Yield: if mouse is stationary and no click/drag occurred,
 *   wram is untouched (0 bytes modified) and *joypad_mask is unchanged.
 * - Otherwise writes directly to WRAM cursor/camera/menu offsets and injects
 *   A/B edges into *joypad_mask.
 * Returns true if any WRAM or joypad state was modified.
 */
bool fm_input_apply_pre_frame(FmMouseState *state,
                              FmInputMode mode,
                              const FmMenuLayout *menu_layout,
                              uint8_t *wram,
                              uint16_t *joypad_mask);

#ifdef __cplusplus
}
#endif

#endif /* FM_INPUT_H */
