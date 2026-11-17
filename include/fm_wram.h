#ifndef FM_WRAM_H
#define FM_WRAM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FM_WRAM_SIZE 0x20000u

/* Verified WRAM offsets (Bank $7E) */
#define FM_WRAM_CURSOR_SCREEN_X   0x02A0u /* 16-bit SNES screen X (256x224) */
#define FM_WRAM_CURSOR_SCREEN_Y   0x02A2u /* 16-bit SNES screen Y (256x224) */
#define FM_WRAM_CAMERA_TARGET_X   0x02AEu /* 16-bit tactical camera target X */
#define FM_WRAM_CAMERA_TARGET_Y   0x02B0u /* 16-bit tactical camera target Y */
#define FM_WRAM_MENU_CURSOR_IDX   0x0334u /* 8-bit menu cursor slot index */
#define FM_WRAM_MENU_CURSOR_MIR   0x0366u /* 8-bit menu cursor mirror */

#define FM_WRAM_BG1_SCROLL_X      0x0D72u /* 16-bit BG1 H-scroll mirror */
#define FM_WRAM_BG1_SCROLL_Y      0x0D74u /* 16-bit BG1 V-scroll mirror */
#define FM_WRAM_BG2_SCROLL_X      0x0D76u /* 16-bit BG2 H-scroll mirror */
#define FM_WRAM_BG2_SCROLL_Y      0x0D78u /* 16-bit BG2 V-scroll mirror */
#define FM_WRAM_BG3_SCROLL_X      0x0D7Au /* 16-bit BG3 H-scroll mirror */
#define FM_WRAM_BG3_SCROLL_Y      0x0D7Cu /* 16-bit BG3 V-scroll mirror */
#define FM_WRAM_BG_SCROLL_SHADOW  0x0D8Au /* 6x uint16_t shadow scroll mirrors */

#define FM_WRAM_P1_HELD           0x0DC8u /* 16-bit P1 held buttons */
#define FM_WRAM_P2_HELD           0x0DCAu /* 16-bit P2 held buttons */
#define FM_WRAM_P1_PRESSED        0x0DCCu /* 16-bit P1 newly pressed edge */
#define FM_WRAM_P2_PRESSED        0x0DCEu /* 16-bit P2 newly pressed edge */
#define FM_WRAM_P1_RELEASED       0x0DD0u /* 16-bit P1 newly released edge */
#define FM_WRAM_P2_RELEASED       0x0DD2u /* 16-bit P2 newly released edge */

#define FM_WRAM_INVENTORY_START   0xCF80u /* $7E:CF80-$7E:D1FF inventory */
#define FM_WRAM_INVENTORY_SIZE    0x0280u
#define FM_WRAM_ACTIVE_UNIT_0     0xD580u /* $7E:D580 active unit 0 (0x8D bytes) */
#define FM_WRAM_ACTIVE_UNIT_1     0xD60Du /* $7E:D60D active unit 1 (0x8D bytes) */
#define FM_WRAM_UNIT_STRIDE       0x008Du

typedef struct FmWramUnitRaw {
    uint8_t bytes[FM_WRAM_UNIT_STRIDE];
} FmWramUnitRaw;

/*
 * Non-breaking C union overlay over the 128 KB SNES WRAM buffer.
 * All multi-byte fields are 2-byte aligned except active_unit_1 ($7E:D60D),
 * which uses byte-backed FmWramUnitRaw to avoid unaligned access UB.
 */
