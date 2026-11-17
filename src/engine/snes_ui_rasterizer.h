#ifndef FM_ENGINE_SNES_UI_RASTERIZER_H
#define FM_ENGINE_SNES_UI_RASTERIZER_H

#include <stdbool.h>
#include <stdint.h>
#include "../../include/fm_snes_ui_theme.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FmSnesCanvas {
    uint32_t *pixels;
    int width;
    int height;
    int stride;
} FmSnesCanvas;

/* Initialize a target RGBA software canvas */
void fm_snes_canvas_init(FmSnesCanvas *canvas, uint32_t *pixels, int width, int height);

/* Map semantic palette enum to 32-bit BGR555 RGBA color */
uint32_t fm_snes_palette_color(FmSnesPaletteId pal);

/* Fill solid rectangle clipped to canvas bounds */
void draw_snes_rect_fill(FmSnesCanvas *canvas, int x, int y, int w, int h, uint32_t rgba);

/* Draw 1px rectangle outline */
void draw_snes_rect_outline(FmSnesCanvas *canvas, int x, int y, int w, int h, uint32_t rgba);

/* Fill window interior with authentic Quest Royal Blue BGR555 vertical gradient + scanline dither */
void draw_snes_window_fill(FmSnesCanvas *canvas, int x, int y, int w, int h);

/* Draw 8x8 tile-snapped ornate 9-slice gold border around (x, y, w, h) */
void draw_snes_9slice_box(FmSnesCanvas *canvas, int x, int y, int w, int h, bool fill_backdrop);

/* Draw an embossed SNES section header plate inside a 9-slice box */
void draw_snes_header_plate(
    FmSnesCanvas *canvas,
    int x,
    int y,
    int w,
    const char *title,
    FmSnesPaletteId pal
);

/* Render authentic 8x8 SNES pixel font with 1px (#080810) drop shadow (8px char stride) */
void draw_snes_tile_text(
    FmSnesCanvas *canvas,
    int x,
    int y,
    const char *text,
    FmSnesPaletteId pal
);

/* Render compact 6x8 shadowed pixel font for dense 144px-wide wing telemetry rows (6px char stride) */
void draw_snes_compact_text(
    FmSnesCanvas *canvas,
    int x,
    int y,
    const char *text,
    FmSnesPaletteId pal
);

/* Draw a segmented 16-bit RPG gauge bar (8px segment dividers + 3D bevel) */
void draw_snes_gauge_bar(
    FmSnesCanvas *canvas,
    int x,
    int y,
    int w,
    int h,
    int value,
    int max_value,
    uint32_t fill_rgba,
    uint32_t back_rgba
);

/* Draw an 8x8 2BPP/4BPP status icon with 1px drop shadow */
void draw_snes_icon(
    FmSnesCanvas *canvas,
    int x,
    int y,
    FmSnesIconId icon,
    FmSnesPaletteId pal
);

/* Draw a 16x24 miniature Major Arcana Tarot card with Roman header & optional overlevel seal */
void draw_snes_mini_tarot_card(
    FmSnesCanvas *canvas,
    int x,
    int y,
    uint8_t card_id,
    bool overlevel_hazard
);

/* Draw a faceted Zodiac Stone socket (28x14 px) with 3-letter constellation tag */
void draw_snes_zodiac_gem(
    FmSnesCanvas *canvas,
    int x,
    int y,
    uint8_t stone_index,
    bool collected
);

/* Draw Zenobian Stone & Parchment tapestry pillar for Vanilla 16:9 fixed-width menu screens */
void draw_snes_tapestry_pillar(FmSnesCanvas *canvas, int x, int y, int w, int h);

/* Export RGBA canvas to binary PPM (P6) file for closed-loop visual verification */
bool fm_snes_ui_save_ppm(const FmSnesCanvas *canvas, const char *filepath);

#ifdef __cplusplus
}
#endif

#endif /* FM_ENGINE_SNES_UI_RASTERIZER_H */