typedef union FmWramOverlay {
    uint8_t raw[FM_WRAM_SIZE];
    struct {
        uint8_t _pad_0000[0x02A0];
        uint16_t cursor_screen_x;      /* 0x02A0 */
        uint16_t cursor_screen_y;      /* 0x02A2 */
        uint8_t _pad_02A4[0x000A];
        uint16_t camera_target_x;      /* 0x02AE */
        uint16_t camera_target_y;      /* 0x02B0 */
        uint8_t _pad_02B2[0x0082];
        uint8_t menu_cursor_idx;       /* 0x0334 */
        uint8_t _pad_0335[0x0031];
        uint8_t menu_cursor_mirror;    /* 0x0366 */
        uint8_t _pad_0367[0x0A0B];
        uint16_t bg1_scroll_x;         /* 0x0D72 */
        uint16_t bg1_scroll_y;         /* 0x0D74 */
        uint16_t bg2_scroll_x;         /* 0x0D76 */
        uint16_t bg2_scroll_y;         /* 0x0D78 */
        uint16_t bg3_scroll_x;         /* 0x0D7A */
        uint16_t bg3_scroll_y;         /* 0x0D7C */
        uint8_t _pad_0D7E[0x000C];
        uint16_t bg_scroll_shadow[6];  /* 0x0D8A..0x0D95 */
        uint8_t _pad_0D96[0x0032];
        uint16_t p1_held;              /* 0x0DC8 */
        uint16_t p2_held;              /* 0x0DCA */
        uint16_t p1_pressed;           /* 0x0DCC */
        uint16_t p2_pressed;           /* 0x0DCE */
        uint16_t p1_released;          /* 0x0DD0 */
        uint16_t p2_released;          /* 0x0DD2 */
        uint8_t _pad_0DD4[0xC1AC];
        uint8_t inventory_raw[FM_WRAM_INVENTORY_SIZE]; /* 0xCF80..0xD1FF */
        uint8_t _pad_D200[0x0380];
        FmWramUnitRaw active_unit_0;   /* 0xD580 */
        FmWramUnitRaw active_unit_1;   /* 0xD60D */
        uint8_t _pad_tail[FM_WRAM_SIZE - 0xD69A];
    } mapped;
} FmWramOverlay;

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(FmWramOverlay) == FM_WRAM_SIZE, "FmWramOverlay must be exactly 128 KB");
_Static_assert(offsetof(FmWramOverlay, mapped.cursor_screen_x) == FM_WRAM_CURSOR_SCREEN_X, "cursor_screen_x offset");
_Static_assert(offsetof(FmWramOverlay, mapped.cursor_screen_y) == FM_WRAM_CURSOR_SCREEN_Y, "cursor_screen_y offset");
_Static_assert(offsetof(FmWramOverlay, mapped.camera_target_x) == FM_WRAM_CAMERA_TARGET_X, "camera_target_x offset");
_Static_assert(offsetof(FmWramOverlay, mapped.camera_target_y) == FM_WRAM_CAMERA_TARGET_Y, "camera_target_y offset");
_Static_assert(offsetof(FmWramOverlay, mapped.menu_cursor_idx) == FM_WRAM_MENU_CURSOR_IDX, "menu_cursor_idx offset");
_Static_assert(offsetof(FmWramOverlay, mapped.menu_cursor_mirror) == FM_WRAM_MENU_CURSOR_MIR, "menu_cursor_mirror offset");
_Static_assert(offsetof(FmWramOverlay, mapped.bg1_scroll_x) == FM_WRAM_BG1_SCROLL_X, "bg1_scroll_x offset");
_Static_assert(offsetof(FmWramOverlay, mapped.bg_scroll_shadow) == FM_WRAM_BG_SCROLL_SHADOW, "bg_scroll_shadow offset");
_Static_assert(offsetof(FmWramOverlay, mapped.p1_held) == FM_WRAM_P1_HELD, "p1_held offset");
_Static_assert(offsetof(FmWramOverlay, mapped.p1_pressed) == FM_WRAM_P1_PRESSED, "p1_pressed offset");
_Static_assert(offsetof(FmWramOverlay, mapped.p1_released) == FM_WRAM_P1_RELEASED, "p1_released offset");
_Static_assert(offsetof(FmWramOverlay, mapped.inventory_raw) == FM_WRAM_INVENTORY_START, "inventory_raw offset");
_Static_assert(offsetof(FmWramOverlay, mapped.active_unit_0) == FM_WRAM_ACTIVE_UNIT_0, "active_unit_0 offset");
_Static_assert(offsetof(FmWramOverlay, mapped.active_unit_1) == FM_WRAM_ACTIVE_UNIT_1, "active_unit_1 offset");
#endif

static inline uint16_t fm_wram_rd16(const uint8_t *wram, uint32_t addr) {
    uint32_t a = addr & (FM_WRAM_SIZE - 1u);
    return (uint16_t)(wram[a] | ((uint16_t)wram[(a + 1u) & (FM_WRAM_SIZE - 1u)] << 8));
}

static inline void fm_wram_wr16(uint8_t *wram, uint32_t addr, uint16_t val) {
    uint32_t a = addr & (FM_WRAM_SIZE - 1u);
    wram[a] = (uint8_t)(val & 0xFFu);
    wram[(a + 1u) & (FM_WRAM_SIZE - 1u)] = (uint8_t)((val >> 8) & 0xFFu);
}

#ifdef __cplusplus
}
#endif

#endif /* FM_WRAM_H */
